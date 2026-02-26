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
#include <inttypes.h>
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

// 0.2 ≈ 51/256, 0.8 ≈ 205/256
#define EMA_ALPHA_NUM   51
#define EMA_ALPHA_DENOM 256
void ema_update(uint8_t value, uint8_t *value_ema, uint32_t *value_cnt) {
    if ((*value_cnt)++ == 0)
        *value_ema = value;
    else
        *value_ema = (uint8_t)((EMA_ALPHA_NUM * (uint16_t)value + (EMA_ALPHA_DENOM - EMA_ALPHA_NUM) * (uint16_t)(*value_ema)) / EMA_ALPHA_DENOM);
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
    {"mqtt-reconnect-delay",  required_argument, 0, 0},
    {"mqtt-reconnect-delay-max", required_argument, 0, 0},
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
    cfg->address = (uint16_t)config_get_integer("address", E22900T22_CONFIG_ADDRESS_DEFAULT);
    cfg->network = (uint8_t)config_get_integer("network", E22900T22_CONFIG_NETWORK_DEFAULT);
    cfg->channel = (uint8_t)config_get_integer("channel", E22900T22_CONFIG_CHANNEL_DEFAULT);
    cfg->packet_maxsize = (uint8_t)config_get_integer("packet-size", E22900T22_CONFIG_PACKET_MAXSIZE_DEFAULT);
    cfg->packet_maxrate = (uint8_t)config_get_integer("packet-rate", E22900T22_CONFIG_PACKET_MAXRATE_DEFAULT);
    cfg->crypt = E22900T22_CONFIG_CRYPT_DEFAULT;
    cfg->wor_enabled = E22900T22_CONFIG_WOR_ENABLED_DEFAULT;
    cfg->wor_cycle = E22900T22_CONFIG_WOR_CYCLE_DEFAULT;
    cfg->transmit_power = E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT;
    cfg->listen_before_transmit = config_get_bool("listen-before-transmit", E22900T22_CONFIG_LISTEN_BEFORE_TRANSMIT);
    cfg->rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    cfg->rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    cfg->read_timeout_command = (uint32_t)config_get_integer("read-timeout-command", E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    cfg->read_timeout_packet = (uint32_t)config_get_integer("read-timeout-packet", E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    cfg->debug = config_get_bool("debug", false);

    printf("config: e22900t22u: address=0x%04" PRIX16 ", network=0x%02" PRIX8 ", channel=%d, packet-size=%d, packet-rate=%d, rssi-channel=%s, rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%" PRIu32
           ", read-timeout-packet=%" PRIu32 ", crypt=%04" PRIX16 ", wor=%s, wor-cycle=%" PRIu16 "ms, transmit-power=%" PRIu8 ", debug=%s\n",
           cfg->address, cfg->network, cfg->channel, cfg->packet_maxsize, cfg->packet_maxrate, cfg->rssi_channel ? "on" : "off", cfg->rssi_packet ? "on" : "off", cfg->listen_before_transmit ? "on" : "off", cfg->read_timeout_command,
           cfg->read_timeout_packet, cfg->crypt, cfg->wor_enabled ? "on" : "off", cfg->wor_cycle, cfg->transmit_power, cfg->debug ? "on" : "off");
}

void config_populate_mqtt(mqtt_config_t *cfg) {
    cfg->client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    cfg->server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    cfg->tls_insecure = config_get_bool("mqtt-tls-insecure", MQTT_TLS_DEFAULT);
    cfg->use_synchronous = MQTT_SYNCHRONOUS_DEFAULT;
    cfg->reconnect_delay = (unsigned int)config_get_integer("mqtt-reconnect-delay", MQTT_RECONNECT_DELAY_DEFAULT);
    cfg->reconnect_delay_max = (unsigned int)config_get_integer("mqtt-reconnect-delay-max", MQTT_RECONNECT_DELAY_MAX_DEFAULT);
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
    uint32_t stat_beacons_tx;
    uint32_t stat_forwards_rx;
    uint32_t stat_forwards_unwrapped;
    uint32_t stat_duplicates;
    uint32_t stat_acks_tx;
    uint32_t stat_mesh_ctrl_rx;
    uint32_t stat_mesh_unknown;
} mesh;

static void mesh_begin(void) {
    memset(&mesh, 0, sizeof(mesh));
    mesh.enabled = config_get_bool("mesh-enable", false);
    mesh.station_id = (uint16_t)config_get_integer("mesh-station-id", GATEWAY_STATION_ID_DEFAULT);
    mesh.beacon_interval = config_get_integer("mesh-beacon-interval", INTERVAL_BEACON_DEFAULT);
    iotdata_mesh_dedup_init(&mesh.dedup);
    if (mesh.enabled)
        printf("mesh: enabled, gateway station_id=%" PRIu16 ", beacon interval=%" PRIu32 "s\n", mesh.station_id, (uint32_t)mesh.beacon_interval);
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
        printf("mesh: tx BEACON gen=%" PRIu16 " station=%" PRIu16 "\n", beacon.generation, beacon.sender_station);
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
        printf("mesh: tx ACK to station=%" PRIu16 " seq=%" PRIu16 "\n", fwd_station, fwd_seq);
    if (device_packet_write(buf, IOTDATA_MESH_ACK_SIZE))
        mesh.stat_acks_tx++;
    else
        fprintf(stderr, "mesh: ack tx failed\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool mesh_handle_forward(const uint8_t *buf, int len, const uint8_t **inner, int *inner_len) {
    iotdata_mesh_forward_t fwd;
    if (!iotdata_mesh_unpack_forward(buf, len, &fwd)) {
        fprintf(stderr, "mesh: FORWARD unpack failed (len=%d)\n", len);
        return false;
    }
    mesh.stat_forwards_rx++;
    if (debug_mesh)
        printf("mesh: rx FORWARD from station=%" PRIu16 " seq=%" PRIu16 " ttl=%" PRIu8 " origin={station=%" PRIu16 ", seq=%" PRIu16 "} inner=%d B\n", fwd.sender_station, fwd.sender_seq, fwd.ttl, fwd.origin_station, fwd.origin_sequence,
               fwd.inner_len);
    /* duplicate suppression on the ORIGINAL sensor packet */
    if (!iotdata_mesh_dedup_check_and_add(&mesh.dedup, fwd.origin_station, fwd.origin_sequence)) {
        mesh.stat_duplicates++;
        if (debug_mesh)
            printf("mesh: FORWARD duplicate suppressed (origin station=%" PRIu16 " seq=%" PRIu16 ")\n", fwd.origin_station, fwd.origin_sequence);
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

static void mesh_handle_beacon(const uint8_t *buf, int len) {
    /* gateway receiving another gateway's beacon — log for multi-gateway awareness */
    iotdata_mesh_beacon_t b;
    if (iotdata_mesh_unpack_beacon(buf, len, &b))
        if (debug_mesh)
            printf("mesh: rx BEACON from gateway=%" PRIu16 " gen=%" PRIu16 " cost=%" PRIu8 " flags=0x%" PRIX8 "\n", b.gateway_id, b.generation, b.cost, b.flags);
}

static void mesh_handle_route_error(const uint8_t *buf, int len) {
    iotdata_mesh_route_error_t err;
    if (iotdata_mesh_unpack_route_error(buf, len, &err))
        printf("mesh: rx ROUTE_ERROR from station=%" PRIu16 " reason=%s\n", err.sender_station, iotdata_mesh_reason_name(err.reason));
}

static void mesh_handle_neighbour_report(const uint8_t *buf, int len) {
    /* full topology aggregation is future work — log receipt for now */
    uint8_t variant;
    uint16_t station_id, sequence;
    iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence);
    printf("mesh rx NEIGHBOUR_REPORT from station=%" PRIu16 " (%d bytes)\n", station_id, len);
    (void)variant;
    (void)sequence;
}

static void mesh_handle_pong(const uint8_t *buf, int len) {
    uint8_t variant;
    uint16_t station_id, sequence;
    iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence);
    printf("mesh: rx PONG from station=%" PRIu16 " (%d bytes)\n", station_id, len);
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
static uint32_t stat_channel_rssi_cnt = 0, stat_packet_rssi_cnt = 0;
static uint8_t stat_channel_rssi_ema, stat_packet_rssi_ema;
static uint32_t stat_packets_okay = 0, stat_packets_drop = 0, stat_packets_decode_err = 0;

static void process_sensor_packet(const uint8_t *buf, int len, uint8_t packet_rssi, const char *topic_prefix, const char *via) {
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
                printf("mesh: direct packet duplicate suppressed (station=%" PRIu16 " seq=%" PRIu16 ")\n", station_id, sequence);
            return;
        }
    if (variant_id >= IOTDATA_VARIANT_MAPS_COUNT) {
        fprintf(stderr, "process: unknown variant %" PRIu8 " (station=%" PRIu16 ", size=%d)\n", variant_id, station_id, len);
        stat_packets_drop++;
        return;
    }
    // decode to json and publish
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant_id);
    char *json = NULL;
    iotdata_decode_to_json_scratch_t scratch;
    iotdata_status_t rc;
    if ((rc = iotdata_decode_to_json(buf, (size_t)len, &json, &scratch)) != IOTDATA_OK) {
        fprintf(stderr, "process: decode failed: %s (variant=%" PRIu8 ", station=%" PRIu16 ", size=%d)\n", iotdata_strerror(rc), variant_id, station_id, len);
        stat_packets_decode_err++;
        return;
    }
    char topic[255];
    snprintf(topic, sizeof(topic), "%s/%s/%" PRIu16, topic_prefix, vdef->name, station_id);
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

    uint8_t packet_buffer[E22900T22_PACKET_MAXSIZE + 1]; /* +1 for RSSI byte */
    int packet_size;

    printf("read-and-publish: iotdata gateway (stat=%" PRIu32 "s, rssi=%" PRIu32 "s [packets=%c, channel=%c], topic-prefix=%s", (uint32_t)interval_stat, (uint32_t)interval_rssi, capture_rssi_packet ? 'y' : 'n',
           capture_rssi_channel ? 'y' : 'n', topic_prefix);
    if (mesh.enabled)
        printf(", mesh=on, beacon=%" PRIu32 "s", (uint32_t)mesh.beacon_interval);
    printf(")\n");

    /* report known variants */
    for (int i = 0; i < IOTDATA_VARIANT_MAPS_COUNT; i++) {
        const iotdata_variant_def_t *vdef = iotdata_get_variant((uint8_t)i);
        printf("read-and-publish: variant[%d] = \"%s\" (pres_bytes=%" PRIu8 ") -> %s/%s/<station_id>\n", i, vdef->name, vdef->num_pres_bytes, topic_prefix, vdef->name);
    }
    if (mesh.enabled)
        printf("read-and-publish: variant[15] = mesh control (gateway station_id=%" PRIu16 ")\n", mesh.station_id);

    while (*running) {

        uint8_t packet_rssi = 0, channel_rssi = 0;
        if (device_packet_read(packet_buffer, sizeof(packet_buffer), &packet_size, &packet_rssi) && *running) {
            uint8_t variant_id;
            uint16_t station_id, sequence;
            if (!iotdata_mesh_peek_header(packet_buffer, packet_size, &variant_id, &station_id, &sequence)) {
                fprintf(stderr, "read-and-publish: packet too short for iotdata header (size=%d)\n", packet_size);
                stat_packets_drop++;
            } else if (variant_id == IOTDATA_MESH_VARIANT) {
                const uint8_t ctrl_type = iotdata_mesh_peek_ctrl_type(packet_buffer, packet_size);
                mesh.stat_mesh_ctrl_rx++;
                if (debug_mesh)
                    printf("mesh: rx %s from station=%" PRIu16 " seq=%" PRIu16 " (%d bytes)\n", iotdata_mesh_ctrl_name(ctrl_type), station_id, sequence, packet_size);
                switch (ctrl_type) {
                case IOTDATA_MESH_CTRL_FORWARD: {
                    const uint8_t *inner;
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
                        printf("mesh: rx unexpected ACK from station=%" PRIu16 "\n", station_id);
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
                        printf("mesh: rx unknown ctrl_type=0x%" PRIX8 " from station=%" PRIu16 "\n", ctrl_type, station_id);
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
            const uint32_t rate_okay = (stat_packets_okay * 6000) / (uint32_t)period_stat, rate_drop = (stat_packets_drop * 6000) / (uint32_t)period_stat;
            printf("packets-okay=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min), packets-drop=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min)", stat_packets_okay, rate_okay / 100, rate_okay % 100, stat_packets_drop, rate_drop / 100,
                   rate_drop % 100);
            stat_packets_okay = stat_packets_drop = stat_packets_decode_err = 0;
            if (capture_rssi_channel)
                printf(", channel-rssi=%d dBm (%" PRIu32 ")", get_rssi_dbm(stat_channel_rssi_ema), stat_channel_rssi_cnt);
            if (capture_rssi_packet)
                printf(", packet-rssi=%d dBm (%" PRIu32 ")", get_rssi_dbm(stat_packet_rssi_ema), stat_packet_rssi_cnt);
            if (mesh.enabled)
                printf(", mesh{fwd=%" PRIu32 ", unwrap=%" PRIu32 ", dedup=%" PRIu32 ", beacons=%" PRIu32 ", acks=%" PRIu32 ", ctrl=%" PRIu32 "}", mesh.stat_forwards_rx, mesh.stat_forwards_unwrapped, mesh.stat_duplicates,
                       mesh.stat_beacons_tx, mesh.stat_acks_tx, mesh.stat_mesh_ctrl_rx);
            printf(", mqtt=%s (disconnects=%" PRIu32 ")", mqtt_is_connected() ? "up" : "down", mqtt_stat_disconnects);
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
    capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
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
