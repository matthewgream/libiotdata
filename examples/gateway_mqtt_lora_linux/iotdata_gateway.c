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
 * Depends upon EBYTE E22 connector
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

// 0.2 ≈ 51/256, 0.8 ≈ 205/256
#define EMA_ALPHA_NUM   51
#define EMA_ALPHA_DENOM 256
void ema_update(uint8_t value, uint8_t *value_ema, uint32_t *value_cnt) {
    *value_ema = ((*value_cnt)++ == 0) ? value : (uint8_t)((EMA_ALPHA_NUM * (uint16_t)value + (EMA_ALPHA_DENOM - EMA_ALPHA_NUM) * (uint16_t)(*value_ema)) / EMA_ALPHA_DENOM);
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

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

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

#define INTERVAL_STAT_DEFAULT            (5 * 60)
#define INTERVAL_RSSI_DEFAULT            (1 * 60)
#define INTERVAL_BEACON_DEFAULT          60 /* seconds */

#define GATEWAY_STATION_ID_DEFAULT       1

#include "config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"help",                  no_argument,       0, 'h'},
    {"config",                required_argument, 0, 0},
    {"port",                  required_argument, 0, 0},
    {"rate",                  required_argument, 0, 0},
    {"bits",                  required_argument, 0, 0},
    {"address",               required_argument, 0, 0},
    {"network",               required_argument, 0, 0},
    {"channel",               required_argument, 0, 0},
    {"packet-size",           required_argument, 0, 0},
    {"packet-rate",           required_argument, 0, 0},
    {"rssi-channel",          required_argument, 0, 0},
    {"rssi-packet",           required_argument, 0, 0},
    {"listen-before-transmit",required_argument, 0, 0},
    {"read-timeout-command",  required_argument, 0, 0},
    {"read-timeout-packet",   required_argument, 0, 0},
    {"interval-rssi",         required_argument, 0, 0},
    {"interval-stat",         required_argument, 0, 0},
    {"debug-e22",             required_argument, 0, 0},
    {"mqtt-client",           required_argument, 0, 0},
    {"mqtt-server",           required_argument, 0, 0},
    {"mqtt-topic-prefix",     required_argument, 0, 0},
    {"mqtt-tls-insecure",     required_argument, 0, 0},
    {"mqtt-reconnect-delay",  required_argument, 0, 0},
    {"mqtt-reconnect-delay-max", required_argument, 0, 0},
    {"mesh-enable",           required_argument, 0, 0},
    {"mesh-station-id",       required_argument, 0, 0},
    {"mesh-beacon-interval",  required_argument, 0, 0},
    {"debug-mesh",            required_argument, 0, 0},
    {"dedup-enable",             required_argument, 0, 0},
    {"dedup-port",               required_argument, 0, 0},
    {"dedup-peers",              required_argument, 0, 0},
    {"dedup-delay",              required_argument, 0, 0},
    {"debug-dedup",              required_argument, 0, 0},
    {"debug",                 required_argument, 0, 0},
    {"debug-data",            required_argument, 0, 0},
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
    {"interval-stat",          "Statistics output interval in seconds"},
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
    {"debug",                  "Enable general debug output (true/false)"},
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
    cfg->packet_size = (uint8_t)config_get_integer("packet-size", E22900T22_CONFIG_PACKET_SIZE_DEFAULT);
    cfg->packet_rate = (uint8_t)config_get_integer("packet-rate", E22900T22_CONFIG_PACKET_RATE_DEFAULT);
    cfg->crypt = E22900T22_CONFIG_CRYPT_DEFAULT;
    cfg->transmit_power = E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT;
    cfg->transmission_method = E22900T22_CONFIG_TRANSMISSION_METHOD_DEFAULT;
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

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

struct {
    const char *mqtt_topic_prefix;
    bool capture_rssi_packet;
    bool capture_rssi_channel;
    time_t interval_stat;
    time_t interval_rssi;
    time_t interval_stat_last;
    time_t interval_rssi_last;
    bool debug;
    bool debug_data;
    /* statistics */
    uint32_t stat_rssi_channel_cnt;
    uint8_t stat_rssi_channel_ema;
    uint32_t stat_rssi_packet_cnt;
    uint8_t stat_rssi_packet_ema;
    uint32_t stat_packets_okay;
    uint32_t stat_packets_drop;
    uint32_t stat_packets_decode_err;
} process_state;

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_sensor_packet(uint16_t station_id, uint16_t sequence) {
    if (mesh_state.enabled)
        if (!dedup_check_and_add(station_id, sequence)) {
            mesh_state.stat_duplicates++;
            if (process_state.debug || mesh_state.debug)
                printf("process: mesh direct packet duplicate suppressed (station=0x%04" PRIX16 ", sequence=%" PRIu16 ")\n", station_id, sequence);
            return false;
        }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_sensor_packet(const uint8_t *packet_buffer, int packet_length, uint8_t variant_id, uint16_t station_id, uint16_t sequence, const char *topic_prefix, const char *via) {
    (void)sequence;
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant_id);
    if (vdef == NULL) {
        fprintf(stderr, "process: unknown variant %" PRIu8 " (station=0x%04" PRIX16 ", size=%d)\n", variant_id, station_id, packet_length);
        process_state.stat_packets_drop++;
        return;
    }
    char *json = NULL;
    iotdata_decode_to_json_scratch_t scratch;
    iotdata_status_t rc;
    if ((rc = iotdata_decode_to_json(packet_buffer, (size_t)packet_length, &json, &scratch)) != IOTDATA_OK) {
        fprintf(stderr, "process: decode failed: %s (variant=%" PRIu8 ", station=0x%04" PRIX16 ", size=%d)\n", iotdata_strerror(rc), variant_id, station_id, packet_length);
        process_state.stat_packets_decode_err++;
        return;
    }
    char topic[255];
    snprintf(topic, sizeof(topic), "%s/%s/%04" PRIX16, topic_prefix, vdef->name, station_id);
    if (mqtt_send(topic, json, (int)strlen(json)))
        process_state.stat_packets_okay++;
    else {
        fprintf(stderr, "process: mqtt send failed (topic=%s, size=%d)\n", topic, (int)strlen(json));
        process_state.stat_packets_drop++;
    }
    if (process_state.debug)
        printf("  -> %s (%d bytes%s%s)\n", topic, (int)strlen(json), via ? " via " : "", via ? via : "");
    free(json);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_mesh_packet(const uint8_t *packet_buffer, int packet_length, uint8_t variant_id, uint16_t station_id, uint16_t sequence, const char *topic_prefix) {
    (void)variant_id;
    const uint8_t ctrl_type = iotdata_mesh_peek_ctrl_type(packet_buffer, packet_length);
    mesh_state.stat_mesh_ctrl_rx++;
    if (mesh_state.debug)
        printf("mesh: rx %s from station=0x%04" PRIX16 ", sequence=%" PRIu16 " (%d bytes)\n", iotdata_mesh_ctrl_name(ctrl_type), station_id, sequence, packet_length);
    switch (ctrl_type) {
    case IOTDATA_MESH_CTRL_FORWARD: {
        const uint8_t *inner;
        int inner_len;
        if (mesh_handle_forward(packet_buffer, packet_length, &inner, &inner_len)) {
            uint8_t inner_variant;
            uint16_t inner_station, inner_sequence;
            if (iotdata_peek(inner, (size_t)inner_len, &inner_variant, &inner_station, &inner_sequence) != IOTDATA_OK) {
                fprintf(stderr, "mesh: FORWARD inner packet peek failed (len=%d)\n", inner_len);
                process_state.stat_packets_drop++;
            } else if (dedup_check_sensor_packet(inner_station, inner_sequence))
                process_sensor_packet(inner, inner_len, inner_variant, inner_station, inner_sequence, topic_prefix, "mesh");
        }
        break;
    }
    case IOTDATA_MESH_CTRL_BEACON:
        mesh_handle_beacon(packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_ACK:
        if (mesh_state.debug)
            printf("mesh: rx unexpected ACK from station=0x%04" PRIX16 "\n", station_id);
        break;
    case IOTDATA_MESH_CTRL_ROUTE_ERROR:
        mesh_handle_route_error(packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_NEIGHBOUR_RPT:
        mesh_handle_neighbour_report(packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_PONG:
        mesh_handle_pong(packet_buffer, packet_length);
        break;
    default:
        mesh_state.stat_mesh_unknown++;
        if (mesh_state.debug)
            printf("mesh: rx unknown ctrl_type=0x%02" PRIX8 " from station=0x%04" PRIX16 "\n", ctrl_type, station_id);
        break;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_stats(time_t period_stat) {
    const uint32_t rate_okay = (process_state.stat_packets_okay * 6000) / (uint32_t)period_stat, rate_drop = (process_state.stat_packets_drop * 6000) / (uint32_t)period_stat;
    printf("packets{okay=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min), drop=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min)}", process_state.stat_packets_okay, rate_okay / 100, rate_okay % 100, process_state.stat_packets_drop, rate_drop / 100,
           rate_drop % 100);
    process_state.stat_packets_okay = process_state.stat_packets_drop = process_state.stat_packets_decode_err = 0;
    if (process_state.capture_rssi_channel || process_state.capture_rssi_packet) {
        printf(", rssi{");
        if (process_state.capture_rssi_channel)
            printf("channel=%d dBm (%" PRIu32 ")", get_rssi_dbm(process_state.stat_rssi_channel_ema), process_state.stat_rssi_channel_cnt);
        if (process_state.capture_rssi_channel && process_state.capture_rssi_packet)
            printf(", ");
        if (process_state.capture_rssi_packet)
            printf("packet=%d dBm (%" PRIu32 ")", get_rssi_dbm(process_state.stat_rssi_packet_ema), process_state.stat_rssi_packet_cnt);
        printf("}");
    }
    if (mesh_state.enabled) {
        printf(", mesh{fwd=%" PRIu32 ", unwrap=%" PRIu32 ", dedup=%" PRIu32 ", beacons=%" PRIu32 ", acks=%" PRIu32 ", ctrl=%" PRIu32 "}", mesh_state.stat_forwards_rx, mesh_state.stat_forwards_unwrapped, mesh_state.stat_duplicates,
               mesh_state.stat_beacons_tx, mesh_state.stat_acks_tx, mesh_state.stat_mesh_ctrl_rx);
        mesh_state.stat_forwards_rx = mesh_state.stat_forwards_unwrapped = 0;
        mesh_state.stat_duplicates = mesh_state.stat_acks_tx = 0;
        mesh_state.stat_mesh_ctrl_rx = mesh_state.stat_mesh_unknown = 0;
    }
    if (dedup_state.enabled) {
        printf(", dedup{sends=%" PRIu32 "/%" PRIu32 ", recvs=%" PRIu32 "/%" PRIu32 ", injected=%" PRIu32 "}", dedup_state.stat_send_cycles, dedup_state.stat_send_entries, dedup_state.stat_recv_cycles, dedup_state.stat_recv_entries,
               dedup_state.stat_injected);
        dedup_state.stat_send_cycles = dedup_state.stat_send_entries = 0;
        dedup_state.stat_recv_cycles = dedup_state.stat_recv_entries = 0;
        dedup_state.stat_injected = 0;
    }
    printf(", mqtt{%s, disconnects=%" PRIu32 "}", mqtt_is_connected() ? "up" : "down", mqtt_stat_disconnects);
    printf("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_config_populate(void) {
    memset(&process_state, 0, sizeof(process_state));
    process_state.mqtt_topic_prefix = config_get_string("mqtt-topic-prefix", MQTT_TOPIC_PREFIX_DEFAULT);
    process_state.capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    process_state.capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    process_state.interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);
    process_state.interval_stat = config_get_integer("interval-stat", INTERVAL_STAT_DEFAULT);
    process_state.debug = config_get_bool("debug", false);
    process_state.debug_data = config_get_bool("debug-data", false);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_run(void) {
    uint8_t packet_buffer[E22900T22_PACKET_MAXSIZE + 1]; /* +1 for RSSI byte */
    int packet_length;

    printf("process: iotdata gateway (stat=%" PRIu32 "s, rssi=%" PRIu32 "s [packets=%c, channel=%c], topic-prefix=%s", (uint32_t)process_state.interval_stat, (uint32_t)process_state.interval_rssi,
           process_state.capture_rssi_packet ? 'y' : 'n', process_state.capture_rssi_channel ? 'y' : 'n', process_state.mqtt_topic_prefix);
    if (mesh_state.enabled)
        printf(", mesh=on, beacon=%" PRIu32 "s", (uint32_t)mesh_state.beacon_interval);
    printf(")\n");

    for (int i = 0; i < IOTDATA_VARIANT_MAPS_COUNT; i++) {
        const iotdata_variant_def_t *vdef = iotdata_get_variant((uint8_t)i);
        printf("process: variant[%d] = \"%s\" (pres_bytes=%" PRIu8 ") -> %s/%s/<station>\n", i, vdef->name, vdef->num_pres_bytes, process_state.mqtt_topic_prefix, vdef->name);
    }
    if (mesh_state.enabled)
        printf("process: variant[15] = mesh control (gateway station=0x%04" PRIX16 ")\n", mesh_state.station_id);

    while (running) {

        // packet processing
        uint8_t packet_rssi = 0, channel_rssi = 0;
        if (device_packet_read(packet_buffer, sizeof(packet_buffer), &packet_length, &packet_rssi) && running) {
            if (process_state.debug_data)
                debug_hexdump("data: ", packet_buffer, (size_t)packet_length);
            if (process_state.capture_rssi_packet && packet_rssi > 0)
                ema_update(packet_rssi, &process_state.stat_rssi_packet_ema, &process_state.stat_rssi_packet_cnt);
            uint8_t variant_id;
            uint16_t station_id, sequence;
            if (iotdata_peek(packet_buffer, (size_t)packet_length, &variant_id, &station_id, &sequence) != IOTDATA_OK) {
                fprintf(stderr, "process: packet too short for iotdata header (size=%d)\n", packet_length);
                process_state.stat_packets_drop++;
            } else if (variant_id == IOTDATA_MESH_VARIANT)
                process_mesh_packet(packet_buffer, packet_length, variant_id, station_id, sequence, process_state.mqtt_topic_prefix);
            else
                process_sensor_packet(packet_buffer, packet_length, variant_id, station_id, sequence, process_state.mqtt_topic_prefix, "direct");
        }

        // rssi update
        if (running && process_state.capture_rssi_channel && intervalable(process_state.interval_rssi, &process_state.interval_rssi_last))
            if (device_channel_rssi_read(&channel_rssi) && running)
                ema_update(channel_rssi, &process_state.stat_rssi_channel_ema, &process_state.stat_rssi_channel_cnt);

        // mesh beacons
        if (running && mesh_state.enabled && intervalable(mesh_state.beacon_interval, &mesh_state.beacon_last))
            mesh_beacon_send();

        // stats output
        time_t period_stat;
        if (running && (period_stat = intervalable(process_state.interval_stat, &process_state.interval_stat_last)) > 0)
            process_stats(period_stat);
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
    mesh_config_populate();
    dedup_config_populate();
    process_config_populate();

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
    if (!mesh_begin(device_packet_write, dedup_check_and_add))
        goto end_mqtt;
    if (!dedup_begin(mesh_state.station_id, &mesh_state.dedup_ring))
        goto end_mesh;

    process_run();
    ret = EXIT_SUCCESS;

    dedup_end();
end_mesh:
    mesh_end();
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
