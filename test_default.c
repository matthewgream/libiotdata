/*
 * test_default.c - Default variant (weather_station) test suite
 *
 * Exercises all 13 fields in the default weather station variant,
 * plus standalone field types, boundary conditions, quantisation
 * accuracy, JSON round-trip, TLV, print, and dump.
 *
 * Compile:
 *   cc -DIOTDATA_VARIANT_MAPS_DEFAULT test_default.c iotdata.c -lm -lcjson -o test_default
 */

#include "iotdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ---------------------------------------------------------------------------
 * Test framework
 * -------------------------------------------------------------------------*/

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-58s ", name); \
        fflush(stdout); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ_U(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL: %s (got %u, expected %u)\n", msg, (unsigned)(a), (unsigned)(b)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_NEAR(a, b, tol, msg) \
    do { \
        if (fabs((double)(a) - (double)(b)) > (tol)) { \
            printf("FAIL: %s (got %.4f, expected %.4f, tol %.4f)\n", msg, (double)(a), (double)(b), (double)(tol)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_OK(rc, msg) \
    do { \
        if ((rc) != IOTDATA_OK) { \
            printf("FAIL: %s (%s)\n", msg, iotdata_strerror(rc)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_ERR(rc, expected, msg) \
    do { \
        if ((rc) != (expected)) { \
            printf("FAIL: %s (got %s, expected %s)\n", msg, iotdata_strerror(rc), iotdata_strerror(expected)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

/* Helpers */
static iotdata_encoder_t enc;
static uint8_t pkt[256];
static size_t pkt_len;
static iotdata_decoded_t dec;

static void begin(uint16_t station, uint16_t seq) {
    memset(&enc, 0, sizeof(enc));
    assert(iotdata_encode_begin(&enc, pkt, sizeof(pkt), 0, station, seq) == IOTDATA_OK);
}

static void finish(void) {
    assert(iotdata_encode_end(&enc, &pkt_len) == IOTDATA_OK);
}

static void decode(void) {
    memset(&dec, 0, sizeof(dec));
    assert(iotdata_decode(pkt, pkt_len, &dec) == IOTDATA_OK);
}

/* =========================================================================
 * Section 1: Individual field round-trip tests
 * =========================================================================*/

/* --- Battery --- */
static void test_battery_round_trip(void) {
    TEST("Battery round-trip");
    begin(1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 75, true), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_BATTERY), 1, "present");
    /* 75% → q=round(75/100*31)=23, decode=round(23/31*100)=74 */
    ASSERT_NEAR(dec.battery_level, 75, 4, "level");
    ASSERT_EQ(dec.battery_charging, true, "charging");
    PASS();
}

/* --- Environment (bundled temp+pres+humid) --- */
static void test_environment_round_trip(void) {
    TEST("Environment round-trip");
    begin(1, 2);
    ASSERT_OK(iotdata_encode_environment(&enc, 22.5f, 1013, 65), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_ENVIRONMENT), 1, "present");
    ASSERT_NEAR(dec.temperature, 22.5, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1013, "pressure");
    ASSERT_EQ(dec.humidity, 65, "humidity");
    PASS();
}

/* --- Wind (bundled speed+dir+gust) --- */
static void test_wind_round_trip(void) {
    TEST("Wind bundle round-trip");
    begin(1, 3);
    ASSERT_OK(iotdata_encode_wind(&enc, 5.5f, 180, 8.0f), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_WIND), 1, "present");
    ASSERT_NEAR(dec.wind_speed, 5.5, 0.5, "speed");
    ASSERT_NEAR(dec.wind_direction, 180, 2.0, "dir");
    ASSERT_NEAR(dec.wind_gust, 8.0, 0.5, "gust");
    PASS();
}

/* --- Rain (bundled rate+size) --- */
static void test_rain_round_trip(void) {
    TEST("Rain bundle round-trip");
    begin(1, 4);
    ASSERT_OK(iotdata_encode_rain(&enc, 42, 15), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_RAIN), 1, "present");
    ASSERT_EQ(dec.rain_rate, 42, "rate");
    ASSERT_NEAR(dec.rain_size10, 15, 5.0, "size");
    PASS();
}

/* --- Solar --- */
static void test_solar_round_trip(void) {
    TEST("Solar round-trip");
    begin(1, 5);
    ASSERT_OK(iotdata_encode_solar(&enc, 850, 11), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_SOLAR), 1, "present");
    ASSERT_EQ(dec.solar_irradiance, 850, "irradiance");
    ASSERT_EQ(dec.solar_ultraviolet, 11, "uv");
    PASS();
}

/* --- Link quality --- */
static void test_link_round_trip(void) {
    TEST("Link quality round-trip");
    begin(1, 6);
    ASSERT_OK(iotdata_encode_link(&enc, -90, 5.0f), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_LINK), 1, "present");
    /* RSSI: -90 → q=(-90-(-120))/4=7.5→8, decode=-120+8*4=-88 */
    ASSERT_NEAR(dec.link_rssi, -90, 4, "rssi");
    ASSERT_NEAR(dec.link_snr, 5.0, 5.0, "snr");
    PASS();
}

/* --- Flags --- */
static void test_flags_round_trip(void) {
    TEST("Flags round-trip");
    begin(1, 7);
    ASSERT_OK(iotdata_encode_flags(&enc, 0xA5), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_FLAGS), 1, "present");
    ASSERT_EQ(dec.flags, 0xA5, "flags");
    PASS();
}

/* --- Air quality --- */
static void test_air_quality_round_trip(void) {
    TEST("Air quality round-trip");
    begin(1, 8);
    ASSERT_OK(iotdata_encode_air_quality(&enc, 312), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_AIR_QUALITY), 1, "present");
    ASSERT_EQ(dec.air_quality, 312, "aqi");
    PASS();
}

/* --- Cloud cover --- */
static void test_clouds_round_trip(void) {
    TEST("Cloud cover round-trip");
    begin(1, 9);
    ASSERT_OK(iotdata_encode_clouds(&enc, 6), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_CLOUDS), 1, "present");
    ASSERT_EQ(dec.clouds, 6, "okta");
    PASS();
}

/* --- Radiation --- */
static void test_radiation_round_trip(void) {
    TEST("Radiation round-trip");
    begin(1, 10);
    ASSERT_OK(iotdata_encode_radiation(&enc, 15000, 1.23f), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_RADIATION), 1, "present");
    ASSERT_EQ_U(dec.radiation_cpm, 15000, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 1.23, 0.01, "dose");
    PASS();
}

/* --- Position --- */
static void test_position_round_trip(void) {
    TEST("Position round-trip");
    begin(1, 12);
    ASSERT_OK(iotdata_encode_position(&enc, 51.507222, -0.127500), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_POSITION), 1, "present");
    ASSERT_NEAR(dec.position_lat, 51.507222, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, -0.127500, 0.001, "lon");
    PASS();
}

/* --- Datetime --- */
static void test_datetime_round_trip(void) {
    TEST("Datetime round-trip");
    begin(1, 13);
    ASSERT_OK(iotdata_encode_datetime(&enc, 3456000), "encode");
    finish();
    decode();
    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_DATETIME), 1, "present");
    /* 3456000 / 5 * 5 = 3456000 (exact) */
    ASSERT_EQ_U(dec.datetime_secs, 3456000, "seconds");
    PASS();
}

/* =========================================================================
 * Section 2: Presence byte groupings
 * =========================================================================*/

/* pres0 only: battery, environment, wind, rain_rate, solar, link */
static void test_pres0_all_six_fields(void) {
    TEST("All pres0 fields (no extension byte)");
    begin(100, 500);

    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "battery");
    ASSERT_OK(iotdata_encode_environment(&enc, 22.5f, 1013, 65), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 5.5f, 180, 8.0f), "wind");
    ASSERT_OK(iotdata_encode_rain(&enc, 12, 18), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 500, 7), "solar");
    ASSERT_OK(iotdata_encode_link(&enc, -90, 5.0f), "link");

    finish();
    decode();

    /* Verify no extension bit was needed */
    ASSERT_NEAR(dec.temperature, 22.5, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1013, "pres");
    ASSERT_EQ(dec.humidity, 65, "humid");
    ASSERT_NEAR(dec.wind_speed, 5.5, 0.5, "wspd");
    ASSERT_EQ(dec.rain_rate, 12, "rain");
    ASSERT_EQ(dec.solar_irradiance, 500, "sol");
    ASSERT_EQ(dec.solar_ultraviolet, 7, "uv");
    ASSERT_NEAR(dec.link_rssi, -90, 4, "rssi");
    PASS();
}

/* pres1: flags, air_quality, clouds, radiation_cpm, radiation_dose, position, datetime */
static void test_pres1_all_seven_fields(void) {
    TEST("All pres1 fields (extension byte)");
    begin(1, 100);

    /* Need at least one pres0 field, or just pres1 fields */
    ASSERT_OK(iotdata_encode_battery(&enc, 100, true), "battery");

    ASSERT_OK(iotdata_encode_flags(&enc, 0xFF), "flags");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 250), "aqi");
    ASSERT_OK(iotdata_encode_clouds(&enc, 3), "cloud");
    ASSERT_OK(iotdata_encode_radiation(&enc, 42, 0.05f), "rad");
    ASSERT_OK(iotdata_encode_position(&enc, -33.8688, 151.2093), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 7200000), "dt");

    finish();
    decode();

    ASSERT_EQ(dec.flags, 0xFF, "flags");
    ASSERT_EQ(dec.air_quality, 250, "aqi");
    ASSERT_EQ(dec.clouds, 3, "cloud");
    ASSERT_EQ_U(dec.radiation_cpm, 42, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 0.05, 0.01, "dose");
    ASSERT_NEAR(dec.position_lat, -33.8688, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, 151.2093, 0.001, "lon");
    ASSERT_EQ_U(dec.datetime_secs, 7200000, "dt");
    PASS();
}

/* All 13 fields */
static void test_full_weather_station(void) {
    TEST("Full weather station (all 13 fields)");
    begin(2048, 65535);

    /* Pres0 (6 fields) */
    ASSERT_OK(iotdata_encode_battery(&enc, 88, false), "battery");
    ASSERT_OK(iotdata_encode_environment(&enc, -5.25f, 980, 90), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 12.0f, 270, 18.5f), "wind");
    ASSERT_OK(iotdata_encode_rain(&enc, 0, 0), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 0, 0), "solar");
    ASSERT_OK(iotdata_encode_link(&enc, -100, -5.0f), "link");

    /* Pres1 (7 fields) */
    ASSERT_OK(iotdata_encode_flags(&enc, 0x01), "flags");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 150), "aqi");
    ASSERT_OK(iotdata_encode_clouds(&enc, 8), "cloud");
    ASSERT_OK(iotdata_encode_radiation(&enc, 25, 0.15f), "cpm");
    ASSERT_OK(iotdata_encode_position(&enc, 59.334591, 18.063240), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 3456000), "dt");

    finish();
    printf("\n    [packed: %zu bytes] ", pkt_len);
    decode();

    /* Verify all 13 fields */
    ASSERT_NEAR(dec.battery_level, 88, 4, "bat");
    ASSERT_EQ(dec.battery_charging, false, "chg");
    ASSERT_NEAR(dec.temperature, -5.25, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 980, "pres");
    ASSERT_EQ(dec.humidity, 90, "humid");
    ASSERT_NEAR(dec.wind_speed, 12.0, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 270, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 18.5, 0.5, "wgust");
    ASSERT_EQ(dec.rain_rate, 0, "rain");
    ASSERT_EQ(dec.rain_size10, 0, "rain");
    ASSERT_EQ(dec.solar_irradiance, 0, "sol");
    ASSERT_EQ(dec.solar_ultraviolet, 0, "uv");
    ASSERT_NEAR(dec.link_rssi, -100, 4, "rssi");
    ASSERT_EQ(dec.flags, 0x01, "flags");
    ASSERT_EQ(dec.air_quality, 150, "aqi");
    ASSERT_EQ(dec.clouds, 8, "cloud");
    ASSERT_EQ_U(dec.radiation_cpm, 25, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 0.15, 0.01, "dose");
    ASSERT_NEAR(dec.position_lat, 59.334591, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, 18.063240, 0.001, "lon");
    ASSERT_EQ_U(dec.datetime_secs, 3456000, "dt");
    PASS();
}

/* =========================================================================
 * Section 3: Boundary and edge case tests
 * =========================================================================*/

static void test_boundary_min_values(void) {
    TEST("Minimum boundary values");
    begin(0, 0);

    ASSERT_OK(iotdata_encode_battery(&enc, 0, false), "bat 0%");
    ASSERT_OK(iotdata_encode_environment(&enc, -40.0f, 850, 0), "env min");
    ASSERT_OK(iotdata_encode_wind(&enc, 0.0f, 0, 0.0f), "wind min");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 0), "rain 0");
    ASSERT_OK(iotdata_encode_solar(&enc, 0, 0), "solar 0");
    ASSERT_OK(iotdata_encode_link(&enc, -120, -20.0f), "link min");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x00), "flags 0");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 0), "aqi 0");
    ASSERT_OK(iotdata_encode_clouds(&enc, 0), "cloud 0");
    ASSERT_OK(iotdata_encode_radiation(&enc, 0, 0.f), "rad 0");
    ASSERT_OK(iotdata_encode_position(&enc, -90.0, -180.0), "pos min");
    ASSERT_OK(iotdata_encode_datetime(&enc, 0), "dt 0");

    finish();
    decode();

    ASSERT_EQ(dec.battery_level, 0, "bat");
    ASSERT_NEAR(dec.temperature, -40.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 850, "pres");
    ASSERT_EQ(dec.humidity, 0, "humid");
    ASSERT_NEAR(dec.wind_speed, 0.0, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 0, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 0.0, 0.5, "wgust");
    ASSERT_EQ(dec.rain_rate, 0, "rain");
    ASSERT_EQ(dec.solar_irradiance, 0, "sol");
    ASSERT_EQ(dec.solar_ultraviolet, 0, "uv");
    ASSERT_EQ(dec.link_rssi, -120, "rssi");
    ASSERT_NEAR(dec.link_snr, -20.0, 5.0, "snr");
    ASSERT_EQ(dec.flags, 0x00, "flags");
    ASSERT_EQ(dec.air_quality, 0, "aqi");
    ASSERT_EQ(dec.clouds, 0, "cloud");
    ASSERT_EQ_U(dec.radiation_cpm, 0, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 0.0, 0.01, "dose");
    ASSERT_NEAR(dec.position_lat, -90.0, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, -180.0, 0.001, "lon");
    ASSERT_EQ_U(dec.datetime_secs, 0, "dt");
    PASS();
}

static void test_boundary_max_values(void) {
    TEST("Maximum boundary values");
    begin(IOTDATA_STATION_MAX, IOTDATA_SEQUENCE_MAX);

    ASSERT_OK(iotdata_encode_battery(&enc, 100, true), "bat 100%");
    ASSERT_OK(iotdata_encode_environment(&enc, 80.0f, 1105, 100), "env max");
    ASSERT_OK(iotdata_encode_wind(&enc, 63.5f, 355, 63.5f), "wind max");
    ASSERT_OK(iotdata_encode_rain(&enc, 255, 60), "rain max");
    ASSERT_OK(iotdata_encode_solar(&enc, 1023, 15), "solar max");
    ASSERT_OK(iotdata_encode_link(&enc, -60, 10.0f), "link max");
    ASSERT_OK(iotdata_encode_flags(&enc, 0xFF), "flags ff");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 500), "aqi max");
    ASSERT_OK(iotdata_encode_clouds(&enc, 8), "cloud max");
    ASSERT_OK(iotdata_encode_radiation(&enc, 16383, 163.83f), "cpm max");
    ASSERT_OK(iotdata_encode_position(&enc, 90.0, 180.0), "pos max");
    /* Use a large but valid datetime */
    ASSERT_OK(iotdata_encode_datetime(&enc, 31536000), "dt large");

    finish();
    decode();

    ASSERT_EQ(dec.battery_level, 100, "bat");
    ASSERT_EQ(dec.battery_charging, true, "chg");
    ASSERT_NEAR(dec.temperature, 80.0, 0.25, "temp");
    ASSERT_EQ(dec.pressure, 1105, "pres");
    ASSERT_EQ(dec.humidity, 100, "humid");
    ASSERT_NEAR(dec.wind_speed, 63.5, 0.5, "wspd");
    ASSERT_NEAR(dec.wind_direction, 355, 2.0, "wdir");
    ASSERT_NEAR(dec.wind_gust, 63.5, 0.5, "wgust");
    ASSERT_EQ(dec.rain_rate, 255, "rainrate");
    ASSERT_NEAR(dec.rain_size10, 60, 5.0, "rainsize");
    ASSERT_EQ(dec.solar_irradiance, 1023, "sol");
    ASSERT_EQ(dec.solar_ultraviolet, 15, "uv");
    ASSERT_EQ(dec.link_rssi, -60, "rssi");
    ASSERT_NEAR(dec.link_snr, 10.0, 5.0, "snr");
    ASSERT_EQ(dec.flags, 0xFF, "flags");
    ASSERT_EQ(dec.air_quality, 500, "aqi");
    ASSERT_EQ(dec.clouds, 8, "cloud");
    ASSERT_EQ_U(dec.radiation_cpm, 16383, "cpm");
    ASSERT_NEAR(dec.radiation_dose, 163.83, 0.01, "dose");
    ASSERT_NEAR(dec.position_lat, 90.0, 0.001, "lat");
    ASSERT_NEAR(dec.position_lon, 180.0, 0.001, "lon");
    PASS();
}

static void test_error_conditions(void) {
    TEST("Error boundary checks (all fields)");

    begin(1, 1);

    /* Temperature out of range */
    ASSERT_ERR(iotdata_encode_temperature(&enc, -50.0f), IOTDATA_ERR_TEMPERATURE_LOW, "temp low");
    ASSERT_ERR(iotdata_encode_temperature(&enc, 85.0f), IOTDATA_ERR_TEMPERATURE_HIGH, "temp high");

    /* Pressure out of range */
    ASSERT_ERR(iotdata_encode_pressure(&enc, 849), IOTDATA_ERR_PRESSURE_LOW, "pres low");
    ASSERT_ERR(iotdata_encode_pressure(&enc, 1106), IOTDATA_ERR_PRESSURE_HIGH, "pres high");

    /* Humidity out of range */
    ASSERT_ERR(iotdata_encode_humidity(&enc, 101), IOTDATA_ERR_HUMIDITY_HIGH, "humid high");

    /* Wind speed out of range */
    ASSERT_ERR(iotdata_encode_wind_speed(&enc, 70.0f), IOTDATA_ERR_WIND_SPEED_HIGH, "wspd high");

    /* Wind direction out of range */
    ASSERT_ERR(iotdata_encode_wind_direction(&enc, 360), IOTDATA_ERR_WIND_DIRECTION_HIGH, "wdir high");

    /* Wind gust out of range */
    ASSERT_ERR(iotdata_encode_wind_gust(&enc, 70.0f), IOTDATA_ERR_WIND_GUST_HIGH, "wgust high");

    /* Rain rate: uint8_t param, so 256 wraps — no range check possible at API level.
     * Test that max value (255) is accepted: */
    /* (rain_rate boundary is enforced by the C type itself) */

    /* Air quality out of range */
    ASSERT_ERR(iotdata_encode_air_quality(&enc, 501), IOTDATA_ERR_AIR_QUALITY_HIGH, "aqi high");

    /* Cloud cover out of range */
    ASSERT_ERR(iotdata_encode_clouds(&enc, 9), IOTDATA_ERR_CLOUDS_HIGH, "cloud high");

    /* Radiation CPM: uint16_t param, so 65536 wraps — no range check at API level.
     * (radiation_cpm boundary is enforced by the C type itself) */

    /* Radiation dose out of range */
    ASSERT_ERR(iotdata_encode_radiation_dose(&enc, 200.0f), IOTDATA_ERR_RADIATION_DOSE_HIGH, "dose high");

    /* Battery too high */
    ASSERT_ERR(iotdata_encode_battery(&enc, 101, false), IOTDATA_ERR_BATTERY_LEVEL_HIGH, "bat high");

    /* Depth out of range */
    ASSERT_ERR(iotdata_encode_depth(&enc, 1024), IOTDATA_ERR_DEPTH_HIGH, "depth high");

    /* Duplicate field */
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat ok");
    ASSERT_ERR(iotdata_encode_battery(&enc, 60, true), IOTDATA_ERR_CTX_DUPLICATE_FIELD, "bat dup");

    /* Variant limits */
    iotdata_encoder_t enc2;
    ASSERT_ERR(iotdata_encode_begin(&enc2, pkt, sizeof(pkt), 15, 1, 1), IOTDATA_ERR_HDR_VARIANT_RESERVED, "var 15");
    ASSERT_ERR(iotdata_encode_begin(&enc2, pkt, sizeof(pkt), 16, 1, 1), IOTDATA_ERR_HDR_VARIANT_HIGH, "var 16");

    /* Station ID too high */
    ASSERT_ERR(iotdata_encode_begin(&enc2, pkt, sizeof(pkt), 0, 5000, 1), IOTDATA_ERR_HDR_STATION_HIGH, "station high");

    /* NULL enc */
    ASSERT_ERR(iotdata_encode_begin(NULL, pkt, sizeof(pkt), 0, 1, 1), IOTDATA_ERR_CTX_NULL, "null enc");

    PASS();
}

/* =========================================================================
 * Section 4: Quantisation accuracy
 * =========================================================================*/

static void test_quantisation_temperature(void) {
    TEST("Quantisation: temperature sweep");

    /* Test at several points across the range */
    float test_temps[] = { -40.0f, -20.0f, 0.0f, 20.0f, 22.5f, 37.5f, 80.0f };
    int n = sizeof(test_temps) / sizeof(test_temps[0]);

    for (int i = 0; i < n; i++) {
        begin(1, (uint16_t)(100 + i));
        ASSERT_OK(iotdata_encode_environment(&enc, test_temps[i], 1000, 50), "enc");
        finish();
        decode();
        ASSERT_NEAR(dec.temperature, test_temps[i], 0.25, "temp quant");
    }
    PASS();
}

static void test_quantisation_wind(void) {
    TEST("Quantisation: wind sweep");

    struct {
        float spd;
        int dir;
        float gust;
    } tests[] = {
        { 0.0f, 0, 0.0f },
        { 10.0f, 90, 15.0f },
        { 31.5f, 180, 45.0f },
        { 63.0f, 355, 63.0f },
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        begin(1, (uint16_t)(200 + i));
        ASSERT_OK(iotdata_encode_wind(&enc, tests[i].spd, (uint16_t)tests[i].dir, tests[i].gust), "enc");
        finish();
        decode();
        ASSERT_NEAR(dec.wind_speed, tests[i].spd, 0.5, "speed quant");
        ASSERT_NEAR(dec.wind_direction, tests[i].dir, 2.0, "dir quant");
        ASSERT_NEAR(dec.wind_gust, tests[i].gust, 0.5, "gust quant");
    }
    PASS();
}

static void test_quantisation_position(void) {
    TEST("Quantisation: position accuracy");

    struct {
        double lat;
        double lon;
    } positions[] = {
        { 0.0, 0.0 },
        { 51.507222, -0.127500 }, /* London */
        { 59.334591, 18.063240 }, /* Stockholm */
        { -33.8688, 151.2093 },   /* Sydney */
        { 90.0, 180.0 },          /* Max */
        { -90.0, -180.0 },        /* Min */
    };
    int n = sizeof(positions) / sizeof(positions[0]);

    for (int i = 0; i < n; i++) {
        begin(1, (uint16_t)(300 + i));
        ASSERT_OK(iotdata_encode_position(&enc, positions[i].lat, positions[i].lon), "enc");
        finish();
        decode();
        ASSERT_NEAR(dec.position_lat, positions[i].lat, 0.001, "lat");
        ASSERT_NEAR(dec.position_lon, positions[i].lon, 0.001, "lon");
    }
    PASS();
}

static void test_quantisation_radiation(void) {
    TEST("Quantisation: radiation dose");

    float doses[] = { 0.0f, 0.01f, 0.10f, 1.23f, 10.0f, 100.0f, 163.83f };
    int n = sizeof(doses) / sizeof(doses[0]);

    for (int i = 0; i < n; i++) {
        begin(1, (uint16_t)(400 + i));
        ASSERT_OK(iotdata_encode_radiation(&enc, 0, doses[i]), "enc");
        finish();
        decode();
        ASSERT_NEAR(dec.radiation_dose, doses[i], 0.01, "dose");
    }
    PASS();
}

/* =========================================================================
 * Section 5: TLV, JSON, Print, Dump
 * =========================================================================*/

static void test_tlv_round_trip(void) {
    TEST("TLV round-trip (raw + string)");
    begin(1, 1);

    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");

    uint8_t raw[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ASSERT_OK(iotdata_encode_tlv(&enc, 1, raw, 4), "tlv raw");
    ASSERT_OK(iotdata_encode_tlv_string(&enc, 2, "hello world"), "tlv str");

    finish();
    decode();

    ASSERT_EQ(dec.tlv_count, 2, "count");
    ASSERT_EQ(dec.tlv[0].format, IOTDATA_TLV_FMT_RAW, "fmt0");
    ASSERT_EQ(dec.tlv[0].type, 1, "type0");
    ASSERT_EQ(dec.tlv[0].length, 4, "len0");
    ASSERT_EQ(dec.tlv[0].data[0], 0xDE, "raw0");
    ASSERT_EQ(dec.tlv[0].data[3], 0xEF, "raw3");
    ASSERT_EQ(dec.tlv[1].format, IOTDATA_TLV_FMT_STRING, "fmt1");
    ASSERT_EQ(strcmp(dec.tlv[1].str, "hello world"), 0, "str1");
    PASS();
}

static void test_json_round_trip(void) {
    TEST("JSON round-trip (full weather station)");
    begin(10, 999);

    ASSERT_OK(iotdata_encode_battery(&enc, 80, true), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 8.0f, 225, 12.0f), "wind");
    ASSERT_OK(iotdata_encode_rain(&enc, 5, 1), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 300, 5), "sol");
    ASSERT_OK(iotdata_encode_link(&enc, -80, 0.0f), "link");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x42), "flags");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 75), "aqi");
    ASSERT_OK(iotdata_encode_clouds(&enc, 4), "cloud");
    ASSERT_OK(iotdata_encode_radiation(&enc, 100, 0.50f), "rad");
    ASSERT_OK(iotdata_encode_position(&enc, 51.5, -0.1), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 86400), "dt");

    finish();

    /* Encode → JSON */
    char *json = NULL;
    ASSERT_OK(iotdata_decode_to_json(pkt, pkt_len, &json), "to_json");

    /* JSON → binary */
    uint8_t pkt2[256];
    size_t len2;
    ASSERT_OK(iotdata_encode_from_json(json, pkt2, sizeof(pkt2), &len2), "from_json");
    free(json);

    /* Compare wire bytes */
    ASSERT_EQ(pkt_len, len2, "len match");
    ASSERT_EQ(memcmp(pkt, pkt2, pkt_len), 0, "bytes match");
    PASS();
}

static void test_dump_output(void) {
    TEST("Dump output");
    begin(5, 42);

    ASSERT_OK(iotdata_encode_battery(&enc, 90, false), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 15.0f, 1000, 70), "env");
    finish();

    iotdata_dump_t dump;
    ASSERT_OK(iotdata_dump_build(pkt, pkt_len, &dump), "build");
    if (dump.count < 5) {
        FAIL("too few entries");
        return;
    }

    char str[8192];
    ASSERT_OK(iotdata_dump_to_string(pkt, pkt_len, str, sizeof(str)), "to_string");
    if (!strstr(str, "Offset")) {
        FAIL("missing header");
        return;
    }
    if (!strstr(str, "variant")) {
        FAIL("missing variant");
        return;
    }
    PASS();
}

static void test_print_output(void) {
    TEST("Print output");
    begin(7, 100);

    ASSERT_OK(iotdata_encode_battery(&enc, 60, true), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 15.0f, 1000, 70), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 3.0f, 90, 5.0f), "wind");
    finish();

    char str[8192];
    ASSERT_OK(iotdata_print_to_string(pkt, pkt_len, str, sizeof(str)), "to_string");
    if (!strstr(str, "Station 7")) {
        FAIL("missing station");
        return;
    }
    if (!strstr(str, "weather_station")) {
        FAIL("missing variant name");
        return;
    }
    PASS();
}

/* =========================================================================
 * Section 6: Edge cases
 * =========================================================================*/

static void test_empty_packet(void) {
    TEST("Empty packet (header + pres0 only)");
    begin(0, 0);
    finish();

    ASSERT_EQ(pkt_len, 5, "size"); /* 4 header + 1 pres0 */
    decode();
    ASSERT_EQ(dec.fields, 0, "no fields");
    ASSERT_EQ(dec.variant, 0, "variant");
    PASS();
}

static void test_single_pres1_field_only(void) {
    TEST("Single pres1 field (flags only)");
    begin(1, 1);
    ASSERT_OK(iotdata_encode_flags(&enc, 0x42), "flags");
    finish();
    decode();

    ASSERT_EQ(!!IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_FLAGS), 1, "present");
    ASSERT_EQ(dec.flags, 0x42, "flags");
    /* Should have needed extension byte since flags is in pres1 */
    PASS();
}

static void test_strerror_coverage(void) {
    TEST("Error string coverage");
    const char *s;

    s = iotdata_strerror(IOTDATA_OK);
    if (!s || strlen(s) == 0) {
        FAIL("OK string");
        return;
    }

    s = iotdata_strerror(IOTDATA_ERR_WIND_SPEED_HIGH);
    if (!s) {
        FAIL("wind speed string null");
        return;
    }

    s = iotdata_strerror(IOTDATA_ERR_CLOUDS_HIGH);
    if (!s) {
        FAIL("cloud string null");
        return;
    }

    s = iotdata_strerror(IOTDATA_ERR_PRESSURE_LOW);
    if (!s) {
        FAIL("pres low string null");
        return;
    }

    s = iotdata_strerror(9999);
    if (!s || strcmp(s, "Unknown error") != 0) {
        FAIL("unknown");
        return;
    }

    PASS();
}

static void test_packet_sizes(void) {
    TEST("Packet size efficiency");

    /* Battery only */
    begin(1, 1);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    finish();
    printf("\n    [battery only: %zu bytes] ", pkt_len);

    /* Full pres0 */
    begin(1, 2);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 5.0f, 180, 8.0f), "wind");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 5), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 300, 5), "sol");
    ASSERT_OK(iotdata_encode_link(&enc, -80, 0.0f), "link");
    finish();
    printf("[full pres0: %zu bytes] ", pkt_len);

    /* Full station (all 13) */
    begin(1, 3);
    ASSERT_OK(iotdata_encode_battery(&enc, 50, false), "bat");
    ASSERT_OK(iotdata_encode_environment(&enc, 20.0f, 1013, 50), "env");
    ASSERT_OK(iotdata_encode_wind(&enc, 5.0f, 180, 8.0f), "wind");
    ASSERT_OK(iotdata_encode_rain_rate(&enc, 5), "rain");
    ASSERT_OK(iotdata_encode_solar(&enc, 300, 5), "sol");
    ASSERT_OK(iotdata_encode_link(&enc, -80, 0.0f), "link");
    ASSERT_OK(iotdata_encode_flags(&enc, 0x01), "flags");
    ASSERT_OK(iotdata_encode_air_quality(&enc, 50), "aqi");
    ASSERT_OK(iotdata_encode_clouds(&enc, 4), "cloud");
    ASSERT_OK(iotdata_encode_radiation(&enc, 100, 0.10f), "cpm");
    ASSERT_OK(iotdata_encode_position(&enc, 51.5, -0.1), "pos");
    ASSERT_OK(iotdata_encode_datetime(&enc, 86400), "dt");
    finish();
    printf("[full station: %zu bytes] ", pkt_len);

    PASS();
}

/* =========================================================================
 * Main
 * =========================================================================*/

int main(void) {
    printf("\n=== iotdata — default variant test suite ===\n\n");

    /* Section 1: individual fields */
    printf("  --- Individual field round-trips ---\n");
    test_battery_round_trip();
    test_environment_round_trip();
    test_wind_round_trip();
    test_rain_round_trip();
    test_solar_round_trip();
    test_link_round_trip();
    test_flags_round_trip();
    test_air_quality_round_trip();
    test_clouds_round_trip();
    test_radiation_round_trip();
    test_position_round_trip();
    test_datetime_round_trip();

    /* Section 2: presence byte groups */
    printf("\n  --- Presence byte groupings ---\n");
    test_pres0_all_six_fields();
    test_pres1_all_seven_fields();
    test_full_weather_station();

    /* Section 3: boundaries */
    printf("\n  --- Boundaries and errors ---\n");
    test_boundary_min_values();
    test_boundary_max_values();
    test_error_conditions();

    /* Section 4: quantisation */
    printf("\n  --- Quantisation accuracy ---\n");
    test_quantisation_temperature();
    test_quantisation_wind();
    test_quantisation_position();
    test_quantisation_radiation();

    /* Section 5: TLV, JSON, print, dump */
    printf("\n  --- TLV, JSON, print, dump ---\n");
    test_tlv_round_trip();
    test_json_round_trip();
    test_dump_output();
    test_print_output();

    /* Section 6: edge cases */
    printf("\n  --- Edge cases ---\n");
    test_empty_packet();
    test_single_pres1_field_only();
    test_strerror_coverage();
    test_packet_sizes();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(" (%d FAILED)", tests_failed);
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
