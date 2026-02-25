/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * test_failures.c - negative, boundary, failure, and edge-case test suite
 *
 * Exercises error paths, boundary values, negative values, null inputs,
 * truncated packets, malformed data, and quantisation edge cases that
 * are not covered by test_complete.c (which focuses on happy paths).
 *
 * Uses the same two-variant layout as test_complete.c.
 */

#include "test_common.h"

/* ---------------------------------------------------------------------------
 * Custom variant definitions (same as test_complete.c)
 * -------------------------------------------------------------------------*/

const iotdata_variant_def_t failure_variants[2] = {
    [0] = {
        .name = "complete",
        .num_pres_bytes = 3,
        .fields = {
            { IOTDATA_FIELD_BATTERY,           "battery" },
            { IOTDATA_FIELD_LINK,              "link" },
            { IOTDATA_FIELD_ENVIRONMENT,       "environment" },
            { IOTDATA_FIELD_WIND,              "wind" },
            { IOTDATA_FIELD_RAIN,              "rain" },
            { IOTDATA_FIELD_SOLAR,             "solar" },
            { IOTDATA_FIELD_CLOUDS,            "clouds" },
            { IOTDATA_FIELD_AIR_QUALITY_INDEX, "air_quality" },
            { IOTDATA_FIELD_AIR_QUALITY_PM,    "air_quality_pm" },
            { IOTDATA_FIELD_AIR_QUALITY_GAS,   "air_quality_gas" },
            { IOTDATA_FIELD_RADIATION,         "radiation" },
            { IOTDATA_FIELD_DEPTH,             "depth" },
            { IOTDATA_FIELD_POSITION,          "position" },
            { IOTDATA_FIELD_DATETIME,          "datetime" },
            { IOTDATA_FIELD_IMAGE,             "image" },
            { IOTDATA_FIELD_FLAGS,             "flags" },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
            { IOTDATA_FIELD_NONE,              NULL },
        },
    },
    [1] = {
        .name = "standalone",
        .num_pres_bytes = 3,
        .fields = {
            { IOTDATA_FIELD_BATTERY,           "battery" },
            { IOTDATA_FIELD_TEMPERATURE,       "temperature" },
            { IOTDATA_FIELD_PRESSURE,          "pressure" },
            { IOTDATA_FIELD_HUMIDITY,          "humidity" },
            { IOTDATA_FIELD_WIND_SPEED,        "wind_speed" },
            { IOTDATA_FIELD_WIND_DIRECTION,    "wind_direction" },
            { IOTDATA_FIELD_WIND_GUST,         "wind_gust" },
            { IOTDATA_FIELD_RAIN_RATE,         "rain_rate" },
            { IOTDATA_FIELD_RAIN_SIZE,         "rain_size" },
            { IOTDATA_FIELD_RADIATION_CPM,     "radiation_cpm" },
            { IOTDATA_FIELD_RADIATION_DOSE,    "radiation_dose" },
            { IOTDATA_FIELD_DEPTH,             "depth" },
            { IOTDATA_FIELD_POSITION,          "position" },
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
 * Section 1: Negative value round-trips
 * =========================================================================*/

static void test_negative_temperature(void) {
    TEST("Negative temperature round-trip");
    begin(1, 1, 1);
    ASSERT_OK(iotdata_encode_temperature(&enc, -40.0f), "encode -40");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, -40.0, 0.25, "temp -40");

    begin(1, 1, 2);
    ASSERT_OK(iotdata_encode_temperature(&enc, -10.5f), "encode -10.5");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, -10.5, 0.25, "temp -10.5");

    begin(1, 1, 3);
    ASSERT_OK(iotdata_encode_temperature(&enc, -0.25f), "encode -0.25");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, -0.25, 0.25, "temp -0.25");
    PASS();
}

static void test_negative_rssi(void) {
    TEST("Negative RSSI round-trip");
    begin(0, 1, 10);
    ASSERT_OK(iotdata_encode_link(&enc, -120, 0.0f), "encode rssi -120");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.link_rssi, -120, "rssi min");

    begin(0, 1, 11);
    ASSERT_OK(iotdata_encode_link(&enc, -60, 0.0f), "encode rssi -60");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.link_rssi, -60, "rssi max");

    begin(0, 1, 12);
    ASSERT_OK(iotdata_encode_link(&enc, -90, 0.0f), "encode rssi -90");
    finish();
    decode_pkt();
    /* -90 quantised to 4-bit with step 4: (-90 - -120)/4 = 7.5, rounds to 8, dequant = -120 + 8*4 = -88 */
    ASSERT_TRUE(dec.link_rssi >= -92 && dec.link_rssi <= -88, "rssi mid range");
    PASS();
}

static void test_negative_snr(void) {
    TEST("Negative SNR round-trip");
    begin(0, 1, 20);
    ASSERT_OK(iotdata_encode_link(&enc, -80, -20.0f), "encode snr -20");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.link_snr, -20.0, 10.0, "snr min");

    begin(0, 1, 21);
    ASSERT_OK(iotdata_encode_link(&enc, -80, -10.0f), "encode snr -10");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.link_snr, -10.0, 10.0, "snr -10");
    PASS();
}

static void test_negative_position(void) {
    TEST("Negative position (lat/lon) round-trip");

    /* South, West */
    begin(1, 1, 30);
    ASSERT_OK(iotdata_encode_position(&enc, -90.0, -180.0), "encode south pole");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, -90.0, 0.01, "lat -90");
    ASSERT_NEAR(dec.position_lon, -180.0, 0.01, "lon -180");

    /* Near zero */
    begin(1, 1, 31);
    ASSERT_OK(iotdata_encode_position(&enc, -0.001, -0.001), "encode near zero neg");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, -0.001, 0.01, "lat near zero");
    ASSERT_NEAR(dec.position_lon, -0.001, 0.01, "lon near zero");

    /* Southern hemisphere city */
    begin(1, 1, 32);
    ASSERT_OK(iotdata_encode_position(&enc, -33.8688, 151.2093), "encode sydney");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, -33.8688, 0.002, "lat sydney");
    ASSERT_NEAR(dec.position_lon, 151.2093, 0.002, "lon sydney");
    PASS();
}

/* =========================================================================
 * Section 2: Boundary value round-trips
 * =========================================================================*/

static void test_battery_boundaries(void) {
    TEST("Battery level boundaries (0, 100)");

    begin(0, 1, 40);
    ASSERT_OK(iotdata_encode_battery(&enc, 0, false), "encode 0");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.battery_level, 0, "level 0");
    ASSERT_EQ(dec.battery_charging, false, "not charging");

    begin(0, 1, 41);
    ASSERT_OK(iotdata_encode_battery(&enc, 100, true), "encode 100");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.battery_level, 100, "level 100");
    ASSERT_EQ(dec.battery_charging, true, "charging");
    PASS();
}

static void test_temperature_boundaries(void) {
    TEST("Temperature boundaries (-40, 0, 80)");

    begin(1, 1, 50);
    ASSERT_OK(iotdata_encode_temperature(&enc, -40.0f), "encode min");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, -40.0, 0.25, "temp min");

    begin(1, 1, 51);
    ASSERT_OK(iotdata_encode_temperature(&enc, 0.0f), "encode zero");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, 0.0, 0.25, "temp zero");

    begin(1, 1, 52);
    ASSERT_OK(iotdata_encode_temperature(&enc, 80.0f), "encode max");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.temperature, 80.0, 0.25, "temp max");
    PASS();
}

static void test_pressure_boundaries(void) {
    TEST("Pressure boundaries (850, 1105)");

    begin(1, 1, 55);
    ASSERT_OK(iotdata_encode_pressure(&enc, 850), "encode min");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.pressure, 850, "pres min");

    begin(1, 1, 56);
    ASSERT_OK(iotdata_encode_pressure(&enc, 1105), "encode max");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.pressure, 1105, "pres max");
    PASS();
}

static void test_wind_speed_boundaries(void) {
    TEST("Wind speed boundaries (0, max)");

    begin(1, 1, 60);
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 0.0f), "encode zero");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_speed, 0.0, 0.5, "speed zero");

    begin(1, 1, 61);
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 63.5f), "encode max");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_speed, 63.5, 0.5, "speed max");
    PASS();
}

static void test_wind_direction_boundaries(void) {
    TEST("Wind direction boundaries (0, 359)");

    begin(1, 1, 65);
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 0), "encode 0");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_direction, 0, 2.0, "dir 0");

    begin(1, 1, 66);
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 359), "encode 359");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.wind_direction, 359, 2.0, "dir 359");
    PASS();
}

static void test_position_boundaries(void) {
    TEST("Position boundaries (poles, antimeridian)");

    /* North pole */
    begin(1, 1, 70);
    ASSERT_OK(iotdata_encode_position(&enc, 90.0, 0.0), "encode north pole");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, 90.0, 0.01, "lat 90");

    /* South pole */
    begin(1, 1, 71);
    ASSERT_OK(iotdata_encode_position(&enc, -90.0, 0.0), "encode south pole");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, -90.0, 0.01, "lat -90");

    /* East extreme */
    begin(1, 1, 72);
    ASSERT_OK(iotdata_encode_position(&enc, 0.0, 179.999), "encode east extreme");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lon, 179.999, 0.01, "lon east");

    /* West extreme */
    begin(1, 1, 73);
    ASSERT_OK(iotdata_encode_position(&enc, 0.0, -180.0), "encode west extreme");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lon, -180.0, 0.01, "lon west");

    /* Origin */
    begin(1, 1, 74);
    ASSERT_OK(iotdata_encode_position(&enc, 0.0, 0.0), "encode origin");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.position_lat, 0.0, 0.01, "lat origin");
    ASSERT_NEAR(dec.position_lon, 0.0, 0.01, "lon origin");
    PASS();
}

static void test_datetime_boundaries(void) {
    TEST("Datetime boundaries (0, max)");

    begin(1, 1, 80);
    ASSERT_OK(iotdata_encode_datetime(&enc, 0), "encode 0");
    finish();
    decode_pkt();
    /* datetime is quantised by resolution of 5 */
    ASSERT_EQ_U(dec.datetime_secs, 0, "dt zero");

    begin(1, 1, 81);
    /* max raw value = (1<<24)-1 = 16777215, max seconds = 16777215 * 5 = 83886075 */
    ASSERT_OK(iotdata_encode_datetime(&enc, 83886075), "encode max");
    finish();
    decode_pkt();
    ASSERT_NEAR((double)dec.datetime_secs, 83886075.0, 5.0, "dt max");
    PASS();
}

static void test_radiation_boundaries(void) {
    TEST("Radiation CPM/dose boundaries");

    begin(1, 1, 85);
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 0), "encode cpm 0");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.radiation_cpm, 0, "cpm zero");

    begin(1, 1, 86);
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 16383), "encode cpm max");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.radiation_cpm, 16383, "cpm max");

    begin(1, 1, 87);
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 0.0f), "encode dose 0");
    finish();
    decode_pkt();
    ASSERT_NEAR(dec.radiation_dose, 0.0, 0.01, "dose zero");
    PASS();
}

static void test_clouds_boundaries(void) {
    TEST("Clouds boundaries (0, 8)");

    begin(0, 1, 90);
    ASSERT_OK(iotdata_encode_clouds(&enc, 0), "encode 0");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.clouds, 0, "clouds 0");

    begin(0, 1, 91);
    ASSERT_OK(iotdata_encode_clouds(&enc, 8), "encode 8");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.clouds, 8, "clouds 8");
    PASS();
}

static void test_solar_boundaries(void) {
    TEST("Solar boundaries (irradiance 0/1023, UV 0/15)");

    begin(0, 1, 95);
    ASSERT_OK(iotdata_encode_solar(&enc, 0, 0), "encode zero");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.solar_irradiance, 0, "irr 0");
    ASSERT_EQ(dec.solar_ultraviolet, 0, "uv 0");

    begin(0, 1, 96);
    ASSERT_OK(iotdata_encode_solar(&enc, 1023, 15), "encode max");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.solar_irradiance, 1023, "irr max");
    ASSERT_EQ(dec.solar_ultraviolet, 15, "uv max");
    PASS();
}

static void test_header_boundaries(void) {
    TEST("Header boundaries (station/sequence max)");

    begin(0, 4095, 65535);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.station, 4095, "station max");
    ASSERT_EQ_U(dec.sequence, 65535, "seq max");

    begin(0, 0, 0);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.station, 0, "station 0");
    ASSERT_EQ_U(dec.sequence, 0, "seq 0");
    PASS();
}

static void test_humidity_boundaries(void) {
    TEST("Humidity boundaries (0, 100)");

    begin(1, 1, 100);
    ASSERT_OK(iotdata_encode_humidity(&enc, 0), "encode 0");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.humidity, 0, "hum 0");

    begin(1, 1, 101);
    ASSERT_OK(iotdata_encode_humidity(&enc, 100), "encode 100");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.humidity, 100, "hum 100");
    PASS();
}

static void test_flags_boundaries(void) {
    TEST("Flags boundaries (0x00, 0xFF)");

    begin(1, 1, 105);
    ASSERT_OK(iotdata_encode_flags(&enc, 0x00), "encode 0");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.flags, 0x00, "flags 0");

    begin(1, 1, 106);
    ASSERT_OK(iotdata_encode_flags(&enc, 0xFF), "encode FF");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.flags, 0xFF, "flags FF");
    PASS();
}

/* =========================================================================
 * Section 3: Field value error conditions (out of range)
 * =========================================================================*/

static void test_battery_errors(void) {
    TEST("Battery level > 100");
    begin(0, 1, 200);
    ASSERT_ERR(iotdata_encode_battery(&enc, 101, false), IOTDATA_ERR_BATTERY_LEVEL_HIGH, "bat high");
    PASS();
}

static void test_temperature_errors(void) {
    TEST("Temperature out of range");
    begin(1, 1, 201);
    ASSERT_ERR(iotdata_encode_temperature(&enc, -40.5f), IOTDATA_ERR_TEMPERATURE_LOW, "temp low");
    ASSERT_ERR(iotdata_encode_temperature(&enc, 80.5f), IOTDATA_ERR_TEMPERATURE_HIGH, "temp high");
    PASS();
}

static void test_pressure_errors(void) {
    TEST("Pressure out of range");
    begin(1, 1, 202);
    ASSERT_ERR(iotdata_encode_pressure(&enc, 849), IOTDATA_ERR_PRESSURE_LOW, "pres low");
    ASSERT_ERR(iotdata_encode_pressure(&enc, 1106), IOTDATA_ERR_PRESSURE_HIGH, "pres high");
    PASS();
}

static void test_humidity_errors(void) {
    TEST("Humidity > 100");
    begin(1, 1, 203);
    ASSERT_ERR(iotdata_encode_humidity(&enc, 101), IOTDATA_ERR_HUMIDITY_HIGH, "hum high");
    PASS();
}

static void test_wind_errors(void) {
    TEST("Wind speed/direction/gust out of range");
    begin(1, 1, 204);
    ASSERT_ERR(iotdata_encode_wind_speed(&enc, 64.0f), IOTDATA_ERR_WIND_SPEED_HIGH, "speed high");
    ASSERT_ERR(iotdata_encode_wind_direction(&enc, 360), IOTDATA_ERR_WIND_DIRECTION_HIGH, "dir high");
    ASSERT_ERR(iotdata_encode_wind_gust(&enc, 64.0f), IOTDATA_ERR_WIND_GUST_HIGH, "gust high");
    PASS();
}

static void test_rssi_errors(void) {
    TEST("RSSI out of range");
    begin(0, 1, 205);
    ASSERT_ERR(iotdata_encode_link(&enc, -121, 0.0f), IOTDATA_ERR_LINK_RSSI_LOW, "rssi low");
    ASSERT_ERR(iotdata_encode_link(&enc, -59, 0.0f), IOTDATA_ERR_LINK_RSSI_HIGH, "rssi high");
    PASS();
}

static void test_snr_errors(void) {
    TEST("SNR out of range");
    begin(0, 1, 206);
    ASSERT_ERR(iotdata_encode_link(&enc, -80, -20.5f), IOTDATA_ERR_LINK_SNR_LOW, "snr low");
    ASSERT_ERR(iotdata_encode_link(&enc, -80, 10.5f), IOTDATA_ERR_LINK_SNR_HIGH, "snr high");
    PASS();
}

static void test_solar_errors(void) {
    TEST("Solar out of range");
    begin(0, 1, 207);
    ASSERT_ERR(iotdata_encode_solar(&enc, 1024, 0), IOTDATA_ERR_SOLAR_IRRADIATION_HIGH, "irr high");
    ASSERT_ERR(iotdata_encode_solar(&enc, 0, 16), IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH, "uv high");
    PASS();
}

static void test_clouds_errors(void) {
    TEST("Clouds > 8");
    begin(0, 1, 208);
    ASSERT_ERR(iotdata_encode_clouds(&enc, 9), IOTDATA_ERR_CLOUDS_HIGH, "clouds high");
    PASS();
}

static void test_depth_errors(void) {
    TEST("Depth > 1023");
    begin(0, 1, 209);
    ASSERT_ERR(iotdata_encode_depth(&enc, 1024), IOTDATA_ERR_DEPTH_HIGH, "depth high");
    PASS();
}

static void test_position_errors(void) {
    TEST("Position out of range");
    begin(1, 1, 210);
    ASSERT_ERR(iotdata_encode_position(&enc, -90.1, 0.0), IOTDATA_ERR_POSITION_LAT_LOW, "lat low");
    ASSERT_ERR(iotdata_encode_position(&enc, 90.1, 0.0), IOTDATA_ERR_POSITION_LAT_HIGH, "lat high");
    ASSERT_ERR(iotdata_encode_position(&enc, 0.0, -180.1), IOTDATA_ERR_POSITION_LON_LOW, "lon low");
    ASSERT_ERR(iotdata_encode_position(&enc, 0.0, 180.1), IOTDATA_ERR_POSITION_LON_HIGH, "lon high");
    PASS();
}

static void test_radiation_errors(void) {
    TEST("Radiation CPM/dose out of range");
    begin(1, 1, 211);
    ASSERT_ERR(iotdata_encode_radiation_cpm(&enc, 16384), IOTDATA_ERR_RADIATION_CPM_HIGH, "cpm high");
    ASSERT_ERR(iotdata_encode_radiation_dose(&enc, 164.0f), IOTDATA_ERR_RADIATION_DOSE_HIGH, "dose high");
    PASS();
}

static void test_aq_index_errors(void) {
    TEST("AQ index > 500");
    begin(0, 1, 212);
    ASSERT_ERR(iotdata_encode_air_quality_index(&enc, 501), IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH, "aqi high");
    PASS();
}

static void test_rain_errors(void) {
    TEST("Rain rate/size at limits");
    begin(1, 1, 213);
    /* rain_rate is uint8_t so max 255 = IOTDATA_RAIN_RATE_MAX, can't exceed */
    /* rain_size input max is RAIN_SIZE_MAX * RAIN_SIZE_SCALE = 15 * 4 = 60 */
    ASSERT_ERR(iotdata_encode_rain_size(&enc, 61), IOTDATA_ERR_RAIN_SIZE_HIGH, "size high");
    PASS();
}

static void test_datetime_errors(void) {
    TEST("Datetime exceeds max");
    begin(1, 1, 214);
    /* IOTDATA_DATETIME_MAX = (1<<24)-1 = 16777215, first failing input = (16777215+1) * 5 = 83886080 (integer division) */
    ASSERT_ERR(iotdata_encode_datetime(&enc, 83886080), IOTDATA_ERR_DATETIME_HIGH, "dt high");
    PASS();
}

/* =========================================================================
 * Section 4: Encoder state errors
 * =========================================================================*/

static void test_null_encoder(void) {
    TEST("Null encoder context");
    ASSERT_ERR(iotdata_encode_begin(NULL, pkt, sizeof(pkt), 0, 1, 1), IOTDATA_ERR_CTX_NULL, "null enc");
    PASS();
}

static void test_null_buffer(void) {
    TEST("Null output buffer");
    ASSERT_ERR(iotdata_encode_begin(&enc, NULL, 256, 0, 1, 1), IOTDATA_ERR_BUF_NULL, "null buf");
    PASS();
}

static void test_encode_before_begin(void) {
    TEST("Encode field before begin");
    iotdata_encoder_t fresh;
    memset(&fresh, 0, sizeof(fresh));
    ASSERT_ERR(iotdata_encode_battery(&fresh, 50, false), IOTDATA_ERR_CTX_NOT_BEGUN, "not begun");
    PASS();
}

static void test_double_begin(void) {
    TEST("Double begin (re-init allowed)");
    begin(0, 1, 1);
    /* iotdata_encode_begin allows re-initialisation — this is by design */
    ASSERT_OK(iotdata_encode_begin(&enc, pkt, sizeof(pkt), 0, 2, 2), "re-begin OK");
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "field after re-begin");
    finish();
    decode_pkt();
    ASSERT_EQ_U(dec.station, 2, "station from re-begin");
    ASSERT_EQ_U(dec.sequence, 2, "seq from re-begin");
    PASS();
}

static void test_encode_after_end(void) {
    TEST("Encode after end");
    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    ASSERT_ERR(iotdata_encode_battery(&enc, 50, false), IOTDATA_ERR_CTX_ALREADY_ENDED, "after end");
    PASS();
}

static void test_duplicate_field(void) {
    TEST("Duplicate field encode");
    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "first");
    ASSERT_ERR(iotdata_encode_battery(&enc, 60, false), IOTDATA_ERR_CTX_DUPLICATE_FIELD, "duplicate");
    finish();
    PASS();
}

static void test_variant_errors(void) {
    TEST("Invalid variant in begin");
    ASSERT_ERR(iotdata_encode_begin(&enc, pkt, sizeof(pkt), 15, 1, 1), IOTDATA_ERR_HDR_VARIANT_RESERVED, "reserved");
    ASSERT_ERR(iotdata_encode_begin(&enc, pkt, sizeof(pkt), 16, 1, 1), IOTDATA_ERR_HDR_VARIANT_HIGH, "too high");
    PASS();
}

static void test_station_high(void) {
    TEST("Station ID > 4095");
    ASSERT_ERR(iotdata_encode_begin(&enc, pkt, sizeof(pkt), 0, 4096, 1), IOTDATA_ERR_HDR_STATION_HIGH, "station high");
    PASS();
}

static void test_buffer_too_small(void) {
    TEST("Buffer too small for header");
    uint8_t tiny[4];
    ASSERT_ERR(iotdata_encode_begin(&enc, tiny, 4, 0, 1, 1), IOTDATA_ERR_BUF_TOO_SMALL, "too small");
    PASS();
}

/* =========================================================================
 * Section 5: Decoder error paths
 * =========================================================================*/

static void test_decode_null(void) {
    TEST("Decode null buffer/output");
    ASSERT_ERR(iotdata_decode(NULL, 10, &dec), IOTDATA_ERR_CTX_NULL, "null buf");
    uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    ASSERT_ERR(iotdata_decode(buf, 5, NULL), IOTDATA_ERR_CTX_NULL, "null dec");
    PASS();
}

static void test_decode_zero_length(void) {
    TEST("Decode zero length");
    uint8_t buf[] = { 0 };
    ASSERT_ERR(iotdata_decode(buf, 0, &dec), IOTDATA_ERR_DECODE_SHORT, "zero len");
    PASS();
}

static void test_decode_1_byte(void) {
    TEST("Decode 1 byte (too short for header)");
    uint8_t buf[] = { 0x00 };
    ASSERT_ERR(iotdata_decode(buf, 1, &dec), IOTDATA_ERR_DECODE_SHORT, "1 byte");
    PASS();
}

static void test_decode_4_bytes(void) {
    TEST("Decode 4 bytes (header only, no presence)");
    uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
    ASSERT_ERR(iotdata_decode(buf, 4, &dec), IOTDATA_ERR_DECODE_SHORT, "4 bytes");
    PASS();
}

static void test_decode_reserved_variant(void) {
    TEST("Decode reserved variant 15");
    uint8_t buf[] = { 0xF0, 0x00, 0x00, 0x00, 0x00 };
    ASSERT_ERR(iotdata_decode(buf, 5, &dec), IOTDATA_ERR_DECODE_VARIANT, "variant 15");
    PASS();
}

static void test_decode_truncated_field(void) {
    TEST("Decode truncated field data");

    /* Build a valid packet with battery, then truncate the field data */
    begin(0, 1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();

    /* Packet should be 6 bytes: header(4) + pres(1) + field data(1 byte with battery bits).
       Truncate to 5: header + presence, no field data */
    ASSERT_ERR(iotdata_decode(pkt, 5, &dec), IOTDATA_ERR_DECODE_TRUNCATED, "truncated");
    PASS();
}

static void test_decode_empty_packet(void) {
    TEST("Decode valid empty packet (no fields)");

    begin(0, 1, 1);
    finish();

    decode_pkt();
    ASSERT_EQ_U(dec.fields, 0, "no fields");
    ASSERT_EQ_U(dec.station, 1, "station");
    ASSERT_EQ_U(dec.sequence, 1, "seq");
    PASS();
}

static void test_peek_errors(void) {
    TEST("Peek error paths");

    uint8_t short_buf[] = { 0x00, 0x00 };
    ASSERT_ERR(iotdata_peek(short_buf, 2, NULL, NULL, NULL), IOTDATA_ERR_DECODE_SHORT, "peek short");

    uint8_t reserved[] = { 0xF0, 0x00, 0x00, 0x00, 0x00 };
    uint8_t v = 0;
    ASSERT_ERR(iotdata_peek(reserved, 5, &v, NULL, NULL), IOTDATA_ERR_DECODE_VARIANT, "peek reserved");
    PASS();
}

/* =========================================================================
 * Section 6: Image edge cases
 * =========================================================================*/

static void test_image_zero_data(void) {
    TEST("Image with minimum data (1 byte)");

    begin(0, 1, 300);
    uint8_t px = 0x42;
    ASSERT_OK(iotdata_encode_image(&enc, 0, 0, 0, 0, &px, 1), "encode 1 byte");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_data_len, 1, "len 1");
    ASSERT_EQ(dec.image_data[0], 0x42, "data");
    PASS();
}

static void test_image_max_data(void) {
    TEST("Image with maximum data (254 bytes)");

    /* 254-byte image + header/presence/control/length exceeds 256, use larger buffer */
    uint8_t big_pkt[512];
    size_t big_len;
    iotdata_encoder_t big_enc;
    memset(&big_enc, 0, sizeof(big_enc));
    assert(iotdata_encode_begin(&big_enc, big_pkt, sizeof(big_pkt), 0, 1, 301) == IOTDATA_OK);

    uint8_t big[254];
    for (int i = 0; i < 254; i++)
        big[i] = (uint8_t)(i & 0xFF);
    ASSERT_OK(iotdata_encode_image(&big_enc, 0, 0, 0, 0, big, 254), "encode 254");
    assert(iotdata_encode_end(&big_enc, &big_len) == IOTDATA_OK);

    iotdata_decoded_t big_dec;
    assert(iotdata_decode(big_pkt, big_len, &big_dec) == IOTDATA_OK);
    ASSERT_EQ(big_dec.image_data_len, 254, "len 254");
    ASSERT_EQ(big_dec.image_data[0], 0x00, "first");
    ASSERT_EQ(big_dec.image_data[253], 0xFD, "last");
    ASSERT_EQ(memcmp(big_dec.image_data, big, 254), 0, "full match");
    PASS();
}

static void test_image_all_formats(void) {
    TEST("Image all pixel formats round-trip");

    uint8_t px[] = { 0xAA, 0x55, 0xFF, 0x00 };

    /* Bilevel (1bpp) */
    begin(0, 1, 310);
    ASSERT_OK(iotdata_encode_image(&enc, IOTDATA_IMAGE_FMT_BILEVEL, 0, 0, 0, px, 4), "bilevel");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_pixel_format, IOTDATA_IMAGE_FMT_BILEVEL, "fmt bilevel");

    /* Grey4 (2bpp) */
    begin(0, 1, 311);
    ASSERT_OK(iotdata_encode_image(&enc, IOTDATA_IMAGE_FMT_GREY4, 0, 0, 0, px, 4), "grey4");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_pixel_format, IOTDATA_IMAGE_FMT_GREY4, "fmt grey4");

    /* Grey16 (4bpp) */
    begin(0, 1, 312);
    ASSERT_OK(iotdata_encode_image(&enc, IOTDATA_IMAGE_FMT_GREY16, 0, 0, 0, px, 4), "grey16");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.image_pixel_format, IOTDATA_IMAGE_FMT_GREY16, "fmt grey16");
    PASS();
}

/* =========================================================================
 * Section 7: TLV edge cases
 * =========================================================================*/

static void test_tlv_max_entries(void) {
    TEST("TLV fill to max then overflow");

    begin(0, 1, 400);
    uint8_t raw[] = { 0x01 };

    for (int i = 0; i < IOTDATA_TLV_MAX; i++)
        ASSERT_OK(iotdata_encode_tlv(&enc, 0x20, raw, 1), "fill");

    /* One more should fail */
    ASSERT_ERR(iotdata_encode_tlv(&enc, 0x20, raw, 1), IOTDATA_ERR_TLV_FULL, "overflow");
    PASS();
}

static void test_tlv_max_data_length(void) {
    TEST("TLV max data length (255 bytes)");

    /* header(4) + presence(3) + tlv_header(2) + data(255) = 264, exceeds pkt[256] */
    uint8_t big_pkt[512];
    size_t big_len;
    iotdata_encoder_t big_enc;
    assert(iotdata_encode_begin(&big_enc, big_pkt, sizeof(big_pkt), 0, 1, 401) == IOTDATA_OK);

    uint8_t data[255];
    memset(data, 0xAB, 255);
    ASSERT_OK(iotdata_encode_tlv(&big_enc, 0x20, data, 255), "encode 255");
    assert(iotdata_encode_end(&big_enc, &big_len) == IOTDATA_OK);

    iotdata_decoded_t big_dec;
    assert(iotdata_decode(big_pkt, big_len, &big_dec) == IOTDATA_OK);
    ASSERT_EQ(big_dec.tlv_count, 1, "count");
    ASSERT_EQ(big_dec.tlv[0].length, 255, "len");
    ASSERT_EQ(big_dec.tlv[0].raw[0], 0xAB, "first");
    ASSERT_EQ(big_dec.tlv[0].raw[254], 0xAB, "last");
    PASS();
}

static void test_tlv_empty_string(void) {
    TEST("TLV empty string");

    begin(0, 1, 402);
    ASSERT_OK(iotdata_encode_tlv_string(&enc, 0x20, ""), "encode empty");
    finish();
    decode_pkt();
    ASSERT_EQ(dec.tlv_count, 1, "count");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_STRING, "fmt");
    ASSERT_EQ(strcmp(dec.tlv[0].str, ""), 0, "empty str");
    PASS();
}

static void test_tlv_type_boundary(void) {
    TEST("TLV type boundary (0, 63, 64)");

    begin(0, 1, 403);
    uint8_t raw[] = { 0x01 };
    ASSERT_OK(iotdata_encode_tlv(&enc, 0, raw, 1), "type 0");
    ASSERT_OK(iotdata_encode_tlv(&enc, 63, raw, 1), "type 63");
    ASSERT_ERR(iotdata_encode_tlv(&enc, 64, raw, 1), IOTDATA_ERR_TLV_TYPE_HIGH, "type 64");
    PASS();
}

static void test_tlv_kv_mismatch(void) {
    TEST("TLV KV mismatch (odd count)");

    begin(0, 1, 404);
    const char *kv[] = { "key1", "val1", "orphan" };
    char buf[256];
    ASSERT_ERR(iotdata_encode_tlv_type_version(&enc, kv, 3, false, buf, sizeof(buf)), IOTDATA_ERR_TLV_KV_MISMATCH, "odd count");

    const char *kv0[] = { NULL };
    ASSERT_ERR(iotdata_encode_tlv_type_version(&enc, kv0, 0, false, buf, sizeof(buf)), IOTDATA_ERR_TLV_DATA_NULL, "zero count");
    PASS();
}

static void test_tlv_string_invalid_chars(void) {
    TEST("TLV string with invalid 6-bit characters");

    begin(0, 1, 405);
    ASSERT_ERR(iotdata_encode_tlv_string(&enc, 0x20, "hello{world"), IOTDATA_ERR_TLV_STR_CHAR_INVALID, "brace");
    ASSERT_ERR(iotdata_encode_tlv_string(&enc, 0x20,
                                         "\x80"
                                         "test"),
               IOTDATA_ERR_TLV_STR_CHAR_INVALID, "high byte");
    PASS();
}

/* =========================================================================
 * Section 8: Encode buffer overflow scenarios
 * =========================================================================*/

static void test_buffer_overflow_single_field(void) {
    TEST("Buffer overflow: one field, tiny buffer");

    uint8_t small[5]; /* header(4) + pres(1), no room for field bits */
    iotdata_encoder_t small_enc;
    ASSERT_OK(iotdata_encode_begin(&small_enc, small, sizeof(small), 0, 1, 1), "begin");
    ASSERT_OK(iotdata_encode_battery(&small_enc, 50, false), "bat");
    size_t out;
    ASSERT_ERR(iotdata_encode_end(&small_enc, &out), IOTDATA_ERR_BUF_TOO_SMALL, "overflow");
    PASS();
}

static void test_buffer_overflow_many_fields(void) {
    TEST("Buffer overflow: many fields, small buffer");

    uint8_t small[10];
    iotdata_encoder_t small_enc;
    ASSERT_OK(iotdata_encode_begin(&small_enc, small, sizeof(small), 0, 1, 1), "begin");
    ASSERT_OK(iotdata_encode_battery(&small_enc, 50, false), "bat");
    ASSERT_OK(iotdata_encode_link(&small_enc, -80, 0.0f), "link");
    ASSERT_OK(iotdata_encode_environment(&small_enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&small_enc, 5.0f, 180, 8.0f), "wind");
    size_t out;
    ASSERT_ERR(iotdata_encode_end(&small_enc, &out), IOTDATA_ERR_BUF_TOO_SMALL, "overflow many");
    PASS();
}

/* =========================================================================
 * Section 9: JSON error paths
 * =========================================================================*/

static void test_json_parse_error(void) {
    TEST("JSON parse invalid input");

    uint8_t buf[256];
    size_t len;
    iotdata_encode_from_json_scratch_t scratch;
    ASSERT_ERR(iotdata_encode_from_json("{invalid json", buf, sizeof(buf), &len, &scratch), IOTDATA_ERR_JSON_PARSE, "parse");
    ASSERT_ERR(iotdata_encode_from_json("", buf, sizeof(buf), &len, &scratch), IOTDATA_ERR_JSON_PARSE, "empty");
    ASSERT_ERR(iotdata_encode_from_json(NULL, buf, sizeof(buf), &len, &scratch), IOTDATA_ERR_JSON_PARSE, "null");
    PASS();
}

static void test_json_missing_fields(void) {
    TEST("JSON missing required fields");

    uint8_t buf[256];
    size_t len;
    iotdata_encode_from_json_scratch_t scratch;

    /* Valid JSON but missing variant/station/sequence */
    ASSERT_ERR(iotdata_encode_from_json("{\"foo\":1}", buf, sizeof(buf), &len, &scratch), IOTDATA_ERR_JSON_MISSING_FIELD, "missing header");
    PASS();
}

/* =========================================================================
 * Section 10: Dump/print edge cases
 * =========================================================================*/

static void test_dump_short_buffer(void) {
    TEST("Dump with short input buffer");

    uint8_t short_buf[] = { 0x00, 0x00, 0x00 };
    char out[1024];
    iotdata_dump_t dump;
    ASSERT_ERR(iotdata_dump_to_string(&dump, short_buf, 3, out, sizeof(out), false), IOTDATA_ERR_DECODE_SHORT, "dump short");
    PASS();
}

static void test_print_short_buffer(void) {
    TEST("Print with short input buffer");

    uint8_t short_buf[] = { 0x00, 0x00, 0x00 };
    char out[1024];
    iotdata_print_scratch_t scratch;
    ASSERT_ERR(iotdata_print_to_string(short_buf, 3, out, sizeof(out), &scratch), IOTDATA_ERR_DECODE_SHORT, "print short");
    PASS();
}

static void test_dump_empty_packet(void) {
    TEST("Dump empty packet (no fields)");

    begin(0, 1, 1);
    finish();

    char out[4096];
    iotdata_dump_t dump;
    ASSERT_OK(iotdata_dump_to_string(&dump, pkt, pkt_len, out, sizeof(out), true), "dump empty");
    ASSERT_TRUE(strstr(out, "variant") != NULL, "has variant");
    PASS();
}

/* =========================================================================
 * Section 11: Multi-field negative value combinations
 * =========================================================================*/

static void test_all_negative_fields(void) {
    TEST("All-negative field values in one packet");

    begin(1, 1, 500);
    ASSERT_OK(iotdata_encode_battery(&enc, 0, false), "bat 0");
    ASSERT_OK(iotdata_encode_temperature(&enc, -40.0f), "temp min");
    ASSERT_OK(iotdata_encode_pressure(&enc, 850), "pres min");
    ASSERT_OK(iotdata_encode_humidity(&enc, 0), "hum 0");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 0.0f), "wspd 0");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 0), "wdir 0");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 0.0f), "wgust 0");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 0), "rrate 0");
    ASSERT_OK(iotdata_encode_rain_size(&enc, 0), "rsize 0");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 0), "cpm 0");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 0.0f), "dose 0");
    ASSERT_OK(iotdata_encode_depth(&enc, 0), "depth 0");
    ASSERT_OK(iotdata_encode_position(&enc, -90.0, -180.0), "pos min");
    ASSERT_OK(iotdata_encode_datetime(&enc, 0), "dt 0");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x00), "flags 0");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.battery_level, 0, "bat");
    ASSERT_NEAR(dec.temperature, -40.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 850, "pres");
    ASSERT_EQ(dec.humidity, 0, "hum");
    ASSERT_NEAR(dec.position_lat, -90.0, 0.01, "lat");
    ASSERT_NEAR(dec.position_lon, -180.0, 0.01, "lon");
    PASS();
}

static void test_all_max_fields(void) {
    TEST("All-maximum field values in one packet");

    begin(1, 4095, 65535);
    ASSERT_OK(iotdata_encode_battery(&enc, 100, true), "bat 100");
    ASSERT_OK(iotdata_encode_temperature(&enc, 80.0f), "temp max");
    ASSERT_OK(iotdata_encode_pressure(&enc, 1105), "pres max");
    ASSERT_OK(iotdata_encode_humidity(&enc, 100), "hum 100");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 63.5f), "wspd max");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 359), "wdir max");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 63.5f), "wgust max");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 255), "rrate max");
    ASSERT_OK(iotdata_encode_rain_size(&enc, 15), "rsize max");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 16383), "cpm max");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 163.83f), "dose max");
    ASSERT_OK(iotdata_encode_depth(&enc, 1023), "depth max");
    ASSERT_OK(iotdata_encode_position(&enc, 90.0, 179.999), "pos max");
    ASSERT_OK(iotdata_encode_datetime(&enc, 83886075), "dt max");
    ASSERT_OK(iotdata_encode_flags(&enc, 0xFF), "flags FF");
    finish();
    decode_pkt();

    ASSERT_EQ(dec.battery_level, 100, "bat");
    ASSERT_EQ(dec.battery_charging, true, "charging");
    ASSERT_NEAR(dec.temperature, 80.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1105, "pres");
    ASSERT_EQ(dec.humidity, 100, "hum");
    ASSERT_NEAR(dec.wind_speed, 63.5, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 359, 2.0, "wdir");
    ASSERT_EQ_U(dec.radiation_cpm, 16383, "cpm");
    ASSERT_EQ_U(dec.depth, 1023, "depth");
    ASSERT_EQ(dec.flags, 0xFF, "flags");
    PASS();
}

/* =========================================================================
 * Section 12: JSON round-trip with negative/boundary values
 * =========================================================================*/

static void test_json_negative_round_trip(void) {
    TEST("JSON round-trip with negative values");

    begin(1, 1, 600);
    ASSERT_OK(iotdata_encode_battery(&enc, 0, false), "bat");
    ASSERT_OK(iotdata_encode_temperature(&enc, -25.5f), "temp");
    ASSERT_OK(iotdata_encode_pressure(&enc, 950), "pres");
    ASSERT_OK(iotdata_encode_humidity(&enc, 95), "hum");
    ASSERT_OK(iotdata_encode_wind_speed(&enc, 0.5f), "wspd");
    ASSERT_OK(iotdata_encode_wind_direction(&enc, 0), "wdir");
    ASSERT_OK(iotdata_encode_wind_gust(&enc, 0.5f), "wgust");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 0), "rrate");
    ASSERT_OK(iotdata_encode_rain_size(&enc, 0), "rsize");
    ASSERT_OK(iotdata_encode_radiation_cpm(&enc, 0), "cpm");
    ASSERT_OK(iotdata_encode_radiation_dose(&enc, 0.0f), "dose");
    ASSERT_OK(iotdata_encode_depth(&enc, 0), "depth");
    ASSERT_OK(iotdata_encode_position(&enc, -45.0, -90.0), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 0), "dt");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x00), "flags");
    finish();

    char *json = NULL;
    iotdata_decode_to_json_scratch_t dec_scratch;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json, &dec_scratch), "to_json");

    uint8_t pkt2[256] = { 0 };
    size_t len2 = 0;
    iotdata_encode_from_json_scratch_t enc_scratch;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2, &enc_scratch), "from_json");
    free(json);

    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

/* =========================================================================
 * Main
 * =========================================================================*/

int main(void) {
    printf("\n=== iotdata — failure/boundary/negative test suite ===\n\n");

    printf("--- Section 1: Negative value round-trips ---\n");
    test_negative_temperature();
    test_negative_rssi();
    test_negative_snr();
    test_negative_position();

    printf("\n--- Section 2: Boundary value round-trips ---\n");
    test_battery_boundaries();
    test_temperature_boundaries();
    test_pressure_boundaries();
    test_wind_speed_boundaries();
    test_wind_direction_boundaries();
    test_position_boundaries();
    test_datetime_boundaries();
    test_radiation_boundaries();
    test_clouds_boundaries();
    test_solar_boundaries();
    test_header_boundaries();
    test_humidity_boundaries();
    test_flags_boundaries();

    printf("\n--- Section 3: Field value errors (out of range) ---\n");
    test_battery_errors();
    test_temperature_errors();
    test_pressure_errors();
    test_humidity_errors();
    test_wind_errors();
    test_rssi_errors();
    test_snr_errors();
    test_solar_errors();
    test_clouds_errors();
    test_depth_errors();
    test_position_errors();
    test_radiation_errors();
    test_aq_index_errors();
    test_rain_errors();
    test_datetime_errors();

    printf("\n--- Section 4: Encoder state errors ---\n");
    test_null_encoder();
    test_null_buffer();
    test_encode_before_begin();
    test_double_begin();
    test_encode_after_end();
    test_duplicate_field();
    test_variant_errors();
    test_station_high();
    test_buffer_too_small();

    printf("\n--- Section 5: Decoder error paths ---\n");
    test_decode_null();
    test_decode_zero_length();
    test_decode_1_byte();
    test_decode_4_bytes();
    test_decode_reserved_variant();
    test_decode_truncated_field();
    test_decode_empty_packet();
    test_peek_errors();

    printf("\n--- Section 6: Image edge cases ---\n");
    test_image_zero_data();
    test_image_max_data();
    test_image_all_formats();

    printf("\n--- Section 7: TLV edge cases ---\n");
    test_tlv_max_entries();
    test_tlv_max_data_length();
    test_tlv_empty_string();
    test_tlv_type_boundary();
    test_tlv_kv_mismatch();
    test_tlv_string_invalid_chars();

    printf("\n--- Section 8: Buffer overflow ---\n");
    test_buffer_overflow_single_field();
    test_buffer_overflow_many_fields();

    printf("\n--- Section 9: JSON error paths ---\n");
    test_json_parse_error();
    test_json_missing_fields();

    printf("\n--- Section 10: Dump/print edge cases ---\n");
    test_dump_short_buffer();
    test_print_short_buffer();
    test_dump_empty_packet();

    printf("\n--- Section 11: Multi-field negative/boundary combos ---\n");
    test_all_negative_fields();
    test_all_max_fields();

    printf("\n--- Section 12: JSON round-trip with negatives ---\n");
    test_json_negative_round_trip();

    printf("\n--- Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ---\n\n");

    return tests_failed > 0 ? 1 : 0;
}
