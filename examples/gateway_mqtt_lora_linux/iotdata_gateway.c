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

bool e22_debug = false;
void e22_printf_debug(const char *format, ...) {
    if (!e22_debug)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void e22_printf_stdout(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void e22_printf_stderr(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
#define PRINTF_DEBUG e22_printf_debug
#define PRINTF_ERROR e22_printf_stderr
#define PRINTF_INFO  e22_printf_stdout
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

#include "iotdata_gateway_mesh.h"
#include "iotdata_gateway_dedup.h"
#include "iotdata_gateway_stats.h"
#include "iotdata_gateway_process.h"

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

#define STATS_INTERVAL_DEFAULT           (5 * 60)
#define INTERVAL_RSSI_DEFAULT            (1 * 60)
#define INTERVAL_BEACON_DEFAULT          60 /* seconds */

#define GATEWAY_STATION_ID_DEFAULT       1

#include "config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"help",                            no_argument,       0, 'h'},
    {"config",                          required_argument, 0, 0},
    {"port",                            required_argument, 0, 0},
    {"rate",                            required_argument, 0, 0},
    {"bits",                            required_argument, 0, 0},
    {"address",                         required_argument, 0, 0},
    {"network",                         required_argument, 0, 0},
    {"channel",                         required_argument, 0, 0},
    {"packet-size",                     required_argument, 0, 0},
    {"packet-rate",                     required_argument, 0, 0},
    {"rssi-channel",                    required_argument, 0, 0},
    {"rssi-packet",                     required_argument, 0, 0},
    {"listen-before-transmit",          required_argument, 0, 0},
    {"read-timeout-command",            required_argument, 0, 0},
    {"read-timeout-packet",             required_argument, 0, 0},
    {"interval-rssi",                   required_argument, 0, 0},
    {"debug-e22",                       required_argument, 0, 0},
    {"mqtt-client",                     required_argument, 0, 0},
    {"mqtt-server",                     required_argument, 0, 0},
    {"mqtt-topic-prefix",               required_argument, 0, 0},
    {"mqtt-tls-insecure",               required_argument, 0, 0},
    {"mqtt-reconnect-delay",            required_argument, 0, 0},
    {"mqtt-reconnect-delay-max",        required_argument, 0, 0},
    {"mesh-enable",                     required_argument, 0, 0},
    {"mesh-station-id",                 required_argument, 0, 0},
    {"mesh-beacon-interval",            required_argument, 0, 0},
    {"debug-mesh",                      required_argument, 0, 0},
    {"dedup-enable",                    required_argument, 0, 0},
    {"dedup-port",                      required_argument, 0, 0},
    {"dedup-peers",                     required_argument, 0, 0},
    {"dedup-delay",                     required_argument, 0, 0},
    {"debug-dedup",                     required_argument, 0, 0},
    {"stats-display-interval",          required_argument, 0, 0},
    {"stats-publish-interval",          required_argument, 0, 0},
    {"stats-mqtt-topic",                required_argument, 0, 0},
    {"debug",                           required_argument, 0, 0},
    {"debug-data",                      required_argument, 0, 0},
    {0, 0, 0, 0}
};

const config_option_help_t config_options_help [] = {
    {"help",                            "Display this help message and exit"},
    {"config",                          "Configuration file path (default: " CONFIG_FILE_DEFAULT ")"},
    {"port",                            "Serial port device (default: " SERIAL_PORT_DEFAULT ")"},
    {"rate",                            "Serial baud rate (default: 9600)"},
    {"bits",                            "Serial data bits (default: 8N1)"},
    {"address",                         "E22 module address (hex)"},
    {"network",                         "E22 network ID (hex)"},
    {"channel",                         "E22 radio channel"},
    {"packet-size",                     "E22 packet size"},
    {"packet-rate",                     "E22 packet rate"},
    {"rssi-channel",                    "Enable channel RSSI capture (true/false)"},
    {"rssi-packet",                     "Enable packet RSSI capture (true/false)"},
    {"listen-before-transmit",          "Enable listen-before-transmit mode (true/false)"},
    {"read-timeout-command",            "E22 command read timeout in ms"},
    {"read-timeout-packet",             "E22 packet read timeout in ms"},
    {"interval-rssi",                   "RSSI polling interval in seconds"},
    {"debug-e22",                       "Enable E22 module debug output (true/false)"},
    {"mqtt-client",                     "MQTT client ID (default: " MQTT_CLIENT_DEFAULT ")"},
    {"mqtt-server",                     "MQTT server URL (default: " MQTT_SERVER_DEFAULT ")"},
    {"mqtt-topic-prefix",               "MQTT topic prefix (default: " MQTT_TOPIC_PREFIX_DEFAULT ")"},
    {"mqtt-tls-insecure",               "Disable MQTT TLS verification (true/false)"},
    {"mqtt-reconnect-delay",            "MQTT reconnect delay in seconds"},
    {"mqtt-reconnect-delay-max",        "MQTT max reconnect delay in seconds"},
    {"mesh-enable",                     "Enable mesh network support (true/false)"},
    {"mesh-station-id",                 "This gateway's mesh station ID"},
    {"mesh-beacon-interval",            "Mesh beacon transmission interval in seconds"},
    {"debug-mesh",                      "Enable mesh debug output (true/false)"},
    {"dedup-enable",                    "Enable cross-gateway deduplication (true/false)"},
    {"dedup-port",                      "UDP port for dedup peer communication"},
    {"dedup-peers",                     "Comma-separated list of dedup peers (host:port)"},
    {"dedup-delay",                     "Dedup batch send delay in ms"},
    {"debug-dedup",                     "Enable dedup debug output (true/false)"},
    {"stats-display-interval",          "Stats stdout display interval in seconds (0 disables)"},
    {"stats-publish-interval",          "Stats MQTT publish interval in seconds (0 disables)"},
    {"stats-mqtt-topic",                "MQTT stats topic prefix (default: iotdata/stats)"},
    {"debug",                           "Enable general debug output (true/false)"},
    {"crypt",                           "E22 encryption key (default: 0x0000)"},
    {"transmit-power",                  "E22 transmit power level 0-3 (default: 0)"},
    {"transmission-method",             "E22 transmission method 0=transparent, 1=fixed-point (default: 0)"},
    {"debug-data",                      "Enable hex dump of received packet data (true/false)"},
};
// clang-format on

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void e22_serial_config_populate(serial_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->port = config_get_string("port", SERIAL_PORT_DEFAULT);
    cfg->rate = config_get_integer("rate", SERIAL_RATE_DEFAULT);
    cfg->bits = config_get_bits("bits", SERIAL_BITS_DEFAULT);

    printf("config: e22-serial: port=%s, rate=%d, bits=%s\n", cfg->port, cfg->rate, serial_bits_str(cfg->bits));
}

void e22_device_config_populate(e22900t22_config_t *cfg) {
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
    e22_debug = cfg->debug;

    printf("config: e22-device: address=0x%04" PRIX16 ", network=0x%02" PRIX8 ", channel=%d, packet-size=%d, packet-rate=%d, rssi-channel=%s, rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%" PRIu32
           ", read-timeout-packet=%" PRIu32 ", crypt=0x%04" PRIX16 ", transmit-power=%" PRIu8 ", transmission-method=%s, mode-relay=%s, debug=%s\n",
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

void iotdata_dedup_config_populate(dedup_state_t *state) {
    memset(state, 0, sizeof(*state));

    state->enabled = config_get_bool("dedup-enable", false);
    state->port = (uint16_t)config_get_integer("dedup-port", DEDUP_PORT_DEFAULT);
    state->delay_ms = (uint32_t)config_get_integer("dedup-delay", DEDUP_DELAY_MS_DEFAULT);
    const char *peers = config_get_string("dedup-peers", "");
    dedup_peers_parse(state, peers);
    state->debug = config_get_bool("debug-dedup", false);

    printf("config: dedup: enabled=%c, port=%" PRIu16 ", peers=%s, delay=%" PRIu32 "ms, debug=%s\n", state->enabled ? 'y' : 'n', state->port, peers, state->delay_ms, state->debug ? "on" : "off");
}

void iotdata_mesh_config_populate(mesh_state_t *state) {
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
    size_t len = strlen(state->mqtt_topic);
    if (len > 0 && state->mqtt_topic[len - 1] == '/')
        state->mqtt_topic[len - 1] = '\0';

    printf("config: stats: mqtt-topic=%s\n", state->mqtt_topic);
}

void process_config_populate(process_state_t *state) {
    memset(state, 0, sizeof(*state));

    state->mqtt_topic_prefix = config_get_string("mqtt-topic-prefix", MQTT_TOPIC_PREFIX_DEFAULT);
    state->capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    state->capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    state->interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);
    state->stats_display_interval = config_get_integer("stats-display-interval", STATS_INTERVAL_DEFAULT);
    state->stats_publish_interval = config_get_integer("stats-publish-interval", STATS_INTERVAL_DEFAULT);
    state->debug = config_get_bool("debug", false);
    state->debug_data = config_get_bool("debug-data", false);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

typedef struct {
    serial_config_t e22_serial_config;
    e22900t22_config_t e22_device_config;
    mqtt_config_t mqtt_config;
    mesh_state_t mesh_state;
    dedup_state_t dedup_state;
    stats_state_t stats_state;
    process_state_t process_state;
    volatile bool running;
} system_t;

static system_t system_state;

// -----------------------------------------------------------------------------------------------------------------------------------------

bool system_config(system_t *state, const int argc, char *argv[]) {

    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            config_help(argv[0], config_options, config_options_help, (int)(sizeof(config_options_help) / sizeof(config_options_help[0])));
            exit(EXIT_SUCCESS);
        }

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    e22_serial_config_populate(&state->e22_serial_config);
    e22_device_config_populate(&state->e22_device_config);
    mqtt_config_populate(&state->mqtt_config);
    iotdata_mesh_config_populate(&state->mesh_state);
    iotdata_dedup_config_populate(&state->dedup_state);
    stats_config_populate(&state->stats_state);
    process_config_populate(&state->process_state);

    state->running = false;

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void signal_handler(const int sig __attribute__((unused))) {
    if (system_state.running) {
        printf("stopping\n");
        system_state.running = false;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {

    int ret = EXIT_FAILURE;

    setbuf(stdout, NULL);
    printf("starting (iotdata gateway: variants=%d, features=mesh,dedup,stats,mqtt)\n", IOTDATA_VARIANT_MAPS_COUNT);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    system_t *state = &system_state;

    if (!system_config(state, argc, argv))
        goto end_all;

    // DEVICE
    if (!serial_begin(&state->e22_serial_config) || !serial_connect()) {
        fprintf(stderr, "serial: connect failure (port=%s, rate=%d, bits=%s)\n", state->e22_serial_config.port, state->e22_serial_config.rate, serial_bits_str(state->e22_serial_config.bits));
        goto end_all;
    }
    if (!device_connect(E22900T22_MODULE_USB, &state->e22_device_config)) {
        fprintf(stderr, "device: connect failure (port=%s, rate=%d, bits=%s)\n", state->e22_serial_config.port, state->e22_serial_config.rate, serial_bits_str(state->e22_serial_config.bits));
        goto end_serial;
    }
    printf("device: connect success (port=%s, rate=%d, bits=%s)\n", state->e22_serial_config.port, state->e22_serial_config.rate, serial_bits_str(state->e22_serial_config.bits));
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer()))
        goto end_device;

    // MQTT
    if (!mqtt_begin(&state->mqtt_config))
        goto end_device;

    state->running = true;

    // IOTDATA
    if (!mesh_begin(&state->mesh_state, device_packet_write, dedup_check_and_add_handler, (void *)state))
        goto end_mqtt;
    if (!dedup_begin(&state->dedup_state, state->mesh_state.station_id, &state->mesh_state.dedup_ring, &state->running))
        goto end_mesh;

    // PROCESS
    stats_begin(&state->stats_state, state->mesh_state.station_id, &state->e22_device_config);
    ret = process_run(&state->process_state, &state->mesh_state, &state->dedup_state, &state->stats_state, &state->running) ? EXIT_SUCCESS : EXIT_FAILURE;
    stats_end(&state->stats_state);

    dedup_end(&state->dedup_state);
end_mesh:
    mesh_end(&state->mesh_state);
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
