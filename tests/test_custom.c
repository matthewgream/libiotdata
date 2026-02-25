/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * test_custom.c - test suite for custom variant map
 *
 * Defines two custom variants and verifies:
 *   - Custom field ordering works
 *   - Multiple variants coexist
 *   - Fields decode correctly via custom field positions
 *   - JSON round-trip uses custom labels
 *   - Standalone field types (temperature, pressure, humidity,
 *     wind_speed, wind_direction, wind_gust) work in custom layouts
 */

#include "test_common.h"

/* ---------------------------------------------------------------------------
 * Custom variant definitions
 *
 * Variant 0: soil_sensor — 1 presence byte, 4 fields
 *   Uses standalone TEMPERATURE and HUMIDITY (not bundled ENVIRONMENT)
 *
 * Variant 1: wind_mast — 1 presence byte, 5 fields
 *   Uses individual WIND_SPEED, WIND_DIRECTION, WIND_GUST (not bundled WIND)
 *
 * Variant 2: radiation_monitor — 2 presence bytes, 8 fields
 *   Tests pres0+pres1 with custom ordering
 * -------------------------------------------------------------------------*/

const iotdata_variant_def_t custom_variants[3] = {
    /* Variant 0: soil sensor */
    [0] = {
        .name = "soil_sensor",
        .num_pres_bytes = 1,
        .fields = {
            { IOTDATA_FIELD_BATTERY,     "battery"    },
            { IOTDATA_FIELD_TEMPERATURE, "soil_temp"  },
            { IOTDATA_FIELD_HUMIDITY,    "soil_moist" },
            { IOTDATA_FIELD_DEPTH,       "soil_depth" },
            { IOTDATA_FIELD_NONE,        NULL         },
            { IOTDATA_FIELD_NONE,        NULL         },
        },
    },
    /* Variant 1: wind mast */
    [1] = {
        .name = "wind_mast",
        .num_pres_bytes = 1,
        .fields = {
            { IOTDATA_FIELD_BATTERY,     "battery"    },
            { IOTDATA_FIELD_WIND_SPEED,  "wind_speed" },
            { IOTDATA_FIELD_WIND_DIRECTION,    "wind_direction"   },
            { IOTDATA_FIELD_WIND_GUST,   "wind_gust"  },
            { IOTDATA_FIELD_LINK,        "link"       },
            { IOTDATA_FIELD_NONE,        NULL         },
        },
    },
    /* Variant 2: radiation monitor */
    [2] = {
        .name = "radiation_monitor",
        .num_pres_bytes = 2,
        .fields = {
            /* pres0 (6 fields) */
            { IOTDATA_FIELD_BATTERY,         "battery"     },
            { IOTDATA_FIELD_RADIATION_CPM,   "rad_cpm"     },
            { IOTDATA_FIELD_RADIATION_DOSE,  "rad_dose"    },
            { IOTDATA_FIELD_TEMPERATURE,     "temp"        },
            { IOTDATA_FIELD_PRESSURE,        "pressure"    },
            { IOTDATA_FIELD_HUMIDITY,        "humidity"    },
            /* pres1 (7 fields) */
            { IOTDATA_FIELD_POSITION,        "position"    },
            { IOTDATA_FIELD_DATETIME,        "datetime"    },
            { IOTDATA_FIELD_FLAGS,           "flags"       },
            { IOTDATA_FIELD_LINK,            "link"        },
            { IOTDATA_FIELD_SOLAR,           "solar"       },
            { IOTDATA_FIELD_NONE,            NULL          },
            { IOTDATA_FIELD_NONE,            NULL          },
        },
    },
};

/* =========================================================================
 * Variant 0: soil_sensor — standalone TEMPERATURE and HUMIDITY
 * =========================================================================*/

static void test_soil_sensor_basic(void) {
    TEST("Soil sensor: basic round-trip");
    begin(0, 1, 1);

    ASSERT_OK(iotdata_encode_battery(&enc, 72, false), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, 15.5f), "temp");
    ASSERT_OK(iotdata_encode_humidity(&enc, 85), "humid");
    ASSERT_OK(iotdata_encode_depth(&enc, 30), "depth");

    finish();
    printf("\n    [soil sensor: %" PRIu32 " bytes] ", (uint32_t)pkt_len);
    decode_pkt();

    ASSERT_EQ(dec.variant, 0, "variant");
    ASSERT_NEAR(dec.battery_level, 72, 4, "bat");
    ASSERT_NEAR(dec.temperature, 15.5, 0.25, "temp");
    ASSERT_EQ(dec.humidity, 85, "humid");
    ASSERT_EQ(dec.depth, 30, "depth");
    PASS();
}

static void test_soil_sensor_partial(void) {
    TEST("Soil sensor: partial fields (battery + temp only)");
    begin(0, 2, 10);

    ASSERT_OK(iotdata_encode_battery(&enc, 50, true), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, -10.0f), "temp");

    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_BATTERY), 1, "bat present");
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_TEMPERATURE), 1, "temp present");
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_HUMIDITY), 0, "humid absent");
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_DEPTH), 0, "depth absent");
    ASSERT_NEAR(dec.temperature, -10.0, 0.25, "temp");
    PASS();
}

static void test_soil_sensor_boundaries(void) {
    TEST("Soil sensor: boundary values");
    begin(0, 1, 2);

    ASSERT_OK(iotdata_encode_battery(&enc, 0, false), "bat 0");
    ASSERT_OK(iotdata_encode_temperature(&enc, -40.0f), "temp min");
    ASSERT_OK(iotdata_encode_humidity(&enc, 0), "humid min");
    ASSERT_OK(iotdata_encode_depth(&enc, 0), "depth min");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.battery_level, 0, "bat");
    ASSERT_NEAR(dec.temperature, -40.0, 0.25, "temp");
    ASSERT_EQ(dec.humidity, 0, "humid");
    ASSERT_EQ(dec.depth, 0, "depth");

    begin(0, 1, 3);
    ASSERT_OK(iotdata_encode_battery(&enc, 100, true), "bat 100");
    ASSERT_OK(iotdata_encode_temperature(&enc, 80.0f), "temp max");
    ASSERT_OK(iotdata_encode_humidity(&enc, 100), "humid max");
    ASSERT_OK(iotdata_encode_depth(&enc, 1023), "depth max");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.battery_level, 100, "bat");
    ASSERT_NEAR(dec.temperature, 80.0, 0.25, "temp");
    ASSERT_EQ(dec.humidity, 100, "humid");
    ASSERT_EQ(dec.depth, 1023, "depth");
    PASS();
}

/* =========================================================================
 * Variant 1: wind_mast — individual WIND_SPEED, WIND_DIRECTION, WIND_GUST
 * =========================================================================*/

static void test_wind_mast_basic(void) {
    TEST("Wind mast: individual wind fields");
    begin(1, 10, 500);

    ASSERT_OK(iotdata_encode_battery(&enc, 95, true), "bat");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 12.5f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 225), "wdir");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 22.0f), "wgust");
    ASSERT_OK(iotdata_encode_link(&enc, -75, 8.0f), "link");

    finish();
    printf("\n    [wind mast: %" PRIu32 " bytes] ", (uint32_t)pkt_len);
    decode_pkt();

    ASSERT_EQ(dec.variant, 1, "variant");
    ASSERT_NEAR(dec.wind_speed, 12.5, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 225, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 22.0, 0.5, "wgust");
    PASS();
}

static void test_wind_mast_partial(void) {
    TEST("Wind mast: speed + dir only (no gust)");
    begin(1, 10, 501);

    ASSERT_OK(iotdata_encode_wind_speed(&enc, 3.0f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 90), "wdir");

    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_WIND_SPEED), 1, "wspd present");
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_WIND_DIRECTION), 1, "wdir present");
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_WIND_GUST), 0, "wgust absent");
    ASSERT_NEAR(dec.wind_speed, 3.0, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 90, 2.0, "wdir");
    PASS();
}

static void test_wind_mast_boundaries(void) {
    TEST("Wind mast: boundary values");
    begin(1, 1, 1);

    ASSERT_OK(iotdata_encode_wind_speed(&enc, 0.0f), "wspd min");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 0), "wdir min");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 0.0f), "wgust min");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_speed, 0.0, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 0, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 0.0, 0.5, "wgust");

    begin(1, 1, 2);
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 63.5f), "wspd max");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 359), "wdir max");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 63.5f), "wgust max");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_speed, 63.5, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 359, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 63.5, 0.5, "wgust");
    PASS();
}

/* =========================================================================
 * Variant 2: radiation_monitor — multi-presence-byte custom layout
 * =========================================================================*/

static void test_radiation_monitor_pres0(void) {
    TEST("Radiation monitor: pres0 fields only");
    begin(2, 50, 1000);

    ASSERT_OK(iotdata_encode_battery(&enc, 60, false), "bat");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 350), "cpm");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 2.50f), "dose");
    ASSERT_OK(iotdata_encode_temperature(&enc, 22.0f), "temp");
    ASSERT_OK(iotdata_encode_pressure(&enc, 1013), "pres");
    ASSERT_OK(iotdata_encode_humidity(&enc, 55), "humid");

    finish();
    printf("\n    [rad pres0: %" PRIu32 " bytes] ", (uint32_t)pkt_len);
    decode_pkt();

    ASSERT_EQ(dec.variant, 2, "variant");
    ASSERT_EQ_U(dec.radiation_cpm, 350, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 2.50, 0.01, "dose");
    ASSERT_NEAR(dec.temperature, 22.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1013, "pres");
    ASSERT_EQ(dec.humidity, 55, "humid");
    PASS();
}

static void test_radiation_monitor_full(void) {
    TEST("Radiation monitor: all fields (pres0 + pres1)");
    begin(2, 50, 1001);

    /* Pres0 */
    ASSERT_OK(iotdata_encode_battery(&enc, 80, true), "bat");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 1000), "cpm");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 5.00f), "dose");
    ASSERT_OK(iotdata_encode_temperature(&enc, 25.0f), "temp");
    ASSERT_OK(iotdata_encode_pressure(&enc, 1005), "pres");
    ASSERT_OK(iotdata_encode_humidity(&enc, 45), "humid");

    /* Pres1 */
    ASSERT_OK(iotdata_encode_position(&enc, 51.5, -0.1), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 172800), "dt");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x03), "flags");
    ASSERT_OK(iotdata_encode_link(&enc, -85, 3.0f), "link");
    ASSERT_OK(iotdata_encode_solar(&enc, 400, 6), "solar");

    finish();
    printf("\n    [rad full: %" PRIu32 " bytes] ", (uint32_t)pkt_len);
    decode_pkt();

    /* Verify pres0 */
    ASSERT_EQ_U(dec.radiation_cpm, 1000, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 5.00, 0.01, "dose");
    ASSERT_NEAR(dec.temperature, 25.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1005, "pres");
    ASSERT_EQ(dec.humidity, 45, "humid");

    /* Verify pres1 */
    ASSERT_NEAR(dec.position_lat, 51.5, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, -0.1, 0.001, "lon");
    ASSERT_EQ_U(dec.datetime_secs, 172800, "dt");
    ASSERT_EQ(dec.flags, 0x03, "flags");
    ASSERT_EQ(dec.solar_irradiance, 400, "sol");
    ASSERT_EQ(dec.solar_ultraviolet, 6, "uv");
    PASS();
}

/* =========================================================================
 * Cross-variant tests
 * =========================================================================*/

static void test_variant_id_in_packet(void) {
    TEST("Variant ID preserved in packet");

    /* Encode with variant 0 */
    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.variant, 0, "var0");

    /* Encode with variant 1 */
    begin(1, 1, 2);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.variant, 1, "var1");

    /* Encode with variant 2 */
    begin(2, 1, 3);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.variant, 2, "var2");

    PASS();
}

static void test_json_uses_custom_labels(void) {
    TEST("JSON uses custom variant labels");
    begin(0, 1, 100);

    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, 15.0f), "temp");
    ASSERT_OK(iotdata_encode_humidity(&enc, 80), "humid");
    ASSERT_OK(iotdata_encode_depth(&enc, 25), "depth");

    finish();

    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    /* Verify custom labels appear */
    if (!strstr(json, "soil_temp")) {
        free(json);
        FAIL("missing soil_temp");
        return;
    }
    if (!strstr(json, "soil_moist")) {
        free(json);
        FAIL("missing soil_moist");
        return;
    }
    if (!strstr(json, "soil_depth")) {
        free(json);
        FAIL("missing soil_depth");
        return;
    }

    /* JSON round-trip */
    uint8_t pkt2[256];
    size_t len2;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

static void test_json_wind_mast_labels(void) {
    TEST("JSON wind mast uses custom labels");
    begin(1, 5, 200);

    ASSERT_OK(iotdata_encode_battery(&enc, 90, false), "bat");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 8.0f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 180), "wdir");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 12.0f), "wgust");

    finish();

    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    if (!strstr(json, "wind_speed")) {
        free(json);
        FAIL("missing wind_speed");
        return;
    }
    if (!strstr(json, "wind_direction")) {
        free(json);
        FAIL("missing wind_direction");
        return;
    }
    if (!strstr(json, "wind_gust")) {
        free(json);
        FAIL("missing wind_gust");
        return;
    }

    /* JSON round-trip */
    uint8_t pkt2[256];
    size_t len2;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

static void test_print_shows_variant_name(void) {
    TEST("Print output shows custom variant names");

    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();

    char str[4096];
    iotdata_print_scratch_t print_scratch;
    ASSERT_OK(iotdata_print_to_string(pkt, pkt_len, str, sizeof(str), &print_scratch), "print");
    if (!strstr(str, "soil_sensor")) {
        FAIL("missing soil_sensor");
        return;
    }

    begin(1, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();

    ASSERT_OK(iotdata_print_to_string(pkt, pkt_len, str, sizeof(str), &print_scratch), "print");
    if (!strstr(str, "wind_mast")) {
        FAIL("missing wind_mast");
        return;
    }

    PASS();
}

static void test_get_variant_function(void) {
    TEST("iotdata_get_variant returns correct variants");
    const iotdata_variant_def_t *v0 = iotdata_get_variant(0);
    if (v0 == NULL) {
        FAIL("v0 null");
        return;
    }
    const iotdata_variant_def_t *v1 = iotdata_get_variant(1);
    if (v1 == NULL) {
        FAIL("v1 null");
        return;
    }
    const iotdata_variant_def_t *v2 = iotdata_get_variant(2);
    if (v2 == NULL) {
        FAIL("v2 null");
        return;
    }

    if (strcmp(v0->name, "soil_sensor") != 0) {
        FAIL("v0 name");
        return;
    }
    if (strcmp(v1->name, "wind_mast") != 0) {
        FAIL("v1 name");
        return;
    }
    if (strcmp(v2->name, "radiation_monitor") != 0) {
        FAIL("v2 name");
        return;
    }

    ASSERT_EQ(v0->num_pres_bytes, 1, "v0 pres");
    ASSERT_EQ(v1->num_pres_bytes, 1, "v1 pres");
    ASSERT_EQ(v2->num_pres_bytes, 2, "v2 pres");

    /* Out-of-range variant returns NULL */
    const iotdata_variant_def_t *vx = iotdata_get_variant(14);
    if (vx != NULL) {
        FAIL("out of range variant");
        return;
    }

    PASS();
}

static void test_empty_packets_all_variants(void) {
    TEST("Empty packets for all variants");

    for (uint8_t v = 0; v < 3; v++) {
        begin(v, 1, v);
        finish();
        ASSERT_EQ(pkt_len, 5, "5 bytes");
        decode_pkt();
        ASSERT_EQ(dec.variant, v, "variant");
        ASSERT_EQ(dec.fields, 0, "no fields");
    }
    PASS();
}

/* =========================================================================
 * Main
 * =========================================================================*/

int main(void) {
    printf("\n=== iotdata — custom variant test suite ===\n\n");

    /* Soil sensor */
    printf("  --- Variant 0: soil_sensor ---\n");
    test_soil_sensor_basic();
    test_soil_sensor_partial();
    test_soil_sensor_boundaries();

    /* Wind mast */
    printf("\n  --- Variant 1: wind_mast ---\n");
    test_wind_mast_basic();
    test_wind_mast_partial();
    test_wind_mast_boundaries();

    /* Radiation monitor */
    printf("\n  --- Variant 2: radiation_monitor ---\n");
    test_radiation_monitor_pres0();
    test_radiation_monitor_full();

    /* Cross-variant */
    printf("\n  --- Cross-variant ---\n");
    test_variant_id_in_packet();
    test_json_uses_custom_labels();
    test_json_wind_mast_labels();
    test_print_shows_variant_name();
    test_get_variant_function();
    test_empty_packets_all_variants();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(" (%d FAILED)", tests_failed);
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
