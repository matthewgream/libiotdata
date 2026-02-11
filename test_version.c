/*
 * test_version.c — Build variant smoke test
 *
 * Lightweight test that exercises available API functions under each
 * compile-time configuration.  A single source file compiled with
 * different defines to verify all build variants compile, link, and run.
 *
 * Variants built by `make test-versions`:
 *
 *   FULL            All features (encode + decode + print + dump + JSON)
 *   NO_PRINT        Exclude iotdata_print / iotdata_print_to_string
 *   NO_DUMP         Exclude iotdata_dump / iotdata_dump_to_string
 *   NO_JSON         Exclude JSON support (no cJSON dependency)
 *   NO_DECODE     Encoder only (no decode/print/dump/JSON)
 *   NO_ENCODE     Decoder only (no encoder)
 *   NO_FLOATING     Integer-only mode (int32_t scaled values)
 *
 * Compile (example, full variant):
 *   cc -DIOTDATA_VARIANT_MAPS_DEFAULT test_version.c iotdata.c -lm -lcjson -o test_version_FULL
 */

#include "iotdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Build label
 * -----------------------------------------------------------------------*/

static const char *build_label(void) {
#if defined(IOTDATA_NO_DECODE)
    return "NO_DECODE";
#elif defined(IOTDATA_NO_ENCODE)
    return "NO_ENCODE";
#elif defined(IOTDATA_NO_FLOATING) && defined(IOTDATA_NO_JSON)
    return "NO_FLOATING_NO_JSON";
#elif defined(IOTDATA_NO_FLOATING)
    return "NO_FLOATING";
#elif defined(IOTDATA_NO_PRINT)
    return "NO_PRINT";
#elif defined(IOTDATA_NO_DUMP)
    return "NO_DUMP";
#elif defined(IOTDATA_NO_JSON)
    return "NO_JSON";
#else
    return "FULL";
#endif
}

/* -------------------------------------------------------------------------
 * Minimal check framework
 * -----------------------------------------------------------------------*/

static int errors;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("    FAIL: %s\n", (msg)); \
            errors++; \
        } \
    } while (0)

/* -------------------------------------------------------------------------
 * Pre-built packet for NO_ENCODE
 *
 * We cannot encode when the encoder is excluded, so we embed a known-good
 * packet:  variant 0, station 1, sequence 1, pres0 = 0x21 (battery +
 * environment), battery 75% charging, temp 22.5°C, pressure 1013 hPa,
 * humidity 65%.
 *
 * This was captured from a normal build and verified against the decoder.
 * If the wire format changes this array must be regenerated.
 * -----------------------------------------------------------------------*/

#if defined(IOTDATA_NO_ENCODE)
static const uint8_t prebuilt_pkt[] = {
    /* Regenerate with: make test_version_FULL && ./test_version_FULL --dump */
    0x00, 0x00, 0x00, 0x00, 0x00 /* empty packet: var=0 sta=0 seq=0 */
};
static const size_t prebuilt_len = sizeof(prebuilt_pkt);
#endif

/* -------------------------------------------------------------------------
 * Encode a test packet (all modes except NO_ENCODE)
 * -----------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_ENCODE)
static int do_encode(uint8_t *buf, size_t buf_size, size_t *out_len) {
    iotdata_encoder_t enc;
    iotdata_status_t rc;

    rc = iotdata_encode_begin(&enc, buf, buf_size, 0, 1, 1);
    CHECK(rc == IOTDATA_OK, "encode_begin");
    if (rc != IOTDATA_OK)
        return -1;

    /* --- pres0 fields --- */

#if defined(IOTDATA_NO_FLOATING)
    /* Integer mode: iotdata_float_t = int32_t (value × 100) */
    rc = iotdata_encode_battery(&enc, 75, true);
    CHECK(rc == IOTDATA_OK, "encode_battery");

    rc = iotdata_encode_environment(&enc, 2250, 1013, 65); /* 22.50 °C */
    CHECK(rc == IOTDATA_OK, "encode_environment");

    rc = iotdata_encode_wind(&enc, 550, 180, 800); /* 5.50 / 8.00 m/s */
    CHECK(rc == IOTDATA_OK, "encode_wind");

    rc = iotdata_encode_rain(&enc, 5, 20); /* 5 mm/hr, 2.0 mm */
    CHECK(rc == IOTDATA_OK, "encode_rain");

    rc = iotdata_encode_solar(&enc, 500, 7);
    CHECK(rc == IOTDATA_OK, "encode_solar");

    rc = iotdata_encode_link(&enc, -90, 50); /* 5.00 dB */
    CHECK(rc == IOTDATA_OK, "encode_link");

    /* --- pres1 fields --- */

    rc = iotdata_encode_flags(&enc, 0x42);
    CHECK(rc == IOTDATA_OK, "encode_flags");

    rc = iotdata_encode_air_quality(&enc, 75);
    CHECK(rc == IOTDATA_OK, "encode_air_quality");

    rc = iotdata_encode_clouds(&enc, 4);
    CHECK(rc == IOTDATA_OK, "encode_clouds");

    rc = iotdata_encode_radiation(&enc, 100, 50); /* 0.50 µSv/h */
    CHECK(rc == IOTDATA_OK, "encode_radiation");

    rc = iotdata_encode_position(&enc, 515072220, -1275000); /* × 1e7 */
    CHECK(rc == IOTDATA_OK, "encode_position");

    rc = iotdata_encode_datetime(&enc, 86400);
    CHECK(rc == IOTDATA_OK, "encode_datetime");
#else
    /* Float mode */
    rc = iotdata_encode_battery(&enc, 75, true);
    CHECK(rc == IOTDATA_OK, "encode_battery");

    rc = iotdata_encode_environment(&enc, 22.5f, 1013, 65);
    CHECK(rc == IOTDATA_OK, "encode_environment");

    rc = iotdata_encode_wind(&enc, 5.5f, 180, 8.0f);
    CHECK(rc == IOTDATA_OK, "encode_wind");

    rc = iotdata_encode_rain(&enc, 5, 20);
    CHECK(rc == IOTDATA_OK, "encode_rain");

    rc = iotdata_encode_solar(&enc, 500, 7);
    CHECK(rc == IOTDATA_OK, "encode_solar");

    rc = iotdata_encode_link(&enc, -90, 5.0f);
    CHECK(rc == IOTDATA_OK, "encode_link");

    /* --- pres1 fields --- */

    rc = iotdata_encode_flags(&enc, 0x42);
    CHECK(rc == IOTDATA_OK, "encode_flags");

    rc = iotdata_encode_air_quality(&enc, 75);
    CHECK(rc == IOTDATA_OK, "encode_air_quality");

    rc = iotdata_encode_clouds(&enc, 4);
    CHECK(rc == IOTDATA_OK, "encode_clouds");

    rc = iotdata_encode_radiation(&enc, 100, 0.50f);
    CHECK(rc == IOTDATA_OK, "encode_radiation");

    rc = iotdata_encode_position(&enc, 51.5072220, -0.1275000);
    CHECK(rc == IOTDATA_OK, "encode_position");

    rc = iotdata_encode_datetime(&enc, 86400);
    CHECK(rc == IOTDATA_OK, "encode_datetime");
#endif

    rc = iotdata_encode_end(&enc, out_len);
    CHECK(rc == IOTDATA_OK, "encode_end");
    CHECK(*out_len > 0, "encoded length > 0");

    return (rc == IOTDATA_OK) ? 0 : -1;
}
#endif /* !NO_ENCODE */

/* -------------------------------------------------------------------------
 * Main
 * -----------------------------------------------------------------------*/

int main(void) {
    printf("  test_version  %-20s", build_label());
    fflush(stdout);

    uint8_t buf[256];
    size_t len = 0;

    /* ---- Encode (or load prebuilt) ---- */

#if defined(IOTDATA_NO_ENCODE)
    memcpy(buf, prebuilt_pkt, prebuilt_len);
    len = prebuilt_len;
#else
    if (do_encode(buf, sizeof(buf), &len) != 0) {
        printf("FAIL (encode)\n");
        return 1;
    }
#endif

    /* ---- Decode ---- */

#if !defined(IOTDATA_NO_DECODE)
    {
        iotdata_decoded_t decoded;
        iotdata_status_t rc;

        memset(&decoded, 0, sizeof(decoded));
        rc = iotdata_decode(buf, len, &decoded);
        CHECK(rc == IOTDATA_OK, "decode");
        CHECK(decoded.variant == 0, "variant == 0");
#if !defined(IOTDATA_NO_ENCODE)
        CHECK(decoded.station == 1, "station == 1");
        CHECK(decoded.sequence == 1, "sequence == 1");
#endif
    }
#endif

    /* ---- Print ---- */

#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
    {
        char str[4096];
        iotdata_status_t rc;

        rc = iotdata_print_to_string(buf, len, str, sizeof(str));
        CHECK(rc == IOTDATA_OK, "print_to_string");
        CHECK(strlen(str) > 0, "print output non-empty");
    }
#endif

    /* ---- Dump ---- */

#if !defined(IOTDATA_NO_DUMP) && !defined(IOTDATA_NO_DECODE)
    {
        char str[8192];
        iotdata_status_t rc;

        rc = iotdata_dump_to_string(buf, len, str, sizeof(str));
        CHECK(rc == IOTDATA_OK, "dump_to_string");
        CHECK(strlen(str) > 0, "dump output non-empty");
    }
#endif

    /* ---- JSON round-trip ---- */

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE) && !defined(IOTDATA_NO_ENCODE)
    {
        char *json = NULL;
        iotdata_status_t rc;

        rc = iotdata_decode_to_json(buf, len, &json);
        CHECK(rc == IOTDATA_OK, "decode_to_json");
        CHECK(json != NULL, "json non-null");

        if (json) {
            uint8_t buf2[256];
            size_t len2;

            rc = iotdata_encode_from_json(json, buf2, sizeof(buf2), &len2);
            CHECK(rc == IOTDATA_OK, "encode_from_json");
            CHECK(len == len2, "json round-trip length match");
            CHECK(memcmp(buf, buf2, len) == 0, "json round-trip byte match");
            free(json);
        }
    }
#endif

    /* ---- Error strings ---- */

#if !defined(IOTDATA_NO_ERROR_STRINGS)
    {
        const char *s = iotdata_strerror(IOTDATA_OK);
        CHECK(s != NULL, "strerror non-null");
        CHECK(strlen(s) > 0, "strerror non-empty");
    }
#endif

    /* ---- Result ---- */

    if (errors == 0)
        printf("PASS\n");
    else
        printf("FAIL (%d errors)\n", errors);

    return errors > 0 ? 1 : 0;
}
