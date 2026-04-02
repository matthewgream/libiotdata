
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEDUP_PORT_DEFAULT     9876

#define DEDUP_DELAY_MS_DEFAULT 20

#define DEDUP_PEERS_MAX        16
#define DEDUP_PENDING_MAX      256
#define DEDUP_BATCH_MAX        32

#define DEDUP_PKT_HEADER_SIZE  3
#define DEDUP_PKT_SIZE         (DEDUP_PKT_HEADER_SIZE + DEDUP_BATCH_MAX * 4) /* 131 bytes */

// -----------------------------------------------------------------------------------------------------------------------------------------

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
    struct timespec pending_first;
    uint16_t gateway_id;
    iotdata_mesh_dedup_ring_t* dedup_ring;
    bool debug;
    /* statistics */
    uint32_t stat_send_cycles;
    uint32_t stat_send_entries;
    uint32_t stat_recv_cycles;
    uint32_t stat_recv_entries;
    uint32_t stat_injected;
} dedup_state;

#define DEDUP_MIN(a, b) ((a) < (b) ? (a) : (b))

// -----------------------------------------------------------------------------------------------------------------------------------------

typedef uint8_t dedup_packet_t[DEDUP_PKT_SIZE];

#define dedup_packet_get_length(pkt)                      (size_t)(3 + (size_t)pkt[2] * 4)

#define dedup_packet_get_gateway_id(pkt)                  (((uint16_t)pkt[0] << 8) | (uint16_t)pkt[1])
#define dedup_packet_get_entry_count(pkt)                 (DEDUP_MIN(pkt[2], DEDUP_BATCH_MAX))
#define dedup_packet_get_entry_station(pkt, entry_index)  (((uint16_t)pkt[(3 + entry_index * 4) + 0] << 8) | (uint16_t)pkt[(3 + entry_index * 4) + 1])
#define dedup_packet_get_entry_sequence(pkt, entry_index) (((uint16_t)pkt[(3 + entry_index * 4) + 2] << 8) | (uint16_t)pkt[(3 + entry_index * 4) + 3])

#define dedup_packet_set_gateway_id(pkt, gateway_id) \
    do { \
        pkt[0] = (uint8_t)((gateway_id) >> 8); \
        pkt[1] = (uint8_t)((gateway_id) & 0xFF); \
    } while (0)
#define dedup_packet_set_entry_count(pkt, entry_count) \
    do { \
        pkt[2] = (uint8_t)(entry_count); \
    } while (0)
#define dedup_packet_set_entry_station(pkt, entry_index, station_id) \
    do { \
        pkt[(3 + entry_index * 4) + 0] = (uint8_t)((station_id) >> 8); \
        pkt[(3 + entry_index * 4) + 1] = (uint8_t)((station_id) & 0xFF); \
    } while (0)
#define dedup_packet_set_entry_sequence(pkt, entry_index, sequence) \
    do { \
        pkt[(3 + entry_index * 4) + 2] = (uint8_t)((sequence) >> 8); \
        pkt[(3 + entry_index * 4) + 3] = (uint8_t)((sequence) & 0xFF); \
    } while (0)

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

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_peers_resolve(void) {
    for (int i = 0; i < dedup_state.peers_count; i++) {
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%" PRIu16, dedup_state.peers[i].port);
        const struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
        struct addrinfo *res;
        const int err = getaddrinfo(dedup_state.peers[i].host, port_str, &hints, &res);
        if (err == 0) {
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

int dedup_recv_setup(void) {
    const int recv_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_fd < 0) {
        fprintf(stderr, "dedup: recv socket: %s\n", strerror(errno));
        return -1;
    }
    int optval = 1;
    setsockopt(recv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof(optval));
    const struct sockaddr_in bind_addr = { .sin_family = AF_INET, .sin_port = htons(dedup_state.port), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(recv_fd, (const struct sockaddr *)&bind_addr, (socklen_t)sizeof(bind_addr)) < 0) {
        fprintf(stderr, "dedup: bind port %" PRIu16 ": %s\n", dedup_state.port, strerror(errno));
        close(recv_fd);
        return -1;
    }
    return recv_fd;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_recv_from_peers(int recv_fd) {
    struct pollfd pfd = { .fd = recv_fd, .events = POLLIN, .revents = 0 };
    if (poll(&pfd, 1, 5) > 0 && (pfd.revents & POLLIN)) {
        dedup_packet_t pkt;
        struct sockaddr_in from;
        socklen_t from_len = (socklen_t)sizeof(from);
        const ssize_t recv_len = recvfrom(recv_fd, pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &from_len);
        if (recv_len >= DEDUP_PKT_HEADER_SIZE) {
            const int entry_count = dedup_packet_get_entry_count(pkt);
            if (recv_len >= (ssize_t)dedup_packet_get_length(pkt)) {
                pthread_mutex_lock(&dedup_state.mutex);
                for (int entry_index = 0; entry_index < entry_count; entry_index++) {
                    iotdata_mesh_dedup_check_and_add(dedup_state.dedup_ring, dedup_packet_get_entry_station(pkt, entry_index), dedup_packet_get_entry_sequence(pkt, entry_index));
                    dedup_state.stat_injected++;
                }
                pthread_mutex_unlock(&dedup_state.mutex);
                dedup_state.stat_recv_cycles++;
                dedup_state.stat_recv_entries += (uint32_t)entry_count;
                if (dedup_state.debug)
                    printf("dedup: rx from gateway=0x%04" PRIX16 ", entries=%d\n", dedup_packet_get_gateway_id(pkt), entry_count);
            }
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int dedup_send_setup(void) {
    const int send_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_fd < 0) {
        fprintf(stderr, "dedup: send socket: %s\n", strerror(errno));
        return -1;
    }
    return send_fd;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int dedup_send_collect(iotdata_mesh_dedup_entry_t *send_entries) {
    int send_count = 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    pthread_mutex_lock(&dedup_state.mutex);
    if (dedup_state.pending_count > 0) {
        const int32_t elapsed_ms = (int32_t)((now.tv_sec - dedup_state.pending_first.tv_sec) * 1000L) + (int32_t)((now.tv_nsec - dedup_state.pending_first.tv_nsec) / 1000000L);
        if (elapsed_ms > 0 && (uint32_t)elapsed_ms >= dedup_state.delay_ms) {
            send_count = dedup_state.pending_count;
            memcpy(send_entries, dedup_state.pending, (size_t)send_count * sizeof(iotdata_mesh_dedup_entry_t));
            dedup_state.pending_count = 0;
        }
    }
    pthread_mutex_unlock(&dedup_state.mutex);
    return send_count;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_send_to_peers(int send_fd, iotdata_mesh_dedup_entry_t *send_entries, int send_count) {
    int send_offset = 0;
    while (send_offset < send_count) {
        const int entry_count = DEDUP_MIN(send_count - send_offset, DEDUP_BATCH_MAX);
        dedup_packet_t pkt;
        dedup_packet_set_gateway_id(pkt, dedup_state.gateway_id);
        dedup_packet_set_entry_count(pkt, entry_count);
        for (int entry_index = 0; entry_index < entry_count; entry_index++) {
            dedup_packet_set_entry_station(pkt, entry_index, send_entries[send_offset + entry_index].station_id);
            dedup_packet_set_entry_sequence(pkt, entry_index, send_entries[send_offset + entry_index].sequence);
        }
        const size_t pkt_len = dedup_packet_get_length(pkt);
        for (int peer = 0; peer < dedup_state.peers_count; peer++)
            if (dedup_state.peers[peer].resolved)
                (void)sendto(send_fd, pkt, pkt_len, 0, (const struct sockaddr *)&dedup_state.peers[peer].addr, (socklen_t)sizeof(dedup_state.peers[peer].addr));
        dedup_state.stat_send_cycles++;
        dedup_state.stat_send_entries += (uint32_t)entry_count;
        send_offset += entry_count;
    }
    if (dedup_state.debug)
        printf("dedup: tx %d entries to %d peers\n", send_count, dedup_state.peers_count);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void *dedup_thread_func(void *arg) {
    (void)arg;

    int recv_fd, send_fd;
    if ((recv_fd = dedup_recv_setup()) < 0)
        goto dedup_end_all;
    if ((send_fd = dedup_send_setup()) < 0)
        goto dedup_end_send;

    iotdata_mesh_dedup_entry_t send_entries[DEDUP_PENDING_MAX];
    while (running) {
        dedup_recv_from_peers(recv_fd);
        if (dedup_state.peers_count > 0) {
            const int send_count = dedup_send_collect(send_entries);
            if (send_count > 0)
                dedup_send_to_peers(send_fd, send_entries, send_count);
        }
    }

    close(send_fd);
dedup_end_send:
    close(recv_fd);
dedup_end_all:
    return NULL;
}


// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_and_add(uint16_t station_id, uint16_t sequence) {
    if (!dedup_state.enabled)
        return iotdata_mesh_dedup_check_and_add(dedup_state.dedup_ring, station_id, sequence);
    bool is_new;
    pthread_mutex_lock(&dedup_state.mutex);
    is_new = iotdata_mesh_dedup_check_and_add(dedup_state.dedup_ring, station_id, sequence);
    if (is_new && dedup_state.pending_count < DEDUP_PENDING_MAX) {
        dedup_state.pending[dedup_state.pending_count].station_id = station_id;
        dedup_state.pending[dedup_state.pending_count].sequence = sequence;
        if (dedup_state.pending_count++ == 0)
            clock_gettime(CLOCK_MONOTONIC, &dedup_state.pending_first);
    }
    pthread_mutex_unlock(&dedup_state.mutex);
    return is_new;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_config_populate(void) {
    memset(&dedup_state, 0, sizeof(dedup_state));
    dedup_state.enabled = config_get_bool("dedup-enable", false);
    dedup_state.port = (uint16_t)config_get_integer("dedup-port", DEDUP_PORT_DEFAULT);
    dedup_state.delay_ms = (uint32_t)config_get_integer("dedup-delay", DEDUP_DELAY_MS_DEFAULT);
    const char *peers = config_get_string("dedup-peers", "");
    dedup_peers_parse(peers);
    dedup_state.debug = config_get_bool("debug-dedup", false);

    printf("config: dedup: enabled=%c, port=%" PRIu16 ", peers=%s, delay=%" PRIu32 "ms, debug=%s\n", dedup_state.enabled ? 'y' : 'n', dedup_state.port, peers, dedup_state.delay_ms, dedup_state.debug ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_begin(uint16_t gateway_id, iotdata_mesh_dedup_ring_t* dedup_ring) {
    if (!dedup_state.enabled) {
        printf("dedup: disabled, not starting\n");
        return true;
    }

    dedup_state.gateway_id = gateway_id;
    dedup_state.dedup_ring = dedup_ring;

    printf("dedup: enabled, port=%" PRIu16 ", peers=%d, gateway_id=%04" PRIX16 ", delay=%" PRIu32 "ms\n", dedup_state.port, dedup_state.peers_count, dedup_state.gateway_id, dedup_state.delay_ms);

    dedup_peers_resolve();
    pthread_mutex_init(&dedup_state.mutex, NULL);
    if (pthread_create(&dedup_state.thread, NULL, dedup_thread_func, NULL) != 0) {
        dedup_state.enabled = false;
        fprintf(stderr, "dedup: thread create failed: %s\n", strerror(errno));
        pthread_mutex_destroy(&dedup_state.mutex);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_end(void) {
    if (!dedup_state.enabled)
        return;
    pthread_join(dedup_state.thread, NULL);
    pthread_mutex_destroy(&dedup_state.mutex);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
