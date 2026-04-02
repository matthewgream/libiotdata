
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

struct {
    bool enabled;
    uint16_t station_id;             /* this gateway's station_id for mesh packets */
    time_t beacon_interval;          /* seconds between beacon transmissions */
    uint16_t beacon_generation;      /* increments each beacon round */
    uint16_t mesh_seq;               /* mesh packet sequence counter */
    time_t beacon_last;              /* last beacon TX time */
    iotdata_mesh_dedup_ring_t dedup_ring; /* dedup ring */
    bool debug;
    /* statistics */
    uint32_t stat_beacons_tx;
    uint32_t stat_forwards_rx;
    uint32_t stat_forwards_unwrapped;
    uint32_t stat_duplicates;
    uint32_t stat_acks_tx;
    uint32_t stat_mesh_ctrl_rx;
    uint32_t stat_mesh_unknown;
} mesh_state;

typedef bool (*mesh_packet_handler_t)(const uint8_t *packet, const int length);
typedef bool (*mesh_dedup_handler_t)(uint16_t station_id, uint16_t sequence);

mesh_packet_handler_t mesh_packet_handler = NULL;
mesh_dedup_handler_t mesh_dedup_handler = NULL;

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
        printf("mesh: tx BEACON generation=%" PRIu16 ", station=0x%04" PRIX16 "\n", beacon.generation, beacon.sender_station);
    if (mesh_packet_handler(buf, IOTDATA_MESH_BEACON_SIZE))
        mesh_state.stat_beacons_tx++;
    else
        fprintf(stderr, "mesh: tx BEACON failed\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

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
        printf("mesh: tx ACK to station=0x%04" PRIX16 ", sequence=%" PRIu16 "\n", fwd_station, fwd_seq);
    if (mesh_packet_handler(buf, IOTDATA_MESH_ACK_SIZE))
        mesh_state.stat_acks_tx++;
    else
        fprintf(stderr, "mesh: tx ACK failed\n");
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
        printf("mesh: rx FORWARD from station=0x%04" PRIX16 ", sequence=%" PRIu16 ", ttl=%" PRIu8 ", origin={station=0x%04" PRIX16 ", sequence=%" PRIu16 "}, inner-length=%d\n", fwd.sender_station, fwd.sender_seq, fwd.ttl,
               fwd.origin_station, fwd.origin_sequence, fwd.inner_len);
    if (mesh_dedup_handler && !mesh_dedup_handler(fwd.origin_station, fwd.origin_sequence)) {
        mesh_state.stat_duplicates++;
        if (mesh_state.debug)
            printf("mesh: rx FORWARD duplicate suppressed origin={station=0x%04" PRIX16 ", sequence=%" PRIu16 "}, inner-length=%d\n", fwd.origin_station, fwd.origin_sequence, fwd.inner_len);
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

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_handle_beacon(const uint8_t *buf, int len) {
    /* gateway receiving another gateway's beacon — log for multi-gateway awareness */
    iotdata_mesh_beacon_t b;
    if (iotdata_mesh_unpack_beacon(buf, len, &b))
        if (mesh_state.debug)
            printf("mesh: rx BEACON from gateway=0x%04" PRIX16 ", generation=%" PRIu16 ", cost=%" PRIu8 ", flags=0x%02" PRIX8 "\n", b.gateway_id, b.generation, b.cost, b.flags);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_handle_route_error(const uint8_t *buf, int len) {
    iotdata_mesh_route_error_t err;
    if (iotdata_mesh_unpack_route_error(buf, len, &err))
        printf("mesh: rx ROUTE_ERROR from station=0x%04" PRIX16 ", reason=%s\n", err.sender_station, iotdata_mesh_reason_name(err.reason));
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_handle_neighbour_report(const uint8_t *buf, int len) {
    /* full topology aggregation is future work — log receipt for now */
    uint8_t variant;
    uint16_t station_id, sequence;
    if (iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence))
        printf("mesh rx NEIGHBOUR_REPORT from station=0x%04" PRIX16 " (%d bytes)\n", station_id, len);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_handle_pong(const uint8_t *buf, int len) {
    uint8_t variant;
    uint16_t station_id, sequence;
    if (iotdata_mesh_peek_header(buf, len, &variant, &station_id, &sequence))
        printf("mesh: rx PONG from station=0x%04" PRIX16 " (%d bytes)\n", station_id, len);
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_config_populate(void) {
    memset(&mesh_state, 0, sizeof(mesh_state));
    mesh_state.enabled = config_get_bool("mesh-enable", false);
    mesh_state.station_id = (uint16_t)config_get_integer("mesh-station-id", GATEWAY_STATION_ID_DEFAULT);
    mesh_state.beacon_interval = (time_t)config_get_integer("mesh-beacon-interval", INTERVAL_BEACON_DEFAULT);
    mesh_state.debug = config_get_bool("debug-mesh", false);

    printf("config: mesh: enabled=%c, station=0x%04" PRIX16 ", beacon-interval=%" PRIu32 ", debug=%s\n", mesh_state.enabled ? 'y' : 'n', mesh_state.station_id, (uint32_t)mesh_state.beacon_interval, mesh_state.debug ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------

bool mesh_begin(mesh_packet_handler_t packet_handler, mesh_dedup_handler_t dedup_handler) {
    if (!mesh_state.enabled) {
        printf("mesh: disabled, not starting\n");
        return true;
    }
    if (!packet_handler) {
        printf("mesh: packet handler is required, not starting\n");
        return true;
    }
    mesh_packet_handler = packet_handler;
    mesh_dedup_handler = dedup_handler;
    iotdata_mesh_dedup_init(&mesh_state.dedup_ring);
    printf("mesh: enabled, station=0x%04" PRIX16 ", beacon-interval=%" PRIu32 "s\n", mesh_state.station_id, (uint32_t)mesh_state.beacon_interval);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------

void mesh_end(void) {
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
