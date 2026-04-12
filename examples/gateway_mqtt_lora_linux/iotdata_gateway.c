// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * iotdata_gateway.c - E22-900T22U to MQTT gateway
 *
 * Receives iotdata binary frames from E22-900T22U radio, decodes to JSON,
 * and publishes to MQTT topic: <prefix>/<variant_name>/<station_id>
 *
 * Variant definitions are compiled in from the common headers.
 * No routing configuration needed — the variant byte in the iotdata header
 * determines the topic automatically.
 *
 * Mesh support (variant 15):
 *   - FORWARD packets are unwrapped and the inner sensor data processed
 *     as if received directly.
 *   - Duplicate suppression via {station_id, sequence} ring buffer.
 *   - Beacon origination on a configurable interval (when mesh-enable=true).
 *   - ACK transmission to FORWARD senders (stub, ready for implementation).
 *   - All mesh control packets are logged for diagnostics.
 *
 * Dedup support:
 *   - allow incoming, and establish outgoing, UDP streams to specified
 *     other gateways to synchronise station_id/sequence pairs for edge
 *     de-duplication before publishing to MQTT. Operates indepemdently of
 *     Mesh protocol.
 *
 * Uses EBYTE E22 connector for low-level, but is sufficiently modular to
 * fit onto another driver.
 * https://github.com/matthewgream/e22900t22
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

volatile bool running = true;

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

time_t intervalable(const time_t interval, time_t *last) {
    time_t now = time(NULL);
    if (*last == 0) {
        *last = now;
        return 0;
    }
    if ((now - *last) > interval) {
        const time_t diff = now - *last;
        *last = now;
        return diff;
    }
    return 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void debug_hexdump(const char *prefix, const uint8_t *data, size_t length) {
    for (size_t offset = 0; offset < length; offset += 16) {
        printf("%s[%04X] ", prefix ? prefix : "", (unsigned)offset);
        for (size_t i = 0; i < 16; i++) {
            if (i == 8)
                printf(" ");
            if (offset + i < length)
                printf("%02X ", data[offset + i]);
            else
                printf("   ");
        }
        printf(" ");
        for (size_t i = 0; i < 16; i++) {
            if (i == 8)
                printf(" ");
            if (offset + i < length)
                printf("%c", isprint(data[offset + i]) ? data[offset + i] : '.');
            else
                printf(" ");
        }
        printf("\n");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "serial_linux.h"

bool debug_e22 = false;
void printf_debug_e22(const char *format, ...) {
    if (!debug_e22)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void printf_stdout_e22(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void printf_stderr_e22(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
#define PRINTF_DEBUG printf_debug_e22
#define PRINTF_ERROR printf_stderr_e22
#define PRINTF_INFO  printf_stdout_e22
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "e22xxxtxx.h"
void __sleep_ms(const uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

// bool device_mode_config(void);
//   bool device_info_read(void);
//   bool device_config_read_and_update(void);
// bool device_mode_transfer(void);
// bool device_connect(const e22900t22_module_t config_module, const e22900t22_config_t *config_device);
//   bool device_channel_rssi_read(uint8_t *rssi);
//   bool device_packet_read(uint8_t *packet, const int max_size, int *packet_size, uint8_t *rssi);
//   bool device_packet_write(const uint8_t *packet, const int length);
// void device_disconnect(void);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS     0
#define MQTT_PUBLISH_RETAIN  false
#include "mqtt_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "iotdata_variant_suite.h"
#include "iotdata.c"
#include "iotdata_mesh.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT              "iotdata_gateway.cfg"

#define SERIAL_PORT_DEFAULT              "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT              9600
#define SERIAL_BITS_DEFAULT              SERIAL_8N1

#define MQTT_CLIENT_DEFAULT              "iotdata_gateway"
#define MQTT_SERVER_DEFAULT              "mqtt://localhost"
#define MQTT_TLS_DEFAULT                 false
#define MQTT_SYNCHRONOUS_DEFAULT         false
#define MQTT_TOPIC_PREFIX_DEFAULT        "iotdata"
#define MQTT_RECONNECT_DELAY_DEFAULT     5
#define MQTT_RECONNECT_DELAY_MAX_DEFAULT 60
#define MQTT_TOPIC_STR_MAX               255

#define STATS_INTERVAL_DEFAULT           (5 * 60)
#define INTERVAL_RSSI_DEFAULT            (1 * 60)
#define INTERVAL_BEACON_DEFAULT          60 /* seconds */

#define GATEWAY_STATION_ID_DEFAULT       1

#include "config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"help",                        no_argument,       0, 'h'},
    {"config",                      required_argument, 0, 0},
    {"port",                        required_argument, 0, 0},
    {"rate",                        required_argument, 0, 0},
    {"bits",                        required_argument, 0, 0},
    {"address",                     required_argument, 0, 0},
    {"network",                     required_argument, 0, 0},
    {"channel",                     required_argument, 0, 0},
    {"packet-size",                 required_argument, 0, 0},
    {"packet-rate",                 required_argument, 0, 0},
    {"rssi-channel",                required_argument, 0, 0},
    {"rssi-packet",                 required_argument, 0, 0},
    {"listen-before-transmit",      required_argument, 0, 0},
    {"read-timeout-command",        required_argument, 0, 0},
    {"read-timeout-packet",         required_argument, 0, 0},
    {"interval-rssi",               required_argument, 0, 0},
    {"debug-e22",                   required_argument, 0, 0},
    {"mqtt-client",                 required_argument, 0, 0},
    {"mqtt-server",                 required_argument, 0, 0},
    {"mqtt-topic-prefix",           required_argument, 0, 0},
    {"mqtt-tls-insecure",           required_argument, 0, 0},
    {"mqtt-reconnect-delay",        required_argument, 0, 0},
    {"mqtt-reconnect-delay-max",    required_argument, 0, 0},
    {"mesh-enable",                 required_argument, 0, 0},
    {"mesh-station-id",             required_argument, 0, 0},
    {"mesh-beacon-interval",        required_argument, 0, 0},
    {"debug-mesh",                  required_argument, 0, 0},
    {"dedup-enable",                required_argument, 0, 0},
    {"dedup-port",                  required_argument, 0, 0},
    {"dedup-peers",                 required_argument, 0, 0},
    {"dedup-delay",                 required_argument, 0, 0},
    {"debug-dedup",                 required_argument, 0, 0},
    {"stats-display-interval",      required_argument, 0, 0},
    {"stats-publish-interval",      required_argument, 0, 0},
    {"stats-mqtt-topic",            required_argument, 0, 0},
    {"debug",                       required_argument, 0, 0},
    {"debug-data",                  required_argument, 0, 0},
    {0, 0, 0, 0}
};

const config_option_help_t config_options_help [] = {
    {"help",                   "Display this help message and exit"},
    {"config",                 "Configuration file path (default: " CONFIG_FILE_DEFAULT ")"},
    {"port",                   "Serial port device (default: " SERIAL_PORT_DEFAULT ")"},
    {"rate",                   "Serial baud rate (default: 9600)"},
    {"bits",                   "Serial data bits (default: 8N1)"},
    {"address",                "E22 module address (hex)"},
    {"network",                "E22 network ID (hex)"},
    {"channel",                "E22 radio channel"},
    {"packet-size",            "E22 packet size"},
    {"packet-rate",            "E22 packet rate"},
    {"rssi-channel",           "Enable channel RSSI capture (true/false)"},
    {"rssi-packet",            "Enable packet RSSI capture (true/false)"},
    {"listen-before-transmit", "Enable listen-before-transmit mode (true/false)"},
    {"read-timeout-command",   "E22 command read timeout in ms"},
    {"read-timeout-packet",    "E22 packet read timeout in ms"},
    {"interval-rssi",          "RSSI polling interval in seconds"},
    {"debug-e22",              "Enable E22 module debug output (true/false)"},
    {"mqtt-client",            "MQTT client ID (default: " MQTT_CLIENT_DEFAULT ")"},
    {"mqtt-server",            "MQTT server URL (default: " MQTT_SERVER_DEFAULT ")"},
    {"mqtt-topic-prefix",      "MQTT topic prefix (default: " MQTT_TOPIC_PREFIX_DEFAULT ")"},
    {"mqtt-tls-insecure",      "Disable MQTT TLS verification (true/false)"},
    {"mqtt-reconnect-delay",   "MQTT reconnect delay in seconds"},
    {"mqtt-reconnect-delay-max","MQTT max reconnect delay in seconds"},
    {"mesh-enable",            "Enable mesh network support (true/false)"},
    {"mesh-station-id",        "This gateway's mesh station ID"},
    {"mesh-beacon-interval",   "Mesh beacon transmission interval in seconds"},
    {"debug-mesh",             "Enable mesh debug output (true/false)"},
    {"dedup-enable",           "Enable cross-gateway deduplication (true/false)"},
    {"dedup-port",             "UDP port for dedup peer communication"},
    {"dedup-peers",            "Comma-separated list of dedup peers (host:port)"},
    {"dedup-delay",            "Dedup batch send delay in ms"},
    {"debug-dedup",            "Enable dedup debug output (true/false)"},
    {"stats-display-interval", "Stats stdout display interval in seconds (0 disables)"},
    {"stats-publish-interval", "Stats MQTT publish interval in seconds (0 disables)"},
    {"stats-mqtt-topic",       "MQTT stats topic prefix (default: iotdata/stats)"},
    {"debug",                  "Enable general debug output (true/false)"},
    {"crypt",                  "E22 encryption key (default: 0x0000)"},
    {"transmit-power",         "E22 transmit power level 0-3 (default: 0)"},
    {"transmission-method",    "E22 transmission method 0=transparent, 1=fixed-point (default: 0)"},
    {"debug-data",             "Enable hex dump of received packet data (true/false)"},
};
// clang-format on

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void serial_config_populate(serial_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->port = config_get_string("port", SERIAL_PORT_DEFAULT);
    cfg->rate = config_get_integer("rate", SERIAL_RATE_DEFAULT);
    cfg->bits = config_get_bits("bits", SERIAL_BITS_DEFAULT);

    printf("config: serial: port=%s, rate=%d, bits=%s\n", cfg->port, cfg->rate, serial_bits_str(cfg->bits));
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void e22_config_populate(e22900t22_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->address = (uint16_t)config_get_integer("address", E22900T22_CONFIG_ADDRESS_DEFAULT);
    cfg->network = (uint8_t)config_get_integer("network", E22900T22_CONFIG_NETWORK_DEFAULT);
    cfg->channel = (uint8_t)config_get_integer("channel", E22900T22_CONFIG_CHANNEL_DEFAULT);
    cfg->crypt = (uint16_t)config_get_integer("crypt", E22900T22_CONFIG_CRYPT_DEFAULT);
    cfg->packet_size = (uint8_t)config_get_integer("packet-size", E22900T22_CONFIG_PACKET_SIZE_DEFAULT);          // index
    cfg->packet_rate = (uint8_t)config_get_integer("packet-rate", E22900T22_CONFIG_PACKET_RATE_DEFAULT);          // index
    cfg->transmit_power = (uint8_t)config_get_integer("transmit-power", E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT); // index
    cfg->transmission_method = (uint8_t)config_get_integer("transmission-method", E22900T22_CONFIG_TRANSMISSION_METHOD_DEFAULT);
    cfg->relay_enabled = E22900T22_CONFIG_RELAY_ENABLED_DEFAULT;
    cfg->listen_before_transmit = config_get_bool("listen-before-transmit", E22900T22_CONFIG_LISTEN_BEFORE_TRANSMIT);
    cfg->rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    cfg->rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    cfg->read_timeout_command = (uint32_t)config_get_integer("read-timeout-command", E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    cfg->read_timeout_packet = (uint32_t)config_get_integer("read-timeout-packet", E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    cfg->debug = config_get_bool("debug-e22", false);
    debug_e22 = cfg->debug;

    printf("config: e22: address=0x%04" PRIX16 ", network=0x%02" PRIX8 ", channel=%d, packet-size=%d, packet-rate=%d, rssi-channel=%s, rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%" PRIu32 ", read-timeout-packet=%" PRIu32
           ", crypt=0x%04" PRIX16 ", transmit-power=%" PRIu8 ", transmission-method=%s, mode-relay=%s, debug=%s\n",
           cfg->address, cfg->network, cfg->channel, cfg->packet_size, cfg->packet_rate, cfg->rssi_channel ? "on" : "off", cfg->rssi_packet ? "on" : "off", cfg->listen_before_transmit ? "on" : "off", cfg->read_timeout_command,
           cfg->read_timeout_packet, cfg->crypt, cfg->transmit_power, cfg->transmission_method == E22900T22_CONFIG_TRANSMISSION_METHOD_TRANSPARENT ? "transparent" : "fixed-point", cfg->relay_enabled ? "on" : "off",
           cfg->debug ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mqtt_config_populate(mqtt_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    cfg->server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    cfg->tls_insecure = config_get_bool("mqtt-tls-insecure", MQTT_TLS_DEFAULT);
    cfg->use_synchronous = MQTT_SYNCHRONOUS_DEFAULT;
    cfg->reconnect_delay = (unsigned int)config_get_integer("mqtt-reconnect-delay", MQTT_RECONNECT_DELAY_DEFAULT);
    cfg->reconnect_delay_max = (unsigned int)config_get_integer("mqtt-reconnect-delay-max", MQTT_RECONNECT_DELAY_MAX_DEFAULT);

    printf("config: mqtt: client=%s, server=%s, tls-insecure=%s, synchronous=%s, reconnect-delay=%d, reconnect-delay-max=%d\n", cfg->client, cfg->server, cfg->tls_insecure ? "on" : "off", cfg->use_synchronous ? "on" : "off",
           cfg->reconnect_delay, cfg->reconnect_delay_max);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "iotdata_gateway_mesh.h"
#include "iotdata_gateway_dedup.h"
#include "iotdata_gateway_stats.h"

void dedup_config_populate(dedup_state_t *state) {
    memset(state, 0, sizeof(*state));

    state->enabled = config_get_bool("dedup-enable", false);
    state->port = (uint16_t)config_get_integer("dedup-port", DEDUP_PORT_DEFAULT);
    state->delay_ms = (uint32_t)config_get_integer("dedup-delay", DEDUP_DELAY_MS_DEFAULT);
    const char *peers = config_get_string("dedup-peers", "");
    dedup_peers_parse(state, peers);
    state->debug = config_get_bool("debug-dedup", false);

    printf("config: dedup: enabled=%c, port=%" PRIu16 ", peers=%s, delay=%" PRIu32 "ms, debug=%s\n", state->enabled ? 'y' : 'n', state->port, peers, state->delay_ms, state->debug ? "on" : "off");
}

void mesh_config_populate(mesh_state_t *state) {
    memset(state, 0, sizeof(*state));

    state->enabled = config_get_bool("mesh-enable", false);
    state->station_id = (uint16_t)config_get_integer("mesh-station-id", GATEWAY_STATION_ID_DEFAULT);
    state->beacon_interval = (time_t)config_get_integer("mesh-beacon-interval", INTERVAL_BEACON_DEFAULT);
    state->debug = config_get_bool("debug-mesh", false);

    printf("config: mesh: enabled=%c, station=0x%04" PRIX16 ", beacon-interval=%" PRIu32 ", debug=%s\n", state->enabled ? 'y' : 'n', state->station_id, (uint32_t)state->beacon_interval, state->debug ? "on" : "off");
}

void stats_config_populate(stats_state_t *state) {
    memset(state, 0, sizeof(*state));

    const char *topic = config_get_string("stats-mqtt-topic", STATS_MQTT_TOPIC_DEFAULT);
    strncpy(state->mqtt_topic, topic, sizeof(state->mqtt_topic) - 1);
    state->mqtt_topic[sizeof(state->mqtt_topic) - 1] = '\0';
    /* strip trailing slash */
    size_t len = strlen(state->mqtt_topic);
    if (len > 0 && state->mqtt_topic[len - 1] == '/')
        state->mqtt_topic[len - 1] = '\0';

    printf("config: stats: mqtt-topic=%s\n", state->mqtt_topic);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

struct {
    const char *mqtt_topic_prefix;
    bool capture_rssi_packet;
    bool capture_rssi_channel;
    time_t stats_display_interval;
    time_t stats_display_interval_last;
    time_t stats_publish_interval;
    time_t stats_publish_interval_last;
    time_t interval_rssi;
    time_t interval_rssi_last;
    mesh_state_t mesh_state;
    dedup_state_t dedup_state;
    stats_state_t stats_state;
    bool debug;
    bool debug_data;
} process_state;

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_and_add_handler(uint16_t station_id, uint16_t sequence) {
    return dedup_check_and_add(&process_state.dedup_state, station_id, sequence);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_sensor_packet(uint16_t station_id, uint16_t sequence) {
    if (process_state.mesh_state.enabled)
        if (!dedup_check_and_add(&process_state.dedup_state, station_id, sequence)) {
            process_state.mesh_state.stat_duplicates++;
            if (process_state.debug || process_state.mesh_state.debug)
                printf("process: mesh direct packet duplicate suppressed (station=0x%04" PRIX16 ", sequence=%" PRIu16 ")\n", station_id, sequence);
            return false;
        }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_sensor_packet(const uint8_t *packet_buffer, int packet_length, uint8_t variant_id, uint16_t station_id, uint16_t sequence, const char *topic_prefix, const char *via) {
    const iotdata_variant_def_t *vdef;
    if ((vdef = iotdata_get_variant(variant_id)) == NULL) {
        fprintf(stderr, "process: unknown variant %" PRIu8 " (station=0x%04" PRIX16 ", size=%d)\n", variant_id, station_id, packet_length);
        stats_on_packet_decode_error(&process_state.stats_state, station_id, variant_id);
        return;
    }
    char *json = NULL;
    iotdata_decode_to_json_scratch_t scratch;
    iotdata_status_t rc;
    if ((rc = iotdata_decode_to_json(packet_buffer, (size_t)packet_length, &json, &scratch)) != IOTDATA_OK) {
        fprintf(stderr, "process: decode failed: %s (variant=%" PRIu8 ", station=0x%04" PRIX16 ", size=%d)\n", iotdata_strerror(rc), variant_id, station_id, packet_length);
        stats_on_packet_decode_error(&process_state.stats_state, station_id, variant_id);
        return;
    }
    stats_on_packet_decoded(&process_state.stats_state, station_id, sequence, variant_id, packet_length, &scratch.dec);
    char topic[MQTT_TOPIC_STR_MAX];
    snprintf(topic, sizeof(topic), "%s/%s/%04" PRIX16, topic_prefix, vdef->name, station_id);
    if (!mqtt_send(topic, json, (int)strlen(json))) {
        fprintf(stderr, "process: mqtt send failed (topic=%s, size=%d)\n", topic, (int)strlen(json));
        stats_on_packet_process_error(&process_state.stats_state, station_id, variant_id);
    }
    if (process_state.debug)
        printf("  -> %s (%d bytes%s%s)\n", topic, (int)strlen(json), via ? " via " : "", via ? via : "");
    free(json);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_mesh_packet(const uint8_t *packet_buffer, int packet_length, uint8_t variant_id, uint16_t station_id, uint16_t sequence, const char *topic_prefix) {
    (void)variant_id;
    const uint8_t ctrl_type = iotdata_mesh_peek_ctrl_type(packet_buffer, packet_length);
    process_state.mesh_state.stat_mesh_ctrl_rx++;
    if (process_state.mesh_state.debug)
        printf("mesh: rx %s from station=0x%04" PRIX16 ", sequence=%" PRIu16 " (%d bytes)\n", iotdata_mesh_ctrl_name(ctrl_type), station_id, sequence, packet_length);
    switch (ctrl_type) {
    case IOTDATA_MESH_CTRL_FORWARD: {
        const uint8_t *inner;
        int inner_len;
        if (mesh_handle_forward(&process_state.mesh_state, packet_buffer, packet_length, &inner, &inner_len)) {
            uint8_t inner_variant;
            uint16_t inner_station, inner_sequence;
            if (iotdata_peek(inner, (size_t)inner_len, &inner_variant, &inner_station, &inner_sequence) != IOTDATA_OK) {
                fprintf(stderr, "mesh: FORWARD inner packet peek failed (len=%d)\n", inner_len);
                stats_on_link_rx_drop(&process_state.stats_state);
            } else if (dedup_check_sensor_packet(inner_station, inner_sequence))
                process_sensor_packet(inner, inner_len, inner_variant, inner_station, inner_sequence, topic_prefix, "mesh");
        }
        break;
    }
    case IOTDATA_MESH_CTRL_BEACON: {
        mesh_handle_beacon(&process_state.mesh_state, packet_buffer, packet_length);
        iotdata_mesh_beacon_t b;
        if (iotdata_mesh_unpack_beacon(packet_buffer, packet_length, &b))
            stats_on_mesh_peer(&process_state.stats_state, b.gateway_id, b.generation, b.cost, b.flags);
        break;
    }
    case IOTDATA_MESH_CTRL_ACK:
        process_state.mesh_state.stat_acks_rx++;
        if (process_state.mesh_state.debug)
            printf("mesh: rx unexpected ACK from station=0x%04" PRIX16 "\n", station_id);
        break;
    case IOTDATA_MESH_CTRL_ROUTE_ERROR:
        mesh_handle_route_error(&process_state.mesh_state, packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_NEIGHBOUR_RPT:
        mesh_handle_neighbour_report(&process_state.mesh_state, packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_PONG:
        mesh_handle_pong(&process_state.mesh_state, packet_buffer, packet_length);
        break;
    default:
        process_state.mesh_state.stat_mesh_unknown++;
        if (process_state.mesh_state.debug)
            printf("mesh: rx unknown ctrl_type=0x%02" PRIX8 " from station=0x%04" PRIX16 "\n", ctrl_type, station_id);
        break;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_config_populate(void) {
    memset(&process_state, 0, sizeof(process_state));
    process_state.mqtt_topic_prefix = config_get_string("mqtt-topic-prefix", MQTT_TOPIC_PREFIX_DEFAULT);
    process_state.capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    process_state.capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    process_state.interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);
    process_state.stats_display_interval = config_get_integer("stats-display-interval", STATS_INTERVAL_DEFAULT);
    process_state.stats_publish_interval = config_get_integer("stats-publish-interval", STATS_INTERVAL_DEFAULT);
    process_state.debug = config_get_bool("debug", false);
    process_state.debug_data = config_get_bool("debug-data", false);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_run(void) {
    uint8_t packet_buffer[E22900T22_PACKET_MAXSIZE + 1]; /* +1 for RSSI byte */
    int packet_length;

    printf("process: iotdata gateway (stats-display=%" PRIu32 "s, stats-publish=%" PRIu32 "s, rssi=%" PRIu32 "s [packets=%c, channel=%c], topic-prefix=%s", (uint32_t)process_state.stats_display_interval,
           (uint32_t)process_state.stats_publish_interval, (uint32_t)process_state.interval_rssi, process_state.capture_rssi_packet ? 'y' : 'n', process_state.capture_rssi_channel ? 'y' : 'n', process_state.mqtt_topic_prefix);
    if (process_state.mesh_state.enabled)
        printf(", mesh=on, beacon=%" PRIu32 "s", (uint32_t)process_state.mesh_state.beacon_interval);
    printf(")\n");

    for (int i = 0; i < IOTDATA_VARIANT_MAPS_COUNT; i++) {
        const iotdata_variant_def_t *vdef = iotdata_get_variant((uint8_t)i);
        printf("process: variant[%d] = \"%s\" (pres_bytes=%" PRIu8 ") -> %s/%s/<station>\n", i, vdef->name, vdef->num_pres_bytes, process_state.mqtt_topic_prefix, vdef->name);
    }
    if (process_state.mesh_state.enabled)
        printf("process: variant[15] = mesh control (gateway station=0x%04" PRIX16 ")\n", process_state.mesh_state.station_id);

    while (running) {

        // packet processing
        uint8_t packet_rssi = 0, channel_rssi = 0;
        if (device_packet_read(packet_buffer, sizeof(packet_buffer), &packet_length, &packet_rssi) && running) {
            stats_on_link_rx_packet(&process_state.stats_state, packet_length);
            if (process_state.debug_data)
                debug_hexdump("data: ", packet_buffer, (size_t)packet_length);
            if (process_state.capture_rssi_packet && packet_rssi > 0)
                stats_on_link_rssi_packet(&process_state.stats_state, packet_rssi);
            uint8_t variant_id;
            uint16_t station_id, sequence;
            if (iotdata_peek(packet_buffer, (size_t)packet_length, &variant_id, &station_id, &sequence) != IOTDATA_OK) {
                fprintf(stderr, "process: packet too short for iotdata header (size=%d)\n", packet_length);
                stats_on_link_rx_drop(&process_state.stats_state);
            } else if (variant_id == IOTDATA_MESH_VARIANT)
                process_mesh_packet(packet_buffer, packet_length, variant_id, station_id, sequence, process_state.mqtt_topic_prefix);
            else
                process_sensor_packet(packet_buffer, packet_length, variant_id, station_id, sequence, process_state.mqtt_topic_prefix, "direct");
        }

        // rssi update
        if (running && process_state.capture_rssi_channel && intervalable(process_state.interval_rssi, &process_state.interval_rssi_last))
            if (device_channel_rssi_read(&channel_rssi) && running)
                stats_on_link_rssi_channel(&process_state.stats_state, channel_rssi);

        // mesh beacons
        if (running && process_state.mesh_state.enabled && intervalable(process_state.mesh_state.beacon_interval, &process_state.mesh_state.beacon_last))
            mesh_beacon_send(&process_state.mesh_state);

        // stats display (stdout)
        time_t period_display;
        if (running && process_state.stats_display_interval > 0 && (period_display = intervalable(process_state.stats_display_interval, &process_state.stats_display_interval_last)) > 0)
            stats_display(period_display, &process_state.stats_state, &process_state.mesh_state, &process_state.dedup_state);

        // stats publish (mqtt)
        if (running && process_state.stats_publish_interval > 0 && intervalable(process_state.stats_publish_interval, &process_state.stats_publish_interval_last) > 0)
            stats_publish(&process_state.stats_state, &process_state.mesh_state, &process_state.dedup_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

serial_config_t serial_config;
e22900t22_config_t e22_config;
mqtt_config_t mqtt_config;

bool config_setup(const int argc, char *argv[]) {

    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            config_help(argv[0], config_options, config_options_help, (int)(sizeof(config_options_help) / sizeof(config_options_help[0])));
            exit(EXIT_SUCCESS);
        }

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    serial_config_populate(&serial_config);
    e22_config_populate(&e22_config);
    mqtt_config_populate(&mqtt_config);
    mesh_config_populate(&process_state.mesh_state);
    dedup_config_populate(&process_state.dedup_state);
    process_state.dedup_state.running = &running;
    process_config_populate();
    stats_config_populate(&process_state.stats_state);

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void signal_handler(const int sig __attribute__((unused))) {
    if (running) {
        printf("stopping\n");
        running = false;
    }
}

int main(int argc, char *argv[]) {

    int ret = EXIT_FAILURE;

    setbuf(stdout, NULL);
    printf("starting (iotdata gateway: variants=%d, features=mesh,dedup)\n", IOTDATA_VARIANT_MAPS_COUNT);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_setup(argc, argv))
        goto end_all;

    if (!serial_begin(&serial_config) || !serial_connect()) {
        fprintf(stderr, "device: connect failure (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
        goto end_all;
    }
    if (!device_connect(E22900T22_MODULE_USB, &e22_config))
        goto end_serial;
    printf("device: connect success (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));

    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer()))
        goto end_device;
    if (!mqtt_begin(&mqtt_config))
        goto end_device;
    if (!mesh_begin(&process_state.mesh_state, device_packet_write, dedup_check_and_add_handler))
        goto end_mqtt;
    if (!dedup_begin(&process_state.dedup_state, process_state.mesh_state.station_id, &process_state.mesh_state.dedup_ring))
        goto end_mesh;
    stats_begin(&process_state.stats_state, process_state.mesh_state.station_id, &e22_config);

    process_run();
    ret = EXIT_SUCCESS;

    stats_end(&process_state.stats_state);
    dedup_end(&process_state.dedup_state);
end_mesh:
    mesh_end(&process_state.mesh_state);
end_mqtt:
    mqtt_end();
end_device:
    device_disconnect();
end_serial:
    serial_end();
end_all:
    return ret;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
