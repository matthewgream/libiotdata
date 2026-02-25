/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * test_complete.c - comprehensive test suite for all field types
 *
 * Defines two custom variants to exercise every field type:
 *   Variant 0: "complete" — all bundled field types plus AQ PM/Gas,
 *              depth, and image (3 presence bytes, 16 fields)
 *   Variant 1: "standalone" — all standalone sub-field types
 *              (3 presence bytes, 15 fields)
 *
 * Tests: field round-trips, boundary values, error conditions,
 * peek, TLV typed helpers, JSON round-trip with TLV, decode
 * error paths, encode buffer overflow, and image compression.
 */

#include "test_common.h"

/* ---------------------------------------------------------------------------
 * Custom variant definitions
 * -------------------------------------------------------------------------*/

const iotdata_variant_def_t complete_variants[2] = {
    /* Variant 0: complete — bundled fields + extras not in default */
    [0] = {
        .name = "complete",
        .num_pres_bytes = 3,
        .fields = {
            /* pres0 (6 fields) */
            { IOTDATA_FIELD_BATTERY,           "battery" },
            { IOTDATA_FIELD_LINK,              "link" },
            { IOTDATA_FIELD_ENVIRONMENT,       "environment" },
            { IOTDATA_FIELD_WIND,              "wind" },
            { IOTDATA_FIELD_RAIN,              "rain" },
            { IOTDATA_FIELD_SOLAR,             "solar" },
            /* pres1 (7 fields) */
            { IOTDATA_FIELD_CLOUDS,            "clouds" },
            { IOTDATA_FIELD_AIR_QUALITY_INDEX, "air_quality" },
            { IOTDATA_FIELD_AIR_QUALITY_PM,    "air_quality_pm" },
            { IOTDATA_FIELD_AIR_QUALITY_GAS,   "air_quality_gas" },
            { IOTDATA_FIELD_RADIATION,         "radiation" },
            { IOTDATA_FIELD_DEPTH,             "depth" },
            { IOTDATA_FIELD_POSITION,          "position" },
            /* pres2 (7 fields) */
            { IOTDATA_FIELD_DATETIME,          "datetime" },
            { IOTDATA_FIELD_IMAGE,             "image" },
            { IOTDATA_FIELD_FLAGS,             "flags" },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
        },
    },
    /* Variant 1: standalone — individual sub-field types */
    [1] = {
        .name = "standalone",
        .num_pres_bytes = 3,
        .fields = {
            /* pres0 (6 fields) */
            { IOTDATA_FIELD_BATTERY,           "battery" },
            { IOTDATA_FIELD_TEMPERATURE,       "temperature" },
            { IOTDATA_FIELD_PRESSURE,          "pressure" },
            { IOTDATA_FIELD_HUMIDITY,          "humidity" },
            { IOTDATA_FIELD_WIND_SPEED,        "wind_speed" },
            { IOTDATA_FIELD_WIND_DIRECTION,    "wind_direction" },
            /* pres1 (7 fields) */
            { IOTDATA_FIELD_WIND_GUST,         "wind_gust" },
            { IOTDATA_FIELD_RAIN_RATE,         "rain_rate" },
            { IOTDATA_FIELD_RAIN_SIZE,         "rain_size" },
            { IOTDATA_FIELD_RADIATION_CPM,     "radiation_cpm" },
            { IOTDATA_FIELD_RADIATION_DOSE,    "radiation_dose" },
            { IOTDATA_FIELD_DEPTH,             "depth" },
            { IOTDATA_FIELD_POSITION,          "position" },
            /* pres2 (7 fields) */
            { IOTDATA_FIELD_DATETIME,          "datetime" },
            { IOTDATA_FIELD_FLAGS,             "flags" },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
        },
    },
};

/* =========================================================================
 * Section 1: Field round-trips for types not in the default variant
 * =========================================================================*/

static void test_aq_pm_round_trip(void) {
    TEST("Air quality PM round-trip");
    begin(0, 1, 1);

    uint16_t pm[4] = { 100, 250, 75, 500 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_AIR_QUALITY_PM), 1, "present");
    ASSERT_EQ_U(dec.aq_pm_present, 0x0F, "mask");
    ASSERT_EQ_U(dec.aq_pm[0], 100, "pm1.0");
    ASSERT_EQ_U(dec.aq_pm[1], 250, "pm2.5");
    ASSERT_EQ_U(dec.aq_pm[2], 75, "pm4.0");
    ASSERT_EQ_U(dec.aq_pm[3], 500, "pm10");
    PASS();
}

static void test_aq_pm_partial(void) {
    TEST("Air quality PM partial (2 channels)");
    begin(0, 1, 2);

    uint16_t pm[4] = { 50, 0, 0, 200 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x09, pm), "encode"); /* PM1.0 + PM10 */
    finish();
    decode_pkt();

    ASSERT_EQ_U(dec.aq_pm_present, 0x09, "mask");
    ASSERT_EQ_U(dec.aq_pm[0], 50, "pm1.0");
    ASSERT_EQ_U(dec.aq_pm[3], 200, "pm10");
    PASS();
}

static void test_aq_gas_round_trip(void) {
    TEST("Air quality gas round-trip");
    begin(0, 1, 3);

    uint16_t gas[8] = { 200, 100, 5000, 500, 250, 100, 0, 0 };
    ASSERT_OK(iotdata_encode_air_quality_gas(&enc, 0x3F, gas), "encode"); /* first 6 slots */
    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_AIR_QUALITY_GAS), 1, "present");
    ASSERT_EQ_U(dec.aq_gas_present, 0x3F, "mask");
    ASSERT_EQ_U(dec.aq_gas[0], 200, "voc");
    ASSERT_EQ_U(dec.aq_gas[1], 100, "nox");
    ASSERT_EQ_U(dec.aq_gas[2], 5000, "co2");
    ASSERT_EQ_U(dec.aq_gas[3], 500, "co");
    ASSERT_EQ_U(dec.aq_gas[4], 250, "hcho");
    ASSERT_EQ_U(dec.aq_gas[5], 100, "o3");
    PASS();
}

static void test_depth_round_trip(void) {
    TEST("Depth round-trip");
    begin(0, 1, 4);

    ASSERT_OK(iotdata_encode_depth(&enc, 500), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_DEPTH), 1, "present");
    ASSERT_EQ_U(dec.depth, 500, "depth");
    PASS();
}

static void test_image_round_trip(void) {
    TEST("Image round-trip");
    begin(0, 1, 5);

    uint8_t img[] = { 0xFF, 0x00, 0xAA, 0x55 };
    ASSERT_OK(iotdata_encode_image(&enc, IOTDATA_IMAGE_FMT_BILEVEL, IOTDATA_IMAGE_SIZE_24x18, IOTDATA_IMAGE_COMP_RAW, IOTDATA_IMAGE_FLAG_INVERT, img, 4), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_IMAGE), 1, "present");
    ASSERT_EQ(dec.image_pixel_format, IOTDATA_IMAGE_FMT_BILEVEL, "fmt");
    ASSERT_EQ(dec.image_size_tier, IOTDATA_IMAGE_SIZE_24x18, "size");
    ASSERT_EQ(dec.image_compression, IOTDATA_IMAGE_COMP_RAW, "comp");
    ASSERT_EQ(dec.image_flags & IOTDATA_IMAGE_FLAG_INVERT, IOTDATA_IMAGE_FLAG_INVERT, "invert");
    ASSERT_EQ(dec.image_data_len, 4, "len");
    ASSERT_EQ(dec.image_data[0], 0xFF, "px0");
    ASSERT_EQ(dec.image_data[1], 0x00, "px1");
    ASSERT_EQ(dec.image_data[2], 0xAA, "px2");
    ASSERT_EQ(dec.image_data[3], 0x55, "px3");
    PASS();
}

/* =========================================================================
 * Section 2: Full variant tests
 * =========================================================================*/

static void test_complete_variant_all_fields(void) {
    TEST("Complete variant - all 16 fields");
    begin(0, 100, 500);

    ASSERT_OK(iotdata_encode_battery(&enc, 90, true), "bat");
    ASSERT_OK(iotdata_encode_link(&enc, -80, 0.0f), "link");
    ASSERT_OK(iotdata_encode_environment(&enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 8.0f, 225, 12.0f), "wind");
    ASSERT_OK(iotdata_encode_rain(&enc, 5, 20), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 300, 5), "solar");
    ASSERT_OK(iotdata_encode_clouds(&enc, 4), "cloud");
    ASSERT_OK(iotdata_encode_air_quality_index(&enc, 75), "aqi");
    uint16_t pm[4] = { 50, 120, 80, 200 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm), "aq_pm");
    uint16_t gas[8] = { 200, 100, 5000, 500, 250, 100, 0, 0 };
    ASSERT_OK(iotdata_encode_air_quality_gas(&enc, 0x3F, gas), "aq_gas");
    ASSERT_OK(iotdata_encode_radiation(&enc, 100, 0.50f), "rad");
    ASSERT_OK(iotdata_encode_depth(&enc, 250), "depth");
    ASSERT_OK(iotdata_encode_position(&enc, 51.5, -0.1), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 86400), "dt");
    uint8_t img[] = { 0xDE, 0xAD };
    ASSERT_OK(iotdata_encode_image(&enc, 0, 0, 0, 0, img, 2), "img");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x42), "flags");
    finish();
    decode_pkt();

    /* Spot-check a selection of fields (90% round-trips exactly with 5-bit quantisation) */
    ASSERT_EQ(dec.battery_level, 90, "bat");
    ASSERT_EQ_U(dec.aq_pm_present, 0x0F, "pm_mask");
    ASSERT_EQ_U(dec.aq_pm[1], 120, "pm25");
    ASSERT_EQ_U(dec.aq_gas_present, 0x3F, "gas_mask");
    ASSERT_EQ_U(dec.aq_gas[2], 5000, "co2");
    ASSERT_EQ_U(dec.depth, 250, "depth");
    ASSERT_EQ(dec.image_data_len, 2, "img_len");
    ASSERT_EQ(dec.image_data[0], 0xDE, "img0");
    ASSERT_EQ(dec.flags, 0x42, "flags");
    PASS();
}

static void test_standalone_variant_all_fields(void) {
    TEST("Standalone variant - all 15 fields");
    begin(1, 200, 600);

    ASSERT_OK(iotdata_encode_battery(&enc, 60, false), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, 22.5f), "temp");
    ASSERT_OK(iotdata_encode_pressure(&enc, 1013), "pres");
    ASSERT_OK(iotdata_encode_humidity(&enc, 55), "hum");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 5.0f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 180), "wdir");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 8.0f), "wgust");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 10), "rrate");
    ASSERT_OK(iotdata_encode_rain_size(&enc, 20), "rsize");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 1500), "cpm");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 0.75f), "dose");
    ASSERT_OK(iotdata_encode_depth(&enc, 100), "depth");
    ASSERT_OK(iotdata_encode_position(&enc, -33.8688, 151.2093), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 43200), "dt");
    ASSERT_OK(iotdata_encode_flags(&enc, 0xFF), "flags");
    finish();
    decode_pkt();

    /* Spot-check standalone fields */
    ASSERT_NEAR(dec.temperature, 22.5, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1013, "pres");
    ASSERT_EQ(dec.humidity, 55, "hum");
    ASSERT_NEAR(dec.wind_speed, 5.0, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 180, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 8.0, 0.5, "wgust");
    ASSERT_EQ(dec.rain_rate, 10, "rrate");
    ASSERT_EQ_U(dec.radiation_cpm, 1500, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 0.75, 0.01, "dose");
    ASSERT_EQ_U(dec.depth, 100, "depth");
    ASSERT_NEAR(dec.position_lat, -33.8688, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, 151.2093, 0.001, "lon");
    ASSERT_EQ(dec.flags, 0xFF, "flags");
    PASS();
}

/* =========================================================================
 * Section 3: Boundary values
 * =========================================================================*/

static void test_aq_pm_boundaries(void) {
    TEST("AQ PM boundary values (min/max)");
    begin(0, 1, 10);

    /* Min: all zeros */
    uint16_t pm_min[4] = { 0, 0, 0, 0 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm_min), "min");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.aq_pm[0], 0, "min0");
    ASSERT_EQ_U(dec.aq_pm[3], 0, "min3");

    /* Max: 1275 on all channels */
    begin(0, 1, 11);
    uint16_t pm_max[4] = { 1275, 1275, 1275, 1275 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm_max), "max");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.aq_pm[0], 1275, "max0");
    ASSERT_EQ_U(dec.aq_pm[3], 1275, "max3");
    PASS();
}

static void test_aq_gas_boundaries(void) {
    TEST("AQ gas boundary values (max per slot)");
    begin(0, 1, 12);

    uint16_t gas[8] = { 510, 510, 51150, 1023, 5115, 1023, 1023, 1023 };
    ASSERT_OK(iotdata_encode_air_quality_gas(&enc, 0xFF, gas), "max all");
    finish();
    decode_pkt();

    ASSERT_EQ_U(dec.aq_gas[0], 510, "voc");
    ASSERT_EQ_U(dec.aq_gas[1], 510, "nox");
    ASSERT_EQ_U(dec.aq_gas[2], 51150, "co2");
    ASSERT_EQ_U(dec.aq_gas[3], 1023, "co");
    ASSERT_EQ_U(dec.aq_gas[4], 5115, "hcho");
    ASSERT_EQ_U(dec.aq_gas[5], 1023, "o3");
    ASSERT_EQ_U(dec.aq_gas[6], 1023, "rsvd6");
    ASSERT_EQ_U(dec.aq_gas[7], 1023, "rsvd7");
    PASS();
}

static void test_depth_boundaries(void) {
    TEST("Depth boundary values");
    begin(0, 1, 13);
    ASSERT_OK(iotdata_encode_depth(&enc, 0), "min");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.depth, 0, "min");

    begin(0, 1, 14);
    ASSERT_OK(iotdata_encode_depth(&enc, 1023), "max");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.depth, 1023, "max");
    PASS();
}

static void test_image_flags_combinations(void) {
    TEST("Image flag combinations");
    uint8_t px[] = { 0x42 };

    /* Fragment + invert */
    begin(0, 1, 15);
    ASSERT_OK(iotdata_encode_image(&enc, 0, 0, 0, IOTDATA_IMAGE_FLAG_FRAGMENT | IOTDATA_IMAGE_FLAG_INVERT, px, 1), "encode");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_flags & IOTDATA_IMAGE_FLAG_FRAGMENT, IOTDATA_IMAGE_FLAG_FRAGMENT, "fragment");
    ASSERT_EQ(dec.image_flags & IOTDATA_IMAGE_FLAG_INVERT, IOTDATA_IMAGE_FLAG_INVERT, "invert");

    /* All formats and sizes */
    begin(0, 1, 16);
    ASSERT_OK(iotdata_encode_image(&enc, IOTDATA_IMAGE_FMT_GREY16, IOTDATA_IMAGE_SIZE_64x48, IOTDATA_IMAGE_COMP_HEATSHRINK, 0, px, 1), "grey16+64x48+hs");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_pixel_format, IOTDATA_IMAGE_FMT_GREY16, "fmt");
    ASSERT_EQ(dec.image_size_tier, IOTDATA_IMAGE_SIZE_64x48, "sz");
    ASSERT_EQ(dec.image_compression, IOTDATA_IMAGE_COMP_HEATSHRINK, "comp");
    PASS();
}

/* =========================================================================
 * Section 4: Error conditions
 * =========================================================================*/

static void test_aq_pm_errors(void) {
    TEST("AQ PM error conditions");
    begin(0, 1, 20);

    uint16_t pm_high[4] = { 1280, 0, 0, 0 };
    ASSERT_ERR(iotdata_encode_air_quality_pm(&enc, 0x01, pm_high), IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH, "pm too high");
    PASS();
}

static void test_aq_gas_errors(void) {
    TEST("AQ gas error conditions");
    begin(0, 1, 21);

    uint16_t gas_high[8] = { 512, 0, 0, 0, 0, 0, 0, 0 }; /* VOC max 510 */
    ASSERT_ERR(iotdata_encode_air_quality_gas(&enc, 0x01, gas_high), IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH, "gas too high");
    PASS();
}

static void test_image_errors(void) {
    TEST("Image error conditions");
    begin(0, 1, 22);
    uint8_t px[] = { 0x42 };

    ASSERT_ERR(iotdata_encode_image(&enc, 3, 0, 0, 0, px, 1), IOTDATA_ERR_IMAGE_FORMAT_HIGH, "fmt high");
    ASSERT_ERR(iotdata_encode_image(&enc, 0, 4, 0, 0, px, 1), IOTDATA_ERR_IMAGE_SIZE_HIGH, "sz high");
    ASSERT_ERR(iotdata_encode_image(&enc, 0, 0, 3, 0, px, 1), IOTDATA_ERR_IMAGE_COMPRESSION_HIGH, "comp high");
    ASSERT_ERR(iotdata_encode_image(&enc, 0, 0, 0, 0, NULL, 1), IOTDATA_ERR_IMAGE_DATA_NULL, "data null");
    ASSERT_ERR(iotdata_encode_image(&enc, 0, 0, 0, 0, px, 255), IOTDATA_ERR_IMAGE_DATA_HIGH, "data high");
    PASS();
}

static void test_tlv_errors(void) {
    TEST("TLV error conditions");
    begin(0, 1, 23);

    /* Type too high */
    uint8_t raw[] = { 0x01 };
    ASSERT_ERR(iotdata_encode_tlv(&enc, 64, raw, 1), IOTDATA_ERR_TLV_TYPE_HIGH, "type high");

    /* Data null */
    ASSERT_ERR(iotdata_encode_tlv(&enc, 1, NULL, 1), IOTDATA_ERR_TLV_DATA_NULL, "data null");

    /* String null */
    ASSERT_ERR(iotdata_encode_tlv_string(&enc, 1, NULL), IOTDATA_ERR_TLV_STR_NULL, "str null");

    /* Invalid 6-bit char */
    ASSERT_ERR(iotdata_encode_tlv_string(&enc, 1, "hello[world"), IOTDATA_ERR_TLV_STR_CHAR_INVALID, "str char invalid");

    /* TLV full (overflow IOTDATA_TLV_MAX) */
    for (int i = 0; i < IOTDATA_TLV_MAX; i++)
        ASSERT_OK(iotdata_encode_tlv(&enc, 0x20, raw, 1), "fill");
    ASSERT_ERR(iotdata_encode_tlv(&enc, 0x20, raw, 1), IOTDATA_ERR_TLV_FULL, "full");

    /* KV mismatch (odd count) */
    begin(0, 1, 24);
    const char *kv_odd[] = { "key1", "val1", "key2" };
    char kv_buf[256];
    ASSERT_ERR(iotdata_encode_tlv_type_version(&enc, kv_odd, 3, false, kv_buf, sizeof(kv_buf)), IOTDATA_ERR_TLV_KV_MISMATCH, "kv odd");
    PASS();
}

static void test_encode_buffer_overflow(void) {
    TEST("Encode buffer overflow");

    /* 5 bytes = header(4) + pres0(1), no room for field data */
    uint8_t small_buf[5];
    iotdata_encoder_t small_enc;
    ASSERT_OK(iotdata_encode_begin(&small_enc, small_buf, 5, 0, 1, 1), "begin ok");
    ASSERT_OK(iotdata_encode_battery(&small_enc, 50, false), "bat ok");
    size_t out_len;
    ASSERT_ERR(iotdata_encode_end(&small_enc, &out_len), IOTDATA_ERR_BUF_TOO_SMALL, "buf overflow");

    /* Buffer too small for even the header */
    ASSERT_ERR(iotdata_encode_begin(&small_enc, small_buf, 4, 0, 1, 1), IOTDATA_ERR_BUF_TOO_SMALL, "buf tiny");
    PASS();
}

/* =========================================================================
 * Section 5: Peek function
 * =========================================================================*/

static void test_peek_basic(void) {
    TEST("Peek basic");
    begin(0, 42, 1234);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();

    uint8_t v;
    uint16_t s, q;
    ASSERT_OK(iotdata_peek(pkt, pkt_len, &v, &s, &q), "peek");
    ASSERT_EQ(v, 0, "variant");
    ASSERT_EQ_U(s, 42, "station");
    ASSERT_EQ_U(q, 1234, "sequence");
    PASS();
}

static void test_peek_null_params(void) {
    TEST("Peek with NULL output params");
    begin(0, 10, 99);
    finish();

    /* All NULL except buf/len */
    ASSERT_OK(iotdata_peek(pkt, pkt_len, NULL, NULL, NULL), "all null");

    /* Partial NULL */
    uint8_t v;
    ASSERT_OK(iotdata_peek(pkt, pkt_len, &v, NULL, NULL), "var only");
    ASSERT_EQ(v, 0, "variant");

    uint16_t s;
    ASSERT_OK(iotdata_peek(pkt, pkt_len, NULL, &s, NULL), "station only");
    ASSERT_EQ_U(s, 10, "station");
    PASS();
}

static void test_peek_short_buffer(void) {
    TEST("Peek short buffer");

    uint8_t short_buf[] = { 0x00, 0x00, 0x00 };
    uint8_t v;
    ASSERT_ERR(iotdata_peek(short_buf, 3, &v, NULL, NULL), IOTDATA_ERR_DECODE_SHORT, "short");
    PASS();
}

static void test_peek_reserved_variant(void) {
    TEST("Peek reserved variant (15)");

    /* Manually construct: variant=15 (0xF), station=0, sequence=0 */
    uint8_t bad[] = { 0xF0, 0x00, 0x00, 0x00, 0x00 };
    uint8_t v;
    ASSERT_ERR(iotdata_peek(bad, 5, &v, NULL, NULL), IOTDATA_ERR_DECODE_VARIANT, "reserved");
    PASS();
}

/* =========================================================================
 * Section 6: TLV typed helpers
 * =========================================================================*/

static void test_tlv_version_round_trip(void) {
    TEST("TLV version (KV) round-trip");
    begin(0, 1, 30);

    const char *kv[] = { "FW", "142", "HW", "3" };
    char kv_buf[256];
    ASSERT_OK(iotdata_encode_tlv_type_version(&enc, kv, 4, false, kv_buf, sizeof(kv_buf)), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv_count, 1, "count");
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_VERSION, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_STRING, "fmt");
    ASSERT_EQ(strcmp(dec.tlv[0].str, "FW 142 HW 3"), 0, "str");
    PASS();
}

static void test_tlv_status_round_trip(void) {
    TEST("TLV status round-trip");
    begin(0, 1, 31);

    uint8_t status_buf[9];
    ASSERT_OK(iotdata_encode_tlv_type_status(&enc, 3600, 86400, 5, IOTDATA_TLV_REASON_POWER_ON, status_buf), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv_count, 1, "count");
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_STATUS, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_RAW, "fmt");
    ASSERT_EQ(dec.tlv[0].length, 9, "len");
    /* Verify raw bytes: session=3600/5=720=0x0002D0 */
    ASSERT_EQ(dec.tlv[0].raw[0], 0x00, "sess0");
    ASSERT_EQ(dec.tlv[0].raw[1], 0x02, "sess1");
    ASSERT_EQ(dec.tlv[0].raw[2], 0xD0, "sess2");
    /* lifetime=86400/5=17280=0x004380 */
    ASSERT_EQ(dec.tlv[0].raw[3], 0x00, "life0");
    ASSERT_EQ(dec.tlv[0].raw[4], 0x43, "life1");
    ASSERT_EQ(dec.tlv[0].raw[5], 0x80, "life2");
    /* restarts=5=0x0005 */
    ASSERT_EQ(dec.tlv[0].raw[6], 0x00, "rst0");
    ASSERT_EQ(dec.tlv[0].raw[7], 0x05, "rst1");
    /* reason=POWER_ON=1 */
    ASSERT_EQ(dec.tlv[0].raw[8], 0x01, "reason");
    PASS();
}

static void test_tlv_health_round_trip(void) {
    TEST("TLV health round-trip");
    begin(0, 1, 32);

    uint8_t health_buf[7];
    ASSERT_OK(iotdata_encode_tlv_type_health(&enc, 42, 3300, 32768, 1800, health_buf), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv_count, 1, "count");
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_HEALTH, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_RAW, "fmt");
    ASSERT_EQ(dec.tlv[0].length, 7, "len");
    /* cpu_temp=42=0x2A */
    ASSERT_EQ(dec.tlv[0].raw[0], 0x2A, "cpu");
    /* supply_mv=3300=0x0CE4 */
    ASSERT_EQ(dec.tlv[0].raw[1], 0x0C, "mv0");
    ASSERT_EQ(dec.tlv[0].raw[2], 0xE4, "mv1");
    /* free_heap=32768=0x8000 */
    ASSERT_EQ(dec.tlv[0].raw[3], 0x80, "heap0");
    ASSERT_EQ(dec.tlv[0].raw[4], 0x00, "heap1");
    /* active=1800/5=360=0x0168 */
    ASSERT_EQ(dec.tlv[0].raw[5], 0x01, "act0");
    ASSERT_EQ(dec.tlv[0].raw[6], 0x68, "act1");
    PASS();
}

static void test_tlv_config_round_trip(void) {
    TEST("TLV config (KV) round-trip");
    begin(0, 1, 33);

    const char *kv[] = { "mode", "auto", "rate", "60" };
    char kv_buf[256];
    ASSERT_OK(iotdata_encode_tlv_type_config(&enc, kv, 4, false, kv_buf, sizeof(kv_buf)), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv_count, 1, "count");
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_CONFIG, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_STRING, "fmt");
    ASSERT_EQ(strcmp(dec.tlv[0].str, "mode auto rate 60"), 0, "str");
    PASS();
}

static void test_tlv_diagnostic_round_trip(void) {
    TEST("TLV diagnostic round-trip (string + raw)");

    /* String mode */
    begin(0, 1, 34);
    ASSERT_OK(iotdata_encode_tlv_type_diagnostic(&enc, "system ok", false), "str");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_DIAGNOSTIC, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_STRING, "fmt_str");
    ASSERT_EQ(strcmp(dec.tlv[0].str, "system ok"), 0, "str_val");

    /* Raw mode */
    begin(0, 1, 35);
    ASSERT_OK(iotdata_encode_tlv_type_diagnostic(&enc, "error42", true), "raw");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_DIAGNOSTIC, "type_raw");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_RAW, "fmt_raw");
    ASSERT_EQ(dec.tlv[0].length, 7, "len_raw");
    ASSERT_EQ(memcmp(dec.tlv[0].raw, "error42", 7), 0, "raw_val");
    PASS();
}

static void test_tlv_userdata_round_trip(void) {
    TEST("TLV userdata round-trip");
    begin(0, 1, 36);

    ASSERT_OK(iotdata_encode_tlv_type_userdata(&enc, "boot event", false), "encode");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_USERDATA, "type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_STRING, "fmt");
    ASSERT_EQ(strcmp(dec.tlv[0].str, "boot event"), 0, "str");
    PASS();
}

static void test_tlv_multiple(void) {
    TEST("Multiple TLV entries in one packet");
    begin(0, 1, 37);

    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");

    const char *ver_kv[] = { "FW", "100" };
    char kv_buf[256];
    ASSERT_OK(iotdata_encode_tlv_type_version(&enc, ver_kv, 2, false, kv_buf, sizeof(kv_buf)), "ver");

    uint8_t status_buf[9];
    ASSERT_OK(iotdata_encode_tlv_type_status(&enc, 600, 7200, 1, IOTDATA_TLV_REASON_SOFTWARE, status_buf), "status");

    ASSERT_OK(iotdata_encode_tlv_type_userdata(&enc, "test data", false), "user");

    finish();
    decode_pkt();

    ASSERT_EQ(dec.tlv_count, 3, "count");
    ASSERT_EQ(dec.tlv[0].type, IOTDATA_TLV_VERSION, "t0");
    ASSERT_EQ(dec.tlv[1].type, IOTDATA_TLV_STATUS, "t1");
    ASSERT_EQ(dec.tlv[2].type, IOTDATA_TLV_USERDATA, "t2");
    PASS();
}

/* =========================================================================
 * Section 7: JSON round-trip
 * =========================================================================*/

static void test_json_round_trip_complete(void) {
    TEST("JSON round-trip (complete variant)");
    begin(0, 10, 999);

    ASSERT_OK(iotdata_encode_battery(&enc, 80, true), "bat");
    ASSERT_OK(iotdata_encode_link(&enc, -80, 0.0f), "link");
    ASSERT_OK(iotdata_encode_environment(&enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 8.0f, 225, 12.0f), "wind");
    ASSERT_OK(iotdata_encode_rain(&enc, 5, 20), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 300, 5), "solar");
    ASSERT_OK(iotdata_encode_clouds(&enc, 4), "cloud");
    ASSERT_OK(iotdata_encode_air_quality_index(&enc, 75), "aqi");
    uint16_t pm[4] = { 50, 120, 80, 200 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm), "pm");
    uint16_t gas[8] = { 200, 100, 5000, 500, 250, 100, 0, 0 };
    ASSERT_OK(iotdata_encode_air_quality_gas(&enc, 0x3F, gas), "gas");
    ASSERT_OK(iotdata_encode_radiation(&enc, 100, 0.50f), "rad");
    ASSERT_OK(iotdata_encode_depth(&enc, 250), "depth");
    ASSERT_OK(iotdata_encode_position(&enc, 51.5, -0.1), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 86400), "dt");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x42), "flags");
    finish();

    /* Binary → JSON */
    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    /* JSON → binary */
    uint8_t pkt2[256];
    size_t len2;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

static void test_json_round_trip_with_tlv(void) {
    TEST("JSON round-trip with TLV");
    begin(0, 5, 100);

    ASSERT_OK(iotdata_encode_battery(&enc, 60, false), "bat");

    /* Add a raw TLV and a string TLV */
    uint8_t raw[] = { 0xDE, 0xAD };
    ASSERT_OK(iotdata_encode_tlv(&enc, 0x20, raw, 2), "tlv raw");
    ASSERT_OK(iotdata_encode_tlv_string(&enc, 0x21, "hello world"), "tlv str");

    /* Also add a userdata TLV (string mode) */
    ASSERT_OK(iotdata_encode_tlv_type_userdata(&enc, "test note", false), "userdata");

    finish();

    /* Binary → JSON */
    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    /* JSON → binary (zero scratch: iotdata_encode_begin does not reset tlv_count) */
    uint8_t pkt2[256];
    size_t len2;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    /* Decode round-tripped packet and verify TLVs */
    ASSERT_OK(iotdata_decode(pkt2, len2, &dec), "decode2");
    ASSERT_EQ(dec.tlv_count, 3, "tlv count");
    ASSERT_EQ(dec.tlv[0].type, 0x20, "t0 type");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_RAW, "t0 fmt");
    ASSERT_EQ(dec.tlv[1].type, 0x21, "t1 type");
    ASSERT_EQ(dec.tlv[1].format, IOTDATA_TLV_FMT_STRING, "t1 fmt");
    ASSERT_EQ(strcmp(dec.tlv[1].str, "hello world"), 0, "t1 str");
    ASSERT_EQ(dec.tlv[2].type, IOTDATA_TLV_USERDATA, "t2 type");
    PASS();
}

static void test_json_round_trip_standalone(void) {
    TEST("JSON round-trip (standalone variant)");
    begin(1, 20, 400);

    ASSERT_OK(iotdata_encode_battery(&enc, 90, true), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, -10.0f), "temp");
    ASSERT_OK(iotdata_encode_pressure(&enc, 950), "pres");
    ASSERT_OK(iotdata_encode_humidity(&enc, 80), "hum");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 15.0f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 270), "wdir");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 25.0f), "wgust");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 20), "rrate");
    ASSERT_OK(iotdata_encode_rain_size(&enc, 8), "rsize");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 200), "cpm");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 1.50f), "dose");
    ASSERT_OK(iotdata_encode_depth(&enc, 500), "depth");
    ASSERT_OK(iotdata_encode_position(&enc, 35.6762, 139.6503), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 43200), "dt");
    ASSERT_OK(iotdata_encode_flags(&enc, 0xAA), "flags");
    finish();

    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    uint8_t pkt2[256];
    size_t len2;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

/* =========================================================================
 * Section 8: Decode error paths
 * =========================================================================*/

static void test_decode_short(void) {
    TEST("Decode short buffer");

    uint8_t short_buf[] = { 0x00, 0x00, 0x00 };
    ASSERT_ERR(iotdata_decode(short_buf, 3, &dec), IOTDATA_ERR_DECODE_SHORT, "short");
    PASS();
}

static void test_decode_truncated(void) {
    TEST("Decode truncated (field data missing)");

    /* Encode a packet with battery, then truncate to header+pres0 only */
    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();

    /* Full packet is 6 bytes; truncate to 5 (header + pres0, no field data) */
    ASSERT_ERR(iotdata_decode(pkt, 5, &dec), IOTDATA_ERR_DECODE_TRUNCATED, "truncated");
    PASS();
}

static void test_decode_reserved_variant(void) {
    TEST("Decode reserved variant (15)");

    /* Manually construct: variant=15, station=0, seq=0, pres0=0 */
    uint8_t bad[] = { 0xF0, 0x00, 0x00, 0x00, 0x00 };
    ASSERT_ERR(iotdata_decode(bad, 5, &dec), IOTDATA_ERR_DECODE_VARIANT, "reserved");
    PASS();
}

/* =========================================================================
 * Section 9: Dump and print
 * =========================================================================*/

static void test_dump_complete_variant(void) {
    TEST("Dump complete variant");
    begin(0, 5, 42);

    ASSERT_OK(iotdata_encode_battery(&enc, 90, false), "bat");
    ASSERT_OK(iotdata_encode_depth(&enc, 300), "depth");
    uint16_t pm[4] = { 100, 200, 150, 300 };
    ASSERT_OK(iotdata_encode_air_quality_pm(&enc, 0x0F, pm), "pm");
    finish();

    char str[8192];
    iotdata_dump_t dump;
    ASSERT_OK(iotdata_dump_to_string(&dump, pkt, pkt_len, str, sizeof(str), true), "dump");
    if (!strstr(str, "variant")) {
        FAIL("missing variant");
        return;
    }
    if (!strstr(str, "battery")) {
        FAIL("missing battery");
        return;
    }
    PASS();
}

static void test_print_complete_variant(void) {
    TEST("Print complete variant");
    begin(0, 7, 100);

    ASSERT_OK(iotdata_encode_battery(&enc, 60, true), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 15.0f, 1000, 70), "env");
    ASSERT_OK(iotdata_encode_depth(&enc, 200), "depth");
    finish();

    char str[8192];
    iotdata_print_scratch_t print_scratch;
    ASSERT_OK(iotdata_print_to_string(pkt, pkt_len, str, sizeof(str), &print_scratch), "print");
    if (!strstr(str, "complete")) {
        FAIL("missing variant name");
        return;
    }
    PASS();
}

/* =========================================================================
 * Section 10: Image compression utilities
 * =========================================================================*/

static void test_image_rle_round_trip(void) {
    TEST("Image RLE compress/decompress");

    /* 128 bilevel pixels: 64 white + 64 black */
    uint8_t pixels[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    uint8_t compressed[64];
    size_t comp_len = iotdata_image_rle_compress(pixels, 128, 1, compressed, sizeof(compressed));
    if (comp_len == 0) {
        FAIL("compress failed");
        return;
    }

    uint8_t decompressed[16] = { 0x42 };
    size_t decomp_len = iotdata_image_rle_decompress(compressed, comp_len, 1, decompressed, sizeof(decompressed));
    if (decomp_len == 0) {
        FAIL("decompress failed");
        return;
    }

    ASSERT_EQ(memcmp(pixels, decompressed, 16), 0, "round-trip");
    PASS();
}

static void test_image_heatshrink_round_trip(void) {
    TEST("Image heatshrink compress/decompress");

    /* Repetitive pattern: 64 bytes of repeating 0,1,2,3 */
    uint8_t raw[64];
    for (int i = 0; i < 64; i++)
        raw[i] = (uint8_t)(i & 3);

    uint8_t compressed[128];
    size_t comp_len = iotdata_image_hs_compress(raw, 64, compressed, sizeof(compressed));
    if (comp_len == 0) {
        FAIL("compress failed");
        return;
    }

    uint8_t decompressed[64] = { 0xFF };
    size_t decomp_len = iotdata_image_hs_decompress(compressed, comp_len, decompressed, sizeof(decompressed));
    if (decomp_len == 0) {
        FAIL("decompress failed");
        return;
    }

    ASSERT_EQ(memcmp(raw, decompressed, 64), 0, "round-trip");
    PASS();
}

/* =========================================================================
 * Main
 * =========================================================================*/

int main(void) {
    printf("\n=== iotdata — comprehensive test suite ===\n\n");

    printf("--- Section 1: Field round-trips (new types) ---\n");
    test_aq_pm_round_trip();
    test_aq_pm_partial();
    test_aq_gas_round_trip();
    test_depth_round_trip();
    test_image_round_trip();

    printf("\n--- Section 2: Full variant tests ---\n");
    test_complete_variant_all_fields();
    test_standalone_variant_all_fields();

    printf("\n--- Section 3: Boundary values ---\n");
    test_aq_pm_boundaries();
    test_aq_gas_boundaries();
    test_depth_boundaries();
    test_image_flags_combinations();

    printf("\n--- Section 4: Error conditions ---\n");
    test_aq_pm_errors();
    test_aq_gas_errors();
    test_image_errors();
    test_tlv_errors();
    test_encode_buffer_overflow();

    printf("\n--- Section 5: Peek ---\n");
    test_peek_basic();
    test_peek_null_params();
    test_peek_short_buffer();
    test_peek_reserved_variant();

    printf("\n--- Section 6: TLV typed helpers ---\n");
    test_tlv_version_round_trip();
    test_tlv_status_round_trip();
    test_tlv_health_round_trip();
    test_tlv_config_round_trip();
    test_tlv_diagnostic_round_trip();
    test_tlv_userdata_round_trip();
    test_tlv_multiple();

    printf("\n--- Section 7: JSON round-trip ---\n");
    test_json_round_trip_complete();
    test_json_round_trip_with_tlv();
    test_json_round_trip_standalone();

    printf("\n--- Section 8: Decode error paths ---\n");
    test_decode_short();
    test_decode_truncated();
    test_decode_reserved_variant();

    printf("\n--- Section 9: Dump and print ---\n");
    test_dump_complete_variant();
    test_print_complete_variant();

    printf("\n--- Section 10: Image compression ---\n");
    test_image_rle_round_trip();
    test_image_heatshrink_round_trip();

    printf("\n--- Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ---\n\n");

    return tests_failed > 0 ? 1 : 0;
}
