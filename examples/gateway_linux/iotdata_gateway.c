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

#include "serial_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool debug_e22900t22u = false;

void printf_debug_e22900t22u(const char *format, ...) {
    if (!debug_e22900t22u)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
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
	//
    {"port",                  required_argument, 0, 0},
    {"rate",                  required_argument, 0, 0},
    {"bits",                  required_argument, 0, 0},
    {"address",               required_argument, 0, 0},
    {"network",               required_argument, 0, 0},
    {"channel",               required_argument, 0, 0},
    {"packet-size",           required_argument, 0, 0},
    {"packet-rate",           required_argument, 0, 0},
    {"rssi-packet",           required_argument, 0, 0},
    {"rssi-channel",          required_argument, 0, 0},
    {"listen-before-transmit",required_argument, 0, 0},
    {"read-timeout-command",  required_argument, 0, 0},
    {"read-timeout-packet",   required_argument, 0, 0},
    {"interval-stat",         required_argument, 0, 0},
    {"interval-rssi",         required_argument, 0, 0},
    {"debug-e22900t22u",      required_argument, 0, 0},
	//
    {"mqtt-client",           required_argument, 0, 0},
    {"mqtt-server",           required_argument, 0, 0},
    {"mqtt-topic-prefix",     required_argument, 0, 0},
    {"mqtt-tls-insecure",     required_argument, 0, 0},
    {"mqtt-reconnect-delay",  required_argument, 0, 0},
    {"mqtt-reconnect-delay-max", required_argument, 0, 0},
	//
    {"mesh-enable",           required_argument, 0, 0},
    {"mesh-station-id",       required_argument, 0, 0},
    {"mesh-beacon-interval",  required_argument, 0, 0},
    {"debug-mesh",            required_argument, 0, 0},
	//
    {"dedup-enable",             required_argument, 0, 0},
    {"dedup-port",               required_argument, 0, 0},
    {"dedup-peers",              required_argument, 0, 0},
    {"dedup-delay",              required_argument, 0, 0},
    {"debug-dedup",              required_argument, 0, 0},
	//
    {"debug",                 required_argument, 0, 0},
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
    cfg->transmit_power = E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT;
    cfg->transmission_method = E22900T22_CONFIG_TRANSMISSION_METHOD_DEFAULT;
    cfg->relay_enabled = E22900T22_CONFIG_RELAY_ENABLED_DEFAULT;
    cfg->listen_before_transmit = config_get_bool("listen-before-transmit", E22900T22_CONFIG_LISTEN_BEFORE_TRANSMIT);
    cfg->rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    cfg->rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    cfg->read_timeout_command = (uint32_t)config_get_integer("read-timeout-command", E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    cfg->read_timeout_packet = (uint32_t)config_get_integer("read-timeout-packet", E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    cfg->debug = config_get_bool("debug-e22900t22u", false);
    debug_e22900t22u = cfg->debug;

    printf("config: e22900t22u: address=0x%04" PRIX16 ", network=0x%02" PRIX8 ", channel=%d, packet-size=%d, packet-rate=%d, rssi-channel=%s, rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%" PRIu32
           ", read-timeout-packet=%" PRIu32 ", crypt=%04" PRIX16 ", transmit-power=%" PRIu8 ", transmission-method=%s, mode-relay=%s, debug=%s\n",
           cfg->address, cfg->network, cfg->channel, cfg->packet_maxsize, cfg->packet_maxrate, cfg->rssi_channel ? "on" : "off", cfg->rssi_packet ? "on" : "off", cfg->listen_before_transmit ? "on" : "off", cfg->read_timeout_command,
           cfg->read_timeout_packet, cfg->crypt, cfg->transmit_power, cfg->transmission_method == E22900T22_CONFIG_TRANSMISSION_METHOD_TRANSPARENT ? "transparent" : "fixed-point", cfg->relay_enabled ? "on" : "off",
           cfg->debug ? "on" : "off");
}

void config_populate_mqtt(mqtt_config_t *cfg) {
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

struct {
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
    bool debug;
} mesh_state;

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEDUP_PORT_DEFAULT     9876
#define DEDUP_DELAY_MS_DEFAULT 20
#define DEDUP_PEERS_MAX        16
#define DEDUP_PENDING_MAX      256
#define DEDUP_BATCH_MAX        32
#define DEDUP_BUF_SIZE         (3 + DEDUP_BATCH_MAX * 4) /* 131 bytes */

typedef struct {
    char host[128];
    uint16_t port;
    struct sockaddr_in addr;
    bool resolved;
} dedup_peer_t;

struct {
    bool enabled;
    uint16_t port;
    uint32_t delay_ms;
    dedup_peer_t peers[DEDUP_PEERS_MAX];
    int peers_count;
    pthread_mutex_t mutex;
    pthread_t thread;
    iotdata_mesh_dedup_entry_t pending[DEDUP_PENDING_MAX];
    int pending_count;
    bool pending_has_new;
    struct timespec pending_first;
    /* statistics */
    uint32_t stat_sends;
    uint32_t stat_entries_sent;
    uint32_t stat_recvs;
    uint32_t stat_entries_recv;
    uint32_t stat_injected;
    bool debug;
} dedup_state;

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_and_add(uint16_t station_id, uint16_t sequence) {
    if (!dedup_state.enabled)
        return iotdata_mesh_dedup_check_and_add(&mesh_state.dedup, station_id, sequence);

    bool is_new;
    pthread_mutex_lock(&dedup_state.mutex);
    is_new = iotdata_mesh_dedup_check_and_add(&mesh_state.dedup, station_id, sequence);
    if (is_new && dedup_state.pending_count < DEDUP_PENDING_MAX) {
        dedup_state.pending[dedup_state.pending_count].station_id = station_id;
        dedup_state.pending[dedup_state.pending_count].sequence = sequence;
        dedup_state.pending_count++;
        if (!dedup_state.pending_has_new) {
            dedup_state.pending_has_new = true;
            clock_gettime(CLOCK_MONOTONIC, &dedup_state.pending_first);
        }
    }
    pthread_mutex_unlock(&dedup_state.mutex);
    return is_new;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_peers_parse(const char *peers_str) {
    if (!peers_str || !*peers_str)
        return;
    char buf[512];
    strncpy(buf, peers_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = NULL, *tok = strtok_r(buf, ",", &save);
    while (tok && dedup_state.peers_count < DEDUP_PEERS_MAX) {
        while (*tok == ' ')
            tok++;
        char *colon = strrchr(tok, ':');
        uint16_t pport = dedup_state.port;
        if (colon) {
            *colon = '\0';
            pport = (uint16_t)atoi(colon + 1);
        }
        strncpy(dedup_state.peers[dedup_state.peers_count].host, tok, sizeof(dedup_state.peers[0].host) - 1);
        dedup_state.peers[dedup_state.peers_count].host[sizeof(dedup_state.peers[0].host) - 1] = '\0';
        dedup_state.peers[dedup_state.peers_count].port = pport;
        dedup_state.peers_count++;
        tok = strtok_r(NULL, ",", &save);
    }
}

void dedup_peers_resolve(void) {
    for (int i = 0; i < dedup_state.peers_count; i++) {
        struct addrinfo hints = { 0 }, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%" PRIu16, dedup_state.peers[i].port);
        const int err = getaddrinfo(dedup_state.peers[i].host, port_str, &hints, &res);
        if (err == 0) {
            memset(&dedup_state.peers[i].addr, 0, sizeof(dedup_state.peers[i].addr));
            memcpy(&dedup_state.peers[i].addr, res->ai_addr, sizeof(dedup_state.peers[i].addr));
            dedup_state.peers[i].resolved = true;
            freeaddrinfo(res);
            printf("dedup: peer[%d] %s:%" PRIu16 " resolved\n", i, dedup_state.peers[i].host, dedup_state.peers[i].port);
        } else {
            dedup_state.peers[i].resolved = false;
            fprintf(stderr, "dedup: peer[%d] %s:%" PRIu16 " resolution failed: %s\n", i, dedup_state.peers[i].host, dedup_state.peers[i].port, gai_strerror(err));
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEDUP_MIN(a, b) ((a) < (b) ? (a) : (b))

void *dedup_thread_func(void *arg) {
    (void)arg;

    const int recv_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_fd < 0) {
        fprintf(stderr, "dedup: recv socket: %s\n", strerror(errno));
        return NULL;
    }
    int optval = 1;
    setsockopt(recv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof(optval));
    struct sockaddr_in bind_addr = { 0 };
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(dedup_state.port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(recv_fd, (struct sockaddr *)&bind_addr, (socklen_t)sizeof(bind_addr)) < 0) {
        fprintf(stderr, "dedup: bind port %" PRIu16 ": %s\n", dedup_state.port, strerror(errno));
        close(recv_fd);
        return NULL;
    }

    const int send_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_fd < 0) {
        fprintf(stderr, "dedup: send socket: %s\n", strerror(errno));
        close(recv_fd);
        return NULL;
    }

    printf("dedup: thread started\n");

    iotdata_mesh_dedup_entry_t send_buf[DEDUP_PENDING_MAX];
    while (running) {
        struct pollfd pfd;
        pfd.fd = recv_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 5) > 0 && (pfd.revents & POLLIN)) {
            uint8_t buf[DEDUP_BUF_SIZE];
            struct sockaddr_in from;
            socklen_t from_len = (socklen_t)sizeof(from);
            const ssize_t n = recvfrom(recv_fd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len);
            if (n >= 3) {
                const uint16_t gw_id = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
                const int num_entries = DEDUP_MIN(buf[2], DEDUP_BATCH_MAX);
                if (n >= (ssize_t)(3 + num_entries * 4)) {
                    pthread_mutex_lock(&dedup_state.mutex);
                    for (int i = 0; i < num_entries; i++) {
                        const uint16_t sid = (uint16_t)(((uint16_t)buf[(3 + i * 4) + 0] << 8) | (uint16_t)buf[(3 + i * 4) + 1]);
                        const uint16_t seq = (uint16_t)(((uint16_t)buf[(3 + i * 4) + 2] << 8) | (uint16_t)buf[(3 + i * 4) + 3]);
                        iotdata_mesh_dedup_check_and_add(&mesh_state.dedup, sid, seq);
                        dedup_state.stat_injected++;
                    }
                    pthread_mutex_unlock(&dedup_state.mutex);
                    dedup_state.stat_recvs++;
                    dedup_state.stat_entries_recv += (uint32_t)num_entries;
                    if (dedup_state.debug)
                        printf("dedup: rx from gw=%" PRIu16 " entries=%d\n", gw_id, num_entries);
                }
            }
        }

        /* check whether pending entries have aged past the coalesce delay */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        bool send_needed = false;
        int send_count = 0;
        pthread_mutex_lock(&dedup_state.mutex);
        if (dedup_state.pending_has_new && dedup_state.pending_count > 0) {
            const long elapsed_ms = (long)(now.tv_sec - dedup_state.pending_first.tv_sec) * 1000L + (long)((now.tv_nsec - dedup_state.pending_first.tv_nsec) / 1000000L);
            if (elapsed_ms >= (long)dedup_state.delay_ms) {
                send_count = dedup_state.pending_count;
                memcpy(send_buf, dedup_state.pending, (size_t)send_count * sizeof(iotdata_mesh_dedup_entry_t));
                dedup_state.pending_count = 0;
                dedup_state.pending_has_new = false;
                send_needed = true;
            }
        }
        pthread_mutex_unlock(&dedup_state.mutex);

        if (send_needed && dedup_state.peers_count > 0) {
            int offset = 0;
            while (offset < send_count) {
                const int num_entries = DEDUP_MIN(send_count - offset, DEDUP_BATCH_MAX);
                uint8_t pkt[DEDUP_BUF_SIZE];
                pkt[0] = (uint8_t)(mesh_state.station_id >> 8);
                pkt[1] = (uint8_t)(mesh_state.station_id & 0xFFu);
                pkt[2] = (uint8_t)num_entries;
                for (int i = 0; i < num_entries; i++) {
                    pkt[(3 + i * 4) + 0] = (uint8_t)(send_buf[offset + i].station_id >> 8);
                    pkt[(3 + i * 4) + 1] = (uint8_t)(send_buf[offset + i].station_id & 0xFFu);
                    pkt[(3 + i * 4) + 2] = (uint8_t)(send_buf[offset + i].sequence >> 8);
                    pkt[(3 + i * 4) + 3] = (uint8_t)(send_buf[offset + i].sequence & 0xFFu);
                }
                const size_t pkt_len = (size_t)(3 + num_entries * 4);
                for (int p = 0; p < dedup_state.peers_count; p++)
                    if (dedup_state.peers[p].resolved)
                        (void)sendto(send_fd, pkt, pkt_len, 0, (const struct sockaddr *)&dedup_state.peers[p].addr, (socklen_t)sizeof(dedup_state.peers[p].addr));
                dedup_state.stat_sends++;
                dedup_state.stat_entries_sent += (uint32_t)num_entries;
                offset += num_entries;
            }
            if (dedup_state.debug)
                printf("dedup: tx %d entries to %d peers\n", send_count, dedup_state.peers_count);
        }
    }

    close(recv_fd);
    close(send_fd);
    return NULL;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void config_populate_dedup(void) {
    memset(&dedup_state, 0, sizeof(dedup_state));
    dedup_state.enabled = config_get_bool("dedup-enable", false);
    dedup_state.port = (uint16_t)config_get_integer("dedup-port", DEDUP_PORT_DEFAULT);
    dedup_state.delay_ms = (uint32_t)config_get_integer("dedup-delay", DEDUP_DELAY_MS_DEFAULT);
    const char *peers = config_get_string("dedup-peers", "");
    dedup_peers_parse(peers);
    dedup_state.debug = config_get_bool("debug-dedup", false);

    printf("config: dedup: enabled=%c, port=%" PRIu16 ", peers=%s, delay=%" PRIu32 "ms, debug=%s\n", dedup_state.enabled ? 'y' : 'n', dedup_state.port, peers, dedup_state.delay_ms, dedup_state.debug ? "on" : "off");
}

bool dedup_begin(void) {
    if (!dedup_state.enabled) {
        printf("dedup: disabled\n");
        return true;
    }

    printf("dedup: enabled, port=%" PRIu16 ", peers=%d, delay=%" PRIu32 "ms\n", dedup_state.port, dedup_state.peers_count, dedup_state.delay_ms);

    pthread_mutex_init(&dedup_state.mutex, NULL);
    dedup_peers_resolve();
    if (pthread_create(&dedup_state.thread, NULL, dedup_thread_func, NULL) != 0) {
        dedup_state.enabled = false;
        fprintf(stderr, "dedup: thread create failed: %s\n", strerror(errno));
        pthread_mutex_destroy(&dedup_state.mutex);
        return false;
    }

    return true;
}

void dedup_end(void) {
    if (!dedup_state.enabled)
        return;
    pthread_join(dedup_state.thread, NULL);
    pthread_mutex_destroy(&dedup_state.mutex);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void config_populate_mesh(void) {
    memset(&mesh_state, 0, sizeof(mesh_state));
    mesh_state.enabled = config_get_bool("mesh-enable", false);
    mesh_state.station_id = (uint16_t)config_get_integer("mesh-station-id", GATEWAY_STATION_ID_DEFAULT);
    mesh_state.beacon_interval = (time_t)config_get_integer("mesh-beacon-interval", INTERVAL_BEACON_DEFAULT);
    mesh_state.debug = config_get_bool("debug-mesh", false);

    printf("config: mesh: enabled=%c, station-id=%04X " PRIX16 ", beacon-interval=%" PRIu32 ", debug=%s\n", mesh_state.enabled ? 'y' : 'n', mesh_state.station_id, (uint32_t)mesh_state.beacon_interval, dedup_state.debug ? "on" : "off");
}

bool mesh_begin(void) {
    if (!mesh_state.enabled) {
        printf("mesh: disabled\n");
        return true;
    }
    iotdata_mesh_dedup_init(&mesh_state.dedup);
    printf("mesh: enabled, station-id=%04X" PRIX16 ", beacon-interval=%" PRIu32 "s\n", mesh_state.station_id, (uint32_t)mesh_state.beacon_interval);
    return true;
}

void mesh_end(void) {
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_beacon_send(void) {
    uint8_t buf[IOTDATA_MESH_BEACON_SIZE];
    const iotdata_mesh_beacon_t beacon = {
        .sender_station = mesh_state.station_id,
        .sender_seq = mesh_state.mesh_seq++,
        .gateway_id = mesh_state.station_id,
        .cost = 0,
        .flags = IOTDATA_MESH_FLAG_ACCEPTING,
        .generation = mesh_state.beacon_generation++,
    };
    mesh_state.beacon_generation &= (IOTDATA_MESH_GENERATION_MOD - 1);
    iotdata_mesh_pack_beacon(buf, &beacon);
    if (mesh_state.debug)
        printf("mesh: tx BEACON gen=%" PRIu16 " station=%" PRIu16 "\n", beacon.generation, beacon.sender_station);
    if (device_packet_write(buf, IOTDATA_MESH_BEACON_SIZE))
        mesh_state.stat_beacons_tx++;
    else
        fprintf(stderr, "mesh: beacon tx failed\n");
}

void mesh_ack_send(uint16_t fwd_station, uint16_t fwd_seq) {
    uint8_t buf[IOTDATA_MESH_ACK_SIZE];
    const iotdata_mesh_ack_t ack = {
        .sender_station = mesh_state.station_id,
        .sender_seq = mesh_state.mesh_seq++,
        .fwd_station = fwd_station,
        .fwd_seq = fwd_seq,
    };
    iotdata_mesh_pack_ack(buf, &ack);
    if (mesh_state.debug)
        printf("mesh: tx ACK to station=%" PRIu16 " seq=%" PRIu16 "\n", fwd_station, fwd_seq);
    if (device_packet_write(buf, IOTDATA_MESH_ACK_SIZE))
        mesh_state.stat_acks_tx++;
    else
        fprintf(stderr, "mesh: ack tx failed\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool mesh_handle_forward(const uint8_t *buf, int len, const uint8_t **inner, int *inner_len) {
    iotdata_mesh_forward_t fwd;
    if (!iotdata_mesh_unpack_forward(buf, len, &fwd)) {
        fprintf(stderr, "mesh: FORWARD unpack failed (len=%d)\n", len);
        return false;
    }
    mesh_state.stat_forwards_rx++;
    if (mesh_state.debug)
        printf("mesh: rx FORWARD from station=%" PRIu16 " seq=%" PRIu16 " ttl=%" PRIu8 " origin={station=%" PRIu16 ", seq=%" PRIu16 "} inner=%d B\n", fwd.sender_station, fwd.sender_seq, fwd.ttl, fwd.origin_station, fwd.origin_sequence,
               fwd.inner_len);
    if (!dedup_check_and_add(fwd.origin_station, fwd.origin_sequence)) {
        mesh_state.stat_duplicates++;
        if (mesh_state.debug)
            printf("mesh: FORWARD duplicate suppressed (origin station=%" PRIu16 " seq=%" PRIu16 ")\n", fwd.origin_station, fwd.origin_sequence);
        /* still ACK to prevent the forwarder from retrying */
        if (mesh_state.enabled)
            mesh_ack_send(fwd.sender_station, fwd.sender_seq);
        return false;
    }
    /* ACK the forwarder */
    if (mesh_state.enabled)
        mesh_ack_send(fwd.sender_station, fwd.sender_seq);
    mesh_state.stat_forwards_unwrapped++;
    *inner = fwd.inner_packet;
    *inner_len = fwd.inner_len;
    return true;
}

void mesh_handle_beacon(const uint8_t *buf, int len) {
    /* gateway receiving another gateway's beacon — log for multi-gateway awareness */
    iotdata_mesh_beacon_t b;
    if (iotdata_mesh_unpack_beacon(buf, len, &b))
        if (mesh_state.debug)
            printf("mesh: rx BEACON from gateway=%" PRIu16 " gen=%" PRIu16 " cost=%" PRIu8 " flags=0x%" PRIX8 "\n", b.gateway_id, b.generation, b.cost, b.flags);
}

void mesh_handle_route_error(const uint8_t *buf, int len) {
    iotdata_mesh_route_error_t err;
    if (iotdata_mesh_unpack_route_error(buf, len, &err))
        printf("mesh: rx ROUTE_ERROR from station=%" PRIu16 " reason=%s\n", err.sender_station, iotdata_mesh_reason_name(err.reason));
}

void mesh_handle_neighbour_report(const uint8_t *buf, int len) {
    /* full topology aggregation is future work — log receipt for now */
    uint8_t variant;
    uint16_t station_id, sequence;
    if (iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence))
        printf("mesh rx NEIGHBOUR_REPORT from station=%" PRIu16 " (%d bytes)\n", station_id, len);
}

void mesh_handle_pong(const uint8_t *buf, int len) {
    uint8_t variant;
    uint16_t station_id, sequence;
    if (iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence))
        printf("mesh: rx PONG from station=%" PRIu16 " (%d bytes)\n", station_id, len);
}

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
    uint32_t stat_channel_rssi_cnt;
    uint32_t stat_packet_rssi_cnt;
    uint8_t stat_channel_rssi_ema;
    uint8_t stat_packet_rssi_ema;
    uint32_t stat_packets_okay;
    uint32_t stat_packets_drop;
    uint32_t stat_packets_decode_err;
    bool debug;
} process_state;

void config_populate_process(void) {
    memset(&process_state, 0, sizeof(process_state));
    process_state.mqtt_topic_prefix = config_get_string("mqtt-topic-prefix", MQTT_TOPIC_PREFIX_DEFAULT);
    process_state.capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    process_state.capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    process_state.interval_stat = config_get_integer("interval-stat", INTERVAL_STAT_DEFAULT);
    process_state.interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);
    process_state.debug = config_get_bool("debug", false);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void process_sensor_packet(const uint8_t *packet_buffer, int packet_length, uint8_t variant_id, uint16_t station_id, uint16_t sequence, const char *topic_prefix, const char *via) {
    if (via == NULL && mesh_state.enabled)
        if (!dedup_check_and_add(station_id, sequence)) {
            mesh_state.stat_duplicates++;
            if (mesh_state.debug)
                printf("mesh: direct packet duplicate suppressed (station=%" PRIu16 " seq=%" PRIu16 ")\n", station_id, sequence);
            return;
        }
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant_id);
    if (vdef == NULL) {
        fprintf(stderr, "process: unknown variant %" PRIu8 " (station=%" PRIu16 ", size=%d)\n", variant_id, station_id, packet_length);
        process_state.stat_packets_drop++;
        return;
    }
    char *json = NULL;
    iotdata_decode_to_json_scratch_t scratch;
    iotdata_status_t rc;
    if ((rc = iotdata_decode_to_json(packet_buffer, (size_t)packet_length, &json, &scratch)) != IOTDATA_OK) {
        fprintf(stderr, "process: decode failed: %s (variant=%" PRIu8 ", station=%" PRIu16 ", size=%d)\n", iotdata_strerror(rc), variant_id, station_id, packet_length);
        process_state.stat_packets_decode_err++;
        return;
    }
    char topic[255];
    snprintf(topic, sizeof(topic), "%s/%s/%" PRIu16, topic_prefix, vdef->name, station_id);
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

void process_mesh_packet(const uint8_t *packet_buffer, int packet_length, uint16_t station_id, uint16_t sequence, const char *topic_prefix) {
    const uint8_t ctrl_type = iotdata_mesh_peek_ctrl_type(packet_buffer, packet_length);
    mesh_state.stat_mesh_ctrl_rx++;
    if (mesh_state.debug)
        printf("mesh: rx %s from station=%" PRIu16 " seq=%" PRIu16 " (%d bytes)\n", iotdata_mesh_ctrl_name(ctrl_type), station_id, sequence, packet_length);
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
            } else
                process_sensor_packet(inner, inner_len, inner_variant, inner_station, inner_sequence, topic_prefix, "mesh");
        }
        break;
    }
    case IOTDATA_MESH_CTRL_BEACON:
        mesh_handle_beacon(packet_buffer, packet_length);
        break;
    case IOTDATA_MESH_CTRL_ACK:
        if (mesh_state.debug)
            printf("mesh: rx unexpected ACK from station=%" PRIu16 "\n", station_id);
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
            printf("mesh: rx unknown ctrl_type=0x%" PRIX8 " from station=%" PRIu16 "\n", ctrl_type, station_id);
        break;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void process_stats(time_t period_stat) {
    const uint32_t rate_okay = (process_state.stat_packets_okay * 6000) / (uint32_t)period_stat, rate_drop = (process_state.stat_packets_drop * 6000) / (uint32_t)period_stat;
    printf("packets{okay=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min), drop=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min)}", process_state.stat_packets_okay, rate_okay / 100, rate_okay % 100, process_state.stat_packets_drop, rate_drop / 100,
           rate_drop % 100);
    process_state.stat_packets_okay = process_state.stat_packets_drop = process_state.stat_packets_decode_err = 0;
    if (process_state.capture_rssi_channel || process_state.capture_rssi_packet) {
        printf(", rssi{");
        if (process_state.capture_rssi_channel)
            printf("channel=%d dBm (%" PRIu32 ")", get_rssi_dbm(process_state.stat_channel_rssi_ema), process_state.stat_channel_rssi_cnt);
        if (process_state.capture_rssi_channel && process_state.capture_rssi_packet)
            printf(", ");
        if (process_state.capture_rssi_packet)
            printf("packet=%d dBm (%" PRIu32 ")", get_rssi_dbm(process_state.stat_packet_rssi_ema), process_state.stat_packet_rssi_cnt);
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
        printf(", dedup{sends=%" PRIu32 "/%" PRIu32 ", recvs=%" PRIu32 "/%" PRIu32 ", injected=%" PRIu32 "}", dedup_state.stat_sends, dedup_state.stat_entries_sent, dedup_state.stat_recvs, dedup_state.stat_entries_recv,
               dedup_state.stat_injected);
        dedup_state.stat_sends = dedup_state.stat_entries_sent = 0;
        dedup_state.stat_recvs = dedup_state.stat_entries_recv = 0;
        dedup_state.stat_injected = 0;
    }
    printf(", mqtt{%s, disconnects=%" PRIu32 "}", mqtt_is_connected() ? "up" : "down", mqtt_stat_disconnects);
    printf("\n");
}

void process_begin(void) {
    uint8_t packet_buffer[E22900T22_PACKET_MAXSIZE + 1]; /* +1 for RSSI byte */
    int packet_length;

    printf("process: iotdata gateway (stat=%" PRIu32 "s, rssi=%" PRIu32 "s [packets=%c, channel=%c], topic-prefix=%s", (uint32_t)process_state.interval_stat, (uint32_t)process_state.interval_rssi,
           process_state.capture_rssi_packet ? 'y' : 'n', process_state.capture_rssi_channel ? 'y' : 'n', process_state.mqtt_topic_prefix);
    if (mesh_state.enabled)
        printf(", mesh=on, beacon=%" PRIu32 "s", (uint32_t)mesh_state.beacon_interval);
    printf(")\n");

    for (int i = 0; i < IOTDATA_VARIANT_MAPS_COUNT; i++) {
        const iotdata_variant_def_t *vdef = iotdata_get_variant((uint8_t)i);
        printf("process: variant[%d] = \"%s\" (pres_bytes=%" PRIu8 ") -> %s/%s/<station_id>\n", i, vdef->name, vdef->num_pres_bytes, process_state.mqtt_topic_prefix, vdef->name);
    }
    if (mesh_state.enabled)
        printf("process: variant[15] = mesh control (gateway station_id=%" PRIu16 ")\n", mesh_state.station_id);

    while (running) {

        // packet processing
        uint8_t packet_rssi = 0, channel_rssi = 0;
        if (device_packet_read(packet_buffer, sizeof(packet_buffer), &packet_length, &packet_rssi) && running) {
            if (process_state.capture_rssi_packet && packet_rssi > 0)
                ema_update(packet_rssi, &process_state.stat_packet_rssi_ema, &process_state.stat_packet_rssi_cnt);
            uint8_t variant_id;
            uint16_t station_id, sequence;
            if (iotdata_peek(packet_buffer, (size_t)packet_length, &variant_id, &station_id, &sequence) != IOTDATA_OK) {
                fprintf(stderr, "process: packet too short for iotdata header (size=%d)\n", packet_length);
                process_state.stat_packets_drop++;
            } else if (variant_id == IOTDATA_MESH_VARIANT)
                process_mesh_packet(packet_buffer, packet_length, station_id, sequence, process_state.mqtt_topic_prefix);
            else
                process_sensor_packet(packet_buffer, packet_length, variant_id, station_id, sequence, process_state.mqtt_topic_prefix, NULL);
        }

        // rssi update
        if (running && process_state.capture_rssi_channel && intervalable(process_state.interval_rssi, &process_state.interval_rssi_last))
            if (device_channel_rssi_read(&channel_rssi) && running)
                ema_update(channel_rssi, &process_state.stat_channel_rssi_ema, &process_state.stat_channel_rssi_cnt);

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
e22900t22_config_t e22900t22u_config;
mqtt_config_t mqtt_config;

bool config_setup(const int argc, char *argv[]) {

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);
    config_populate_mqtt(&mqtt_config);
    config_populate_mesh();
    config_populate_dedup();
    config_populate_process();

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
    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config))
        goto end_serial;
    printf("device: connect success (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));

    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer()))
        goto end_device;
    if (!mqtt_begin(&mqtt_config))
        goto end_device;
    if (!mesh_begin())
        goto end_mqtt;
    if (!dedup_begin())
        goto end_mesh;

    process_begin();
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
