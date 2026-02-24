/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * test_common.h - shared test framework macros, variables, and helpers
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

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
            printf("FAIL: %s (got %.6f, expected %.6f, tol %.6f)\n", msg, (double)(a), (double)(b), (double)(tol)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_OK(rc, msg) \
    do { \
        iotdata_status_t _rc = (rc); \
        if (_rc != IOTDATA_OK) { \
            printf("FAIL: %s (%s)\n", msg, iotdata_strerror(_rc)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_ERR(rc, expected, msg) \
    do { \
        iotdata_status_t _rc = (rc); \
        if (_rc != (expected)) { \
            printf("FAIL: %s (got %s, expected %s)\n", msg, iotdata_strerror(_rc), iotdata_strerror(expected)); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s\n", msg); \
            tests_failed++; \
            return; \
        } \
    } while (0)

/* ---------------------------------------------------------------------------
 * Shared helpers
 * -------------------------------------------------------------------------*/

static iotdata_encoder_t enc;
static uint8_t pkt[256];
static size_t pkt_len;
static iotdata_decoded_t dec;

static void begin(uint8_t variant, uint16_t station, uint16_t seq) {
    assert(iotdata_encode_begin(&enc, pkt, sizeof(pkt), variant, station, seq) == IOTDATA_OK);
}

static void finish(void) {
    assert(iotdata_encode_end(&enc, &pkt_len) == IOTDATA_OK);
}

static void decode_pkt(void) {
    assert(iotdata_decode(pkt, pkt_len, &dec) == IOTDATA_OK);
}

#endif /* TEST_COMMON_H */
