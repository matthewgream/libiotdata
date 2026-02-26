// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22U to MQTT — iotdata gateway
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
 * See: APPENDIX_MESH.md for the full mesh relay protocol specification.
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
#include <time.h>

bool debug_readandsend = false;
bool debug_mesh = false;

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

#define EMA_ALPHA 0.2f
void ema_update(unsigned char value, unsigned char *value_ema, unsigned long *value_cnt) {
    *value_ema = (*value_cnt)++ == 0 ? value : (unsigned char)((EMA_ALPHA * (float)value) + ((1.0f - EMA_ALPHA) * (*value_ema)));
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "serial_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool debug_e22900t22u = false;
void printf_debug_e22900t22u(const char *format, ...) {
    if (debug_e22900t22u) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }
}
void printf_stdout_e22900t22u(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void printf_stderr_e22900t22u(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
#define PRINTF_DEBUG printf_debug_e22900t22u
#define PRINTF_ERROR printf_stderr_e22900t22u
#define PRINTF_INFO  printf_stdout_e22900t22u
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "e22xxxtxx.h"

void __sleep_ms(const unsigned long ms) {
    usleep((unsigned int)ms * 1000);
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

#define CONFIG_FILE_DEFAULT        "iotdata_gateway.cfg"

#define SERIAL_PORT_DEFAULT        "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT        9600
#define SERIAL_BITS_DEFAULT        SERIAL_8N1

#define MQTT_CLIENT_DEFAULT        "iotdata_gateway"
#define MQTT_SERVER_DEFAULT        "mqtt://localhost"
#define MQTT_TLS_DEFAULT           false
#define MQTT_SYNCHRONOUS_DEFAULT   false
#define MQTT_TOPIC_PREFIX_DEFAULT  "iotdata"

#define INTERVAL_STAT_DEFAULT      (5 * 60)
#define INTERVAL_RSSI_DEFAULT      (1 * 60)
#define INTERVAL_BEACON_DEFAULT    60 /* seconds */

#define GATEWAY_STATION_ID_DEFAULT 1

#include "config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"config",                required_argument, 0, 0},
    {"mqtt-client",           required_argument, 0, 0},
    {"mqtt-server",           required_argument, 0, 0},
    {"mqtt-topic-prefix",     required_argument, 0, 0},
    {"port",                  required_argument, 0, 0},
    {"rate",                  required_argument, 0, 0},
    {"bits",                  required_argument, 0, 0},
    {"address",               required_argument, 0, 0},
    {"network",               required_argument, 0, 0},
    {"channel",               required_argument, 0, 0},
    {"packet-size",           required_argument, 0, 0},
    {"listen-before-transmit",required_argument, 0, 0},
    {"rssi-packet",           required_argument, 0, 0},
    {"rssi-channel",          required_argument, 0, 0},
    {"read-timeout-command",  required_argument, 0, 0},
    {"read-timeout-packet",   required_argument, 0, 0},
    {"interval-stat",         required_argument, 0, 0},
    {"interval-rssi",         required_argument, 0, 0},
    {"mesh-enable",           required_argument, 0, 0},
    {"mesh-station-id",       required_argument, 0, 0},
    {"mesh-beacon-interval",  required_argument, 0, 0},
    {"debug-e22900t22u",      required_argument, 0, 0},
    {"debug-mesh",            required_argument, 0, 0},
    {"debug",                 required_argument, 0, 0},
    {"mqtt-tls-insecure",     required_argument, 0, 0},
    {0, 0, 0, 0}
};
// clang-format on

void config_populate_serial(serial_config_t *cfg) {
    cfg->port = config_get_string("port", SERIAL_PORT_DEFAULT);
    cfg->rate = config_get_integer("rate", SERIAL_RATE_DEFAULT);
    cfg->bits = config_get_bits("bits", SERIAL_BITS_DEFAULT);

    printf("config: serial: port=%s, rate=%d, bits=%s\n", cfg->port, cfg->rate, serial_bits_str(cfg->bits));
}

void config_populate_e22900t22u(e22900t22_config_t *cfg) {
    cfg->address = (unsigned short)config_get_integer("address", CONFIG_ADDRESS_DEFAULT);
    cfg->network = (unsigned char)config_get_integer("network", CONFIG_NETWORK_DEFAULT);
    cfg->channel = (unsigned char)config_get_integer("channel", CONFIG_CHANNEL_DEFAULT);
    cfg->packet_maxsize = (unsigned char)config_get_integer("packet-size", CONFIG_PACKET_MAXSIZE_DEFAULT);
    cfg->listen_before_transmit = config_get_bool("listen-before-transmit", CONFIG_LISTEN_BEFORE_TRANSMIT);
    cfg->rssi_packet = config_get_bool("rssi-packet", CONFIG_RSSI_PACKET_DEFAULT);
    cfg->rssi_channel = config_get_bool("rssi-channel", CONFIG_RSSI_CHANNEL_DEFAULT);
    cfg->read_timeout_command = (unsigned long)config_get_integer("read-timeout-command", CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    cfg->read_timeout_packet = (unsigned long)config_get_integer("read-timeout-packet", CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    cfg->debug = config_get_bool("debug", false);

    printf("config: e22900t22u: address=0x%04x, network=0x%02x, channel=%d, packet-size=%d, rssi-channel=%s, "
           "rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%lu, read-timeout-packet=%lu, debug=%s\n",
           cfg->address, cfg->network, cfg->channel, cfg->packet_maxsize, cfg->rssi_channel ? "on" : "off", cfg->rssi_packet ? "on" : "off", cfg->listen_before_transmit ? "on" : "off", cfg->read_timeout_command, cfg->read_timeout_packet,
           cfg->debug ? "on" : "off");
}

void config_populate_mqtt(mqtt_config_t *cfg) {
    cfg->client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    cfg->server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    cfg->tls_insecure = config_get_bool("mqtt-tls-insecure", MQTT_TLS_DEFAULT);
    cfg->use_synchronous = MQTT_SYNCHRONOUS_DEFAULT;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static struct {
    bool enabled;
    uint16_t station_id;             /* this gateway's station_id for mesh packets */
    time_t beacon_interval;          /* seconds between beacon transmissions */
    uint16_t beacon_generation;      /* increments each beacon round */
    uint16_t mesh_seq;               /* mesh packet sequence counter */
    time_t beacon_last;              /* last beacon TX time */
    iotdata_mesh_dedup_ring_t dedup; /* dedup ring */
    /* statistics */
    unsigned long stat_beacons_tx;
    unsigned long stat_forwards_rx;
    unsigned long stat_forwards_unwrapped;
    unsigned long stat_duplicates;
    unsigned long stat_acks_tx;
    unsigned long stat_mesh_ctrl_rx;
    unsigned long stat_mesh_unknown;
} mesh;

static void mesh_begin(void) {
    memset(&mesh, 0, sizeof(mesh));
    mesh.enabled = config_get_bool("mesh-enable", false);
    mesh.station_id = (uint16_t)config_get_integer("mesh-station-id", GATEWAY_STATION_ID_DEFAULT);
    mesh.beacon_interval = config_get_integer("mesh-beacon-interval", INTERVAL_BEACON_DEFAULT);
    iotdata_mesh_dedup_init(&mesh.dedup);
    if (mesh.enabled)
        printf("mesh: enabled, gateway station_id=%u, beacon interval=%lds\n", mesh.station_id, mesh.beacon_interval);
    else
        printf("mesh: disabled\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static void mesh_beacon_send(void) {
    uint8_t buf[IOTDATA_MESH_BEACON_SIZE];
    const iotdata_mesh_beacon_t beacon = {
        .sender_station = mesh.station_id,
        .sender_seq = mesh.mesh_seq++,
        .gateway_id = mesh.station_id,
        .cost = 0,
        .flags = IOTDATA_MESH_FLAG_ACCEPTING,
        .generation = mesh.beacon_generation++,
    };
    mesh.beacon_generation &= (IOTDATA_MESH_GENERATION_MOD - 1);
    iotdata_mesh_pack_beacon(buf, &beacon);
    if (debug_mesh)
        printf("mesh: tx BEACON gen=%u station=%u\n", beacon.generation, beacon.sender_station);
    if (device_packet_write(buf, IOTDATA_MESH_BEACON_SIZE))
        mesh.stat_beacons_tx++;
    else
        fprintf(stderr, "mesh: beacon tx failed\n");
}

static void mesh_ack_send(uint16_t fwd_station, uint16_t fwd_seq) {
    uint8_t buf[IOTDATA_MESH_ACK_SIZE];
    const iotdata_mesh_ack_t ack = {
        .sender_station = mesh.station_id,
        .sender_seq = mesh.mesh_seq++,
        .fwd_station = fwd_station,
        .fwd_seq = fwd_seq,
    };
    iotdata_mesh_pack_ack(buf, &ack);
    if (debug_mesh)
        printf("mesh: tx ACK to station=%u seq=%u\n", fwd_station, fwd_seq);
    if (device_packet_write(buf, IOTDATA_MESH_ACK_SIZE))
        mesh.stat_acks_tx++;
    else
        fprintf(stderr, "mesh: ack tx failed\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool mesh_handle_forward(const unsigned char *buf, int len, const unsigned char **inner, int *inner_len) {
    iotdata_mesh_forward_t fwd;
    if (!iotdata_mesh_unpack_forward(buf, len, &fwd)) {
        fprintf(stderr, "mesh: FORWARD unpack failed (len=%d)\n", len);
        return false;
    }
    mesh.stat_forwards_rx++;
    if (debug_mesh)
        printf("mesh: rx FORWARD from station=%u seq=%u ttl=%u origin={station=%u, seq=%u} inner=%d B\n", fwd.sender_station, fwd.sender_seq, fwd.ttl, fwd.origin_station, fwd.origin_sequence, fwd.inner_len);
    /* duplicate suppression on the ORIGINAL sensor packet */
    if (!iotdata_mesh_dedup_check_and_add(&mesh.dedup, fwd.origin_station, fwd.origin_sequence)) {
        mesh.stat_duplicates++;
        if (debug_mesh)
            printf("mesh: FORWARD duplicate suppressed (origin station=%u seq=%u)\n", fwd.origin_station, fwd.origin_sequence);
        /* still ACK to prevent the forwarder from retrying */
        if (mesh.enabled)
            mesh_ack_send(fwd.sender_station, fwd.sender_seq);
        return false;
    }
    /* ACK the forwarder */
    if (mesh.enabled)
        mesh_ack_send(fwd.sender_station, fwd.sender_seq);
    mesh.stat_forwards_unwrapped++;
    *inner = fwd.inner_packet;
    *inner_len = fwd.inner_len;
    return true;
}

static void mesh_handle_beacon(const unsigned char *buf, int len) {
    /* gateway receiving another gateway's beacon — log for multi-gateway awareness */
    iotdata_mesh_beacon_t b;
    if (iotdata_mesh_unpack_beacon(buf, len, &b))
        if (debug_mesh)
            printf("mesh: rx BEACON from gateway=%u gen=%u cost=%u flags=0x%x\n", b.gateway_id, b.generation, b.cost, b.flags);
}

static void mesh_handle_route_error(const unsigned char *buf, int len) {
    iotdata_mesh_route_error_t err;
    if (iotdata_mesh_unpack_route_error(buf, len, &err))
        printf("mesh: rx ROUTE_ERROR from station=%u reason=%s\n", err.sender_station, iotdata_mesh_reason_name(err.reason));
}

static void mesh_handle_neighbour_report(const unsigned char *buf, int len) {
    /* full topology aggregation is future work — log receipt for now */
    uint8_t variant;
    uint16_t station_id, sequence;
    iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence);
    printf("mesh rx NEIGHBOUR_REPORT from station=%u (%d bytes)\n", station_id, len);
    (void)variant;
    (void)sequence;
}

static void mesh_handle_pong(const unsigned char *buf, int len) {
    uint8_t variant;
    uint16_t station_id, sequence;
    iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence);
    printf("mesh: rx PONG from station=%u (%d bytes)\n", station_id, len);
    (void)variant;
    (void)sequence;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * Process a raw iotdata sensor packet (variant 0–14): decode to JSON
 * and publish to MQTT. Used for both direct-receive and unwrapped
 * FORWARD packets.
 *
 * The 'via' parameter is for logging: NULL for direct, "mesh" for forwarded.
 */

static bool capture_rssi_packet = false, capture_rssi_channel = false;
static unsigned long stat_channel_rssi_cnt = 0, stat_packet_rssi_cnt = 0;
static unsigned char stat_channel_rssi_ema, stat_packet_rssi_ema;
static unsigned long stat_packets_okay = 0, stat_packets_drop = 0, stat_packets_decode_err = 0;

static void process_sensor_packet(const unsigned char *buf, int len, unsigned char packet_rssi, const char *topic_prefix, const char *via) {
    if (capture_rssi_packet && packet_rssi > 0)
        ema_update(packet_rssi, &stat_packet_rssi_ema, &stat_packet_rssi_cnt);

    uint8_t variant_id;
    uint16_t station_id, sequence;
    if (!iotdata_mesh_peek_header(buf, len, &variant_id, &station_id, &sequence)) {
        fprintf(stderr, "process: packet too short for iotdata header (size=%d)\n", len);
        stat_packets_drop++;
        return;
    }
    /* dedup for direct packets (mesh forwards already deduped in mesh_handle_forward) */
    if (via == NULL && mesh.enabled)
        if (!iotdata_mesh_dedup_check_and_add(&mesh.dedup, station_id, sequence)) {
            mesh.stat_duplicates++;
            if (debug_mesh)
                printf("mesh: direct packet duplicate suppressed (station=%u seq=%u)\n", station_id, sequence);
            return;
        }
    if (variant_id >= IOTDATA_VARIANT_MAPS_COUNT) {
        fprintf(stderr, "process: unknown variant %u (station=%u, size=%d)\n", variant_id, station_id, len);
        stat_packets_drop++;
        return;
    }
    // decode to json and publish
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant_id);
    char *json = NULL;
    iotdata_decode_to_json_scratch_t scratch;
    iotdata_status_t rc;
    if ((rc = iotdata_decode_to_json(buf, (size_t)len, &json, &scratch)) != IOTDATA_OK) {
        fprintf(stderr, "process: decode failed: %s (variant=%u, station=%u, size=%d)\n", iotdata_strerror(rc), variant_id, station_id, len);
        stat_packets_decode_err++;
        return;
    }
    char topic[255];
    snprintf(topic, sizeof(topic), "%s/%s/%u", topic_prefix, vdef->name, station_id);
    if (mqtt_send(topic, json, (int)strlen(json)))
        stat_packets_okay++;
    else {
        fprintf(stderr, "process: mqtt send failed (topic=%s, size=%d)\n", topic, (int)strlen(json));
        stat_packets_drop++;
    }
    if (debug_readandsend)
        printf("  -> %s (%d bytes%s%s)\n", topic, (int)strlen(json), via ? " via " : "", via ? via : "");
    free(json);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static time_t interval_stat = 0, interval_stat_last = 0;
static time_t interval_rssi = 0, interval_rssi_last = 0;

void read_and_send(volatile bool *running, const char *topic_prefix) {

    unsigned char packet_buffer[E22900T22_PACKET_MAXSIZE + 1]; /* +1 for RSSI byte */
    int packet_size;

    printf("read-and-publish: iotdata gateway (stat=%lds, rssi=%lds [packets=%c, channel=%c], topic-prefix=%s", interval_stat, interval_rssi, capture_rssi_packet ? 'y' : 'n', capture_rssi_channel ? 'y' : 'n', topic_prefix);
    if (mesh.enabled)
        printf(", mesh=on, beacon=%lds", mesh.beacon_interval);
    printf(")\n");

    /* report known variants */
    for (int i = 0; i < IOTDATA_VARIANT_MAPS_COUNT; i++) {
        const iotdata_variant_def_t *vdef = iotdata_get_variant((uint8_t)i);
        printf("read-and-publish: variant[%d] = \"%s\" (pres_bytes=%d) -> %s/%s/<station_id>\n", i, vdef->name, vdef->num_pres_bytes, topic_prefix, vdef->name);
    }
    if (mesh.enabled)
        printf("read-and-publish: variant[15] = mesh control (gateway station_id=%u)\n", mesh.station_id);

    while (*running) {

        unsigned char packet_rssi = 0, channel_rssi = 0;
        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &packet_rssi) && *running) {
            uint8_t variant_id;
            uint16_t station_id, sequence;
            if (!iotdata_mesh_peek_header(packet_buffer, packet_size, &variant_id, &station_id, &sequence)) {
                fprintf(stderr, "read-and-publish: packet too short for iotdata header (size=%d)\n", packet_size);
                stat_packets_drop++;
            } else if (variant_id == IOTDATA_MESH_VARIANT) {
                const uint8_t ctrl_type = iotdata_mesh_peek_ctrl_type(packet_buffer, packet_size);
                mesh.stat_mesh_ctrl_rx++;
                if (debug_mesh)
                    printf("mesh: rx %s from station=%u seq=%u (%d bytes)\n", iotdata_mesh_ctrl_name(ctrl_type), station_id, sequence, packet_size);
                switch (ctrl_type) {
                case IOTDATA_MESH_CTRL_FORWARD: {
                    const unsigned char *inner;
                    int inner_len;
                    if (mesh_handle_forward(packet_buffer, packet_size, &inner, &inner_len))
                        process_sensor_packet(inner, inner_len, 0, topic_prefix, "mesh");
                    break;
                }
                case IOTDATA_MESH_CTRL_BEACON:
                    mesh_handle_beacon(packet_buffer, packet_size);
                    break;
                case IOTDATA_MESH_CTRL_ACK:
                    if (debug_mesh)
                        printf("mesh: rx unexpected ACK from station=%u\n", station_id);
                    break;
                case IOTDATA_MESH_CTRL_ROUTE_ERROR:
                    mesh_handle_route_error(packet_buffer, packet_size);
                    break;
                case IOTDATA_MESH_CTRL_NEIGHBOUR_RPT:
                    mesh_handle_neighbour_report(packet_buffer, packet_size);
                    break;
                case IOTDATA_MESH_CTRL_PONG:
                    mesh_handle_pong(packet_buffer, packet_size);
                    break;
                default:
                    mesh.stat_mesh_unknown++;
                    if (debug_mesh)
                        printf("mesh: rx unknown ctrl_type=0x%X from station=%u\n", ctrl_type, station_id);
                    break;
                }
            } else {
                process_sensor_packet(packet_buffer, packet_size, packet_rssi, topic_prefix, NULL);
            }
        }

        if (*running && mesh.enabled && intervalable(mesh.beacon_interval, &mesh.beacon_last))
            mesh_beacon_send();

        if (*running && capture_rssi_channel && intervalable(interval_rssi, &interval_rssi_last))
            if (device_channel_rssi_read(&channel_rssi) && *running)
                ema_update(channel_rssi, &stat_channel_rssi_ema, &stat_channel_rssi_cnt);

        time_t period_stat;
        if (*running && (period_stat = intervalable(interval_stat, &interval_stat_last))) {
            printf("packets-okay=%ld (%.2f/min), packets-drop=%ld, decode-err=%ld", stat_packets_okay, ((float)stat_packets_okay / ((float)period_stat / 60.0f)), stat_packets_drop, stat_packets_decode_err);
            stat_packets_okay = stat_packets_drop = stat_packets_decode_err = 0;
            if (capture_rssi_channel)
                printf(", channel-rssi=%d dBm (%ld)", get_rssi_dbm(stat_channel_rssi_ema), stat_channel_rssi_cnt);
            if (capture_rssi_packet)
                printf(", packet-rssi=%d dBm (%ld)", get_rssi_dbm(stat_packet_rssi_ema), stat_packet_rssi_cnt);
            if (mesh.enabled)
                printf(", mesh{fwd=%ld, unwrap=%ld, dedup=%ld, beacons=%ld, acks=%ld, ctrl=%ld}", mesh.stat_forwards_rx, mesh.stat_forwards_unwrapped, mesh.stat_duplicates, mesh.stat_beacons_tx, mesh.stat_acks_tx, mesh.stat_mesh_ctrl_rx);
            printf("\n");
            if (mesh.enabled) {
                mesh.stat_forwards_rx = mesh.stat_forwards_unwrapped = 0;
                mesh.stat_duplicates = mesh.stat_acks_tx = 0;
                mesh.stat_mesh_ctrl_rx = mesh.stat_mesh_unknown = 0;
            }
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

serial_config_t serial_config;
e22900t22_config_t e22900t22u_config;
mqtt_config_t mqtt_config;
const char *mqtt_topic_prefix;

bool config_setup(const int argc, char *argv[]) {

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);
    config_populate_mqtt(&mqtt_config);

    mqtt_topic_prefix = config_get_string("mqtt-topic-prefix", MQTT_TOPIC_PREFIX_DEFAULT);
    capture_rssi_packet = config_get_bool("rssi-packet", CONFIG_RSSI_PACKET_DEFAULT);
    capture_rssi_channel = config_get_bool("rssi-channel", CONFIG_RSSI_CHANNEL_DEFAULT);
    interval_stat = config_get_integer("interval-stat", INTERVAL_STAT_DEFAULT);
    interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);

    debug_readandsend = config_get_bool("debug", false);
    debug_e22900t22u = config_get_bool("debug-e22900t22u", false);
    debug_mesh = config_get_bool("debug-mesh", false);

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool running = true;

void signal_handler(const int sig __attribute__((unused))) {
    if (running) {
        printf("stopping\n");
        running = false;
    }
}

int main(int argc, char *argv[]) {

    setbuf(stdout, NULL);
    printf("starting (iotdata gateway, variants=%d, mesh=available)\n", IOTDATA_VARIANT_MAPS_COUNT);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_setup(argc, argv))
        return EXIT_FAILURE;

    if (!serial_begin(&serial_config) || !serial_connect()) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
        return EXIT_FAILURE;
    }
    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config)) {
        serial_end();
        return EXIT_FAILURE;
    }
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer())) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    if (!mqtt_begin(&mqtt_config)) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    mesh_begin();
    read_and_send(&running, mqtt_topic_prefix);

    device_disconnect();
    serial_end();
    mqtt_end();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
