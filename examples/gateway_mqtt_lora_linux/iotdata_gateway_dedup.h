
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

typedef struct {
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
    iotdata_mesh_dedup_ring_t *dedup_ring;
    volatile bool *running;
    bool debug;
    /* statistics */
    uint32_t stat_send_cycles;
    uint32_t stat_send_entries;
    uint32_t stat_recv_cycles;
    uint32_t stat_recv_entries;
    uint32_t stat_injected;
} dedup_state_t;

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

void dedup_peers_parse(dedup_state_t *state, const char *peers_str) {
    if (!peers_str || !*peers_str)
        return;
    char buf[512];
    strncpy(buf, peers_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = NULL, *tok = strtok_r(buf, ",", &save);
    while (tok && state->peers_count < DEDUP_PEERS_MAX) {
        while (*tok == ' ')
            tok++;
        char *colon = strrchr(tok, ':');
        uint16_t pport = state->port;
        if (colon) {
            *colon = '\0';
            pport = (uint16_t)atoi(colon + 1);
        }
        strncpy(state->peers[state->peers_count].host, tok, sizeof(state->peers[0].host) - 1);
        state->peers[state->peers_count].host[sizeof(state->peers[0].host) - 1] = '\0';
        state->peers[state->peers_count].port = pport;
        state->peers_count++;
        tok = strtok_r(NULL, ",", &save);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_peers_resolve(dedup_state_t *state) {
    for (int i = 0; i < state->peers_count; i++) {
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%" PRIu16, state->peers[i].port);
        const struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
        struct addrinfo *res;
        const int err = getaddrinfo(state->peers[i].host, port_str, &hints, &res);
        if (err == 0) {
            memcpy(&state->peers[i].addr, res->ai_addr, sizeof(state->peers[i].addr));
            state->peers[i].resolved = true;
            freeaddrinfo(res);
            printf("dedup: peer[%d] %s:%" PRIu16 " resolved\n", i, state->peers[i].host, state->peers[i].port);
        } else {
            state->peers[i].resolved = false;
            fprintf(stderr, "dedup: peer[%d] %s:%" PRIu16 " resolution failed: %s\n", i, state->peers[i].host, state->peers[i].port, gai_strerror(err));
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int dedup_recv_setup(dedup_state_t *state) {
    const int recv_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_fd < 0) {
        fprintf(stderr, "dedup: recv socket: %s\n", strerror(errno));
        return -1;
    }
    int optval = 1;
    setsockopt(recv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof(optval));
    const struct sockaddr_in bind_addr = { .sin_family = AF_INET, .sin_port = htons(state->port), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(recv_fd, (const struct sockaddr *)&bind_addr, (socklen_t)sizeof(bind_addr)) < 0) {
        fprintf(stderr, "dedup: bind port %" PRIu16 ": %s\n", state->port, strerror(errno));
        close(recv_fd);
        return -1;
    }
    return recv_fd;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_recv_from_peers(dedup_state_t *state, int recv_fd) {
    struct pollfd pfd = { .fd = recv_fd, .events = POLLIN, .revents = 0 };
    if (poll(&pfd, 1, 5) > 0 && (pfd.revents & POLLIN)) {
        dedup_packet_t pkt;
        struct sockaddr_in from;
        socklen_t from_len = (socklen_t)sizeof(from);
        const ssize_t recv_len = recvfrom(recv_fd, pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &from_len);
        if (recv_len >= DEDUP_PKT_HEADER_SIZE) {
            const int entry_count = dedup_packet_get_entry_count(pkt);
            if (recv_len >= (ssize_t)dedup_packet_get_length(pkt)) {
                pthread_mutex_lock(&state->mutex);
                for (int entry_index = 0; entry_index < entry_count; entry_index++) {
                    iotdata_mesh_dedup_check_and_add(state->dedup_ring, dedup_packet_get_entry_station(pkt, entry_index), dedup_packet_get_entry_sequence(pkt, entry_index));
                    state->stat_injected++;
                }
                pthread_mutex_unlock(&state->mutex);
                state->stat_recv_cycles++;
                state->stat_recv_entries += (uint32_t)entry_count;
                if (state->debug)
                    printf("dedup: rx from gateway=0x%04" PRIX16 ", entries=%d\n", dedup_packet_get_gateway_id(pkt), entry_count);
            }
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int dedup_send_setup(dedup_state_t *state) {
    (void)state;
    const int send_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_fd < 0) {
        fprintf(stderr, "dedup: send socket: %s\n", strerror(errno));
        return -1;
    }
    return send_fd;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

int dedup_send_collect(dedup_state_t *state, iotdata_mesh_dedup_entry_t *send_entries) {
    int send_count = 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    pthread_mutex_lock(&state->mutex);
    if (state->pending_count > 0) {
        const int32_t elapsed_ms = (int32_t)((now.tv_sec - state->pending_first.tv_sec) * 1000L) + (int32_t)((now.tv_nsec - state->pending_first.tv_nsec) / 1000000L);
        if (elapsed_ms > 0 && (uint32_t)elapsed_ms >= state->delay_ms) {
            send_count = state->pending_count;
            memcpy(send_entries, state->pending, (size_t)send_count * sizeof(iotdata_mesh_dedup_entry_t));
            state->pending_count = 0;
        }
    }
    pthread_mutex_unlock(&state->mutex);
    return send_count;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_send_to_peers(dedup_state_t *state, int send_fd, iotdata_mesh_dedup_entry_t *send_entries, int send_count) {
    int send_offset = 0;
    while (send_offset < send_count) {
        const int entry_count = DEDUP_MIN(send_count - send_offset, DEDUP_BATCH_MAX);
        dedup_packet_t pkt;
        dedup_packet_set_gateway_id(pkt, state->gateway_id);
        dedup_packet_set_entry_count(pkt, entry_count);
        for (int entry_index = 0; entry_index < entry_count; entry_index++) {
            dedup_packet_set_entry_station(pkt, entry_index, send_entries[send_offset + entry_index].station_id);
            dedup_packet_set_entry_sequence(pkt, entry_index, send_entries[send_offset + entry_index].sequence);
        }
        const size_t pkt_len = dedup_packet_get_length(pkt);
        for (int peer = 0; peer < state->peers_count; peer++)
            if (state->peers[peer].resolved)
                (void)sendto(send_fd, pkt, pkt_len, 0, (const struct sockaddr *)&state->peers[peer].addr, (socklen_t)sizeof(state->peers[peer].addr));
        state->stat_send_cycles++;
        state->stat_send_entries += (uint32_t)entry_count;
        send_offset += entry_count;
    }
    if (state->debug)
        printf("dedup: tx %d entries to %d peers\n", send_count, state->peers_count);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void *dedup_thread_func(void *arg) {
    dedup_state_t *state = (dedup_state_t *)arg;
    int recv_fd, send_fd;
    if ((recv_fd = dedup_recv_setup(state)) < 0)
        goto dedup_end_all;
    if ((send_fd = dedup_send_setup(state)) < 0)
        goto dedup_end_send;
    iotdata_mesh_dedup_entry_t send_entries[DEDUP_PENDING_MAX];
    while (*state->running) {
        dedup_recv_from_peers(state, recv_fd);
        if (state->peers_count > 0) {
            const int send_count = dedup_send_collect(state, send_entries);
            if (send_count > 0)
                dedup_send_to_peers(state, send_fd, send_entries, send_count);
        }
    }
    close(send_fd);
dedup_end_send:
    close(recv_fd);
dedup_end_all:
    return NULL;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_check_and_add(dedup_state_t *state, uint16_t station_id, uint16_t sequence) {
    if (!state->enabled)
        return iotdata_mesh_dedup_check_and_add(state->dedup_ring, station_id, sequence);
    bool is_new;
    pthread_mutex_lock(&state->mutex);
    is_new = iotdata_mesh_dedup_check_and_add(state->dedup_ring, station_id, sequence);
    if (is_new && state->pending_count < DEDUP_PENDING_MAX) {
        state->pending[state->pending_count].station_id = station_id;
        state->pending[state->pending_count].sequence = sequence;
        if (state->pending_count++ == 0)
            clock_gettime(CLOCK_MONOTONIC, &state->pending_first);
    }
    pthread_mutex_unlock(&state->mutex);
    return is_new;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool dedup_begin(dedup_state_t *state, uint16_t gateway_id, iotdata_mesh_dedup_ring_t *dedup_ring) {
    if (!state->enabled) {
        printf("dedup: disabled, not starting\n");
        return true;
    }
    state->gateway_id = gateway_id;
    state->dedup_ring = dedup_ring;
    printf("dedup: enabled, port=%" PRIu16 ", peers=%d, gateway_id=%04" PRIX16 ", delay=%" PRIu32 "ms\n", state->port, state->peers_count, state->gateway_id, state->delay_ms);
    dedup_peers_resolve(state);
    pthread_mutex_init(&state->mutex, NULL);
    if (pthread_create(&state->thread, NULL, dedup_thread_func, state) != 0) {
        state->enabled = false;
        fprintf(stderr, "dedup: thread create failed: %s\n", strerror(errno));
        pthread_mutex_destroy(&state->mutex);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void dedup_end(dedup_state_t *state) {
    if (!state->enabled)
        return;
    pthread_join(state->thread, NULL);
    pthread_mutex_destroy(&state->mutex);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
