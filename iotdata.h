/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * iotdata.h - reference implementation header
 *
 * Ultra-efficient bit-packed telemetry protocol for remote sensor systems.
 * Designed for constrained devices (ESP32-C3, STM32, nRF52) with
 * battery-optimised LoRa / LPWAN transmission.
 *
 * Architecture:
 *   - Variant field (4 bits) identifies sensor type and determines which
 *     sensor fields occupy which presence flag slots.
 *   - All field encodings are universal — variant only affects slot
 *     assignment and labelling.
 *   - N presence bytes supported via extension bit chaining.
 *   - Variant 15 is reserved for future extended header format.
 *
 * Compile-time options (define BEFORE including this header):
 *
 *   IOTDATA_VARIANT_MAPS_DEFAULT   Default variant maps (weather station)
 *   IOTDATA_VARIANT_MAPS <sym>     Custom variant maps array symbol
 *   IOTDATA_VARIANT_MAPS_COUNT <n> Number of entries in custom maps
 *   IOTDATA_ENABLE_SELECTIVE       Only compile explicitly enabled elements
 *   IOTDATA_ENABLE_xxx             Enable individual field types
 *   IOTDATA_ENABLE_TLV             Enable TLV
 *   IOTDATA_NO_DECODE              Exclude decoder
 *   IOTDATA_NO_ENCODE              Exclude encoder
 *   IOTDATA_NO_PRINT               Exclude Print output support
 *   IOTDATA_NO_DUMP                Exclude Dump output support
 *   IOTDATA_NO_JSON                Exclude JSON support
 *   IOTDATA_NO_CHECKS_STATE        Remove runtime state checks
 *   IOTDATA_NO_CHECKS_TYPES        Remove runtime type checks (e.g. temp limits)
 *   IOTDATA_NO_ERROR_STRINGS       Exclude error strings (iotdata_strerror)
 *   IOTDATA_NO_FLOATING_DOUBLES    Use float instead of double for position
 *   IOTDATA_NO_FLOATING            Integer-only mode (no float/double)
 */

#ifndef IOTDATA_H
#define IOTDATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !defined(IOTDATA_NO_DUMP) || !defined(IOTDATA_NO_PRINT)
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Field control
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_VARIANT_MAPS_DEFAULT)
#define IOTDATA_ENABLE_TLV
#define IOTDATA_ENABLE_BATTERY
#define IOTDATA_ENABLE_LINK
#define IOTDATA_ENABLE_ENVIRONMENT
#define IOTDATA_ENABLE_WIND
#define IOTDATA_ENABLE_RAIN
#define IOTDATA_ENABLE_SOLAR
#define IOTDATA_ENABLE_CLOUDS
#define IOTDATA_ENABLE_AIR_QUALITY
#define IOTDATA_ENABLE_RADIATION
#define IOTDATA_ENABLE_POSITION
#define IOTDATA_ENABLE_DATETIME
#define IOTDATA_ENABLE_FLAGS
#endif

#if !defined(IOTDATA_ENABLE_SELECTIVE)
#define IOTDATA_ENABLE_TLV
#define IOTDATA_ENABLE_BATTERY
#define IOTDATA_ENABLE_LINK
#define IOTDATA_ENABLE_ENVIRONMENT
#define IOTDATA_ENABLE_TEMPERATURE
#define IOTDATA_ENABLE_PRESSURE
#define IOTDATA_ENABLE_HUMIDITY
#define IOTDATA_ENABLE_WIND
#define IOTDATA_ENABLE_WIND_SPEED
#define IOTDATA_ENABLE_WIND_DIRECTION
#define IOTDATA_ENABLE_WIND_GUST
#define IOTDATA_ENABLE_RAIN
#define IOTDATA_ENABLE_RAIN_RATE
#define IOTDATA_ENABLE_RAIN_SIZE
#define IOTDATA_ENABLE_SOLAR
#define IOTDATA_ENABLE_CLOUDS
#define IOTDATA_ENABLE_AIR_QUALITY
#define IOTDATA_ENABLE_RADIATION
#define IOTDATA_ENABLE_RADIATION_CPM
#define IOTDATA_ENABLE_RADIATION_DOSE
#define IOTDATA_ENABLE_DEPTH
#define IOTDATA_ENABLE_POSITION
#define IOTDATA_ENABLE_DATETIME
#define IOTDATA_ENABLE_FLAGS
#endif

/* ---------------------------------------------------------------------------
 * Field BATTERY
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_BATTERY)
#define IOTDATA_BATTERY_FIELDS \
    uint8_t battery_level; \
    bool battery_charging;
#define IOTDATA_BATTERY_LEVEL_MAX   100
#define IOTDATA_BATTERY_LEVEL_BITS  5
#define IOTDATA_BATTERY_CHARGE_BITS 1
#else
#define IOTDATA_BATTERY_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field LINK
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_LINK)
#define IOTDATA_LINK_FIELDS \
    int16_t link_rssi; \
    iotdata_float_t link_snr;
#define IOTDATA_LINK_RSSI_MIN  (-120)
#define IOTDATA_LINK_RSSI_MAX  (-60)
#define IOTDATA_LINK_RSSI_STEP (4)
#define IOTDATA_LINK_RSSI_BITS (4)
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_LINK_SNR_MIN  (-20.0f)
#define IOTDATA_LINK_SNR_MAX  (10.0f)
#define IOTDATA_LINK_SNR_STEP (10.0f)
#else
#define IOTDATA_LINK_SNR_MIN  (-200)
#define IOTDATA_LINK_SNR_MAX  (100)
#define IOTDATA_LINK_SNR_STEP (100)
#endif
#define IOTDATA_LINK_SNR_BITS (2)
#else
#define IOTDATA_LINK_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field ENVIRONMENT, TEMPERATURE, PRESSURE, HUMIDITY
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_TEMPERATURE)
#define IOTDATA_TEMPERATURE_FIELD iotdata_float_t temperature;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_TEMPERATURE_MIN (-40.0f)
#define IOTDATA_TEMPERATURE_MAX (80.0f)
#define IOTDATA_TEMPERATURE_RES (0.25f)
#else
#define IOTDATA_TEMPERATURE_MIN (-4000)
#define IOTDATA_TEMPERATURE_MAX (8000)
#define IOTDATA_TEMPERATURE_RES (25)
#endif
#define IOTDATA_TEMPERATURE_BITS 9
#else
#define IOTDATA_TEMPERATURE_FIELD
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_PRESSURE)
#define IOTDATA_PRESSURE_FIELD uint16_t pressure;
#define IOTDATA_PRESSURE_MIN   850
#define IOTDATA_PRESSURE_MAX   1105
#define IOTDATA_PRESSURE_BITS  8
#else
#define IOTDATA_PRESSURE_FIELD
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_HUMIDITY)
#define IOTDATA_HUMIDITY_FIELD uint8_t humidity;
#define IOTDATA_HUMIDITY_MAX   100
#define IOTDATA_HUMIDITY_BITS  7
#else
#define IOTDATA_HUMIDITY_FIELD
#endif
#define IOTDATA_ENVIRONMENT_FIELDS IOTDATA_TEMPERATURE_FIELD IOTDATA_PRESSURE_FIELD IOTDATA_HUMIDITY_FIELD

/* ---------------------------------------------------------------------------
 * Field WIND, WIND_SPEED, WIND_DIRECTION, WIND_GUST
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_SPEED)
#define IOTDATA_WIND_SPEED_FIELD iotdata_float_t wind_speed;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_WIND_SPEED_RES (0.5f)
#define IOTDATA_WIND_SPEED_MAX (63.5f)
#else
#define IOTDATA_WIND_SPEED_RES (50)
#define IOTDATA_WIND_SPEED_MAX (6350)
#endif
#define IOTDATA_WIND_SPEED_BITS 7
#else
#define IOTDATA_WIND_SPEED_FIELD
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_DIRECTION)
#define IOTDATA_WIND_DIRECTION_FIELD uint16_t wind_direction;
#define IOTDATA_WIND_DIRECTION_MAX   359
#define IOTDATA_WIND_DIRECTION_BITS  8
#else
#define IOTDATA_WIND_DIRECTION_FIELD
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_GUST)
#define IOTDATA_WIND_GUST_FIELD iotdata_float_t wind_gust;
#define IOTDATA_WIND_GUST_BITS  7
#else
#define IOTDATA_WIND_GUST_FIELD
#endif
#define IOTDATA_WIND_FIELDS IOTDATA_WIND_SPEED_FIELD IOTDATA_WIND_DIRECTION_FIELD IOTDATA_WIND_GUST_FIELD

/* ---------------------------------------------------------------------------
 * Field RAIN, RAIN_RATE, RAIN_SIZE
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_RATE)
#define IOTDATA_RAIN_RATE_FIELD uint8_t rain_rate;
#define IOTDATA_RAIN_RATE_MAX   255
#define IOTDATA_RAIN_RATE_BITS  8
#else
#define IOTDATA_RAIN_RATE_FIELD
#endif
#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_SIZE)
#define IOTDATA_RAIN_SIZE_FIELD uint8_t rain_size10;
#define IOTDATA_RAIN_SIZE_MAX   15
#define IOTDATA_RAIN_SIZE_BITS  4
#define IOTDATA_RAIN_SIZE_SCALE 4
#else
#define IOTDATA_RAIN_SIZE_FIELD
#endif
#define IOTDATA_RAIN_FIELDS IOTDATA_RAIN_RATE_FIELD IOTDATA_RAIN_SIZE_FIELD

/* ---------------------------------------------------------------------------
 * Field SOLAR, SOLAR_IRRADIATION, SOLAR_ULTRAVIOLET
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_SOLAR)
#define IOTDATA_SOLAR_FIELDS \
    uint16_t solar_irradiance; \
    uint8_t solar_ultraviolet;
#define IOTDATA_SOLAR_IRRADIATION_MAX  1023
#define IOTDATA_SOLAR_IRRADIATION_BITS 10
#define IOTDATA_SOLAR_ULTRAVIOLET_MAX  15
#define IOTDATA_SOLAR_ULTRAVIOLET_BITS 4
#else
#define IOTDATA_SOLAR_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field CLOUDS
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_CLOUDS)
#define IOTDATA_CLOUDS_FIELDS uint8_t clouds;
#define IOTDATA_CLOUDS_MAX    8
#define IOTDATA_CLOUDS_BITS   4
#else
#define IOTDATA_CLOUDS_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field AIR_QUALITY
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_AIR_QUALITY)
#define IOTDATA_AIR_QUALITY_FIELDS uint16_t air_quality;
#define IOTDATA_AIR_QUALITY_MAX    500
#define IOTDATA_AIR_QUALITY_BITS   9
#else
#define IOTDATA_AIR_QUALITY_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field RADIATION, RADIATION_CPM, RADIATION_DOSE
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_CPM)
#define IOTDATA_RADIATION_CPM_FIELD uint16_t radiation_cpm;
#define IOTDATA_RADIATION_CPM_MAX   16383
#define IOTDATA_RADIATION_CPM_BITS  14
#else
#define IOTDATA_RADIATION_CPM_FIELD
#endif
#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_DOSE)
#define IOTDATA_RADIATION_DOSE_FIELD iotdata_float_t radiation_dose;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_RADIATION_DOSE_MAX_RAW 16383
#define IOTDATA_RADIATION_DOSE_RES     (0.01f)
#define IOTDATA_RADIATION_DOSE_MAX     (163.83f)
#else
#define IOTDATA_RADIATION_DOSE_MAX 16383
#endif
#define IOTDATA_RADIATION_DOSE_BITS 14
#else
#define IOTDATA_RADIATION_DOSE_FIELD
#endif
#define IOTDATA_RADIATION_FIELDS IOTDATA_RADIATION_CPM_FIELD IOTDATA_RADIATION_DOSE_FIELD

/* ---------------------------------------------------------------------------
 * Field DEPTH
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_DEPTH)
#define IOTDATA_DEPTH_FIELDS uint16_t depth;
#define IOTDATA_DEPTH_MAX    1023
#define IOTDATA_DEPTH_BITS   10
#else
#define IOTDATA_DEPTH_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field POSITION
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_POSITION)
#define IOTDATA_POSITION_FIELDS \
    iotdata_double_t position_lat; \
    iotdata_double_t position_lon;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_POS_LAT_LOW  (-90.0)
#define IOTDATA_POS_LAT_HIGH (90.0)
#define IOTDATA_POS_LON_LOW  (-180.0)
#define IOTDATA_POS_LON_HIGH (180.0)
#else
#define IOTDATA_POS_LAT_LOW  (-90 * 1e7)
#define IOTDATA_POS_LAT_HIGH (90 * 1e7)
#define IOTDATA_POS_LON_LOW  (-180 * 1e7)
#define IOTDATA_POS_LON_HIGH (180 * 1e7)
#endif
#define IOTDATA_POS_LAT_BITS 24
#define IOTDATA_POS_LON_BITS 24
#define IOTDATA_POS_SCALE    16777215 /* (1 << 24) - 1 */
#define IOTDATA_POS_BITS     (IOTDATA_POS_LAT_BITS + IOTDATA_POS_LON_BITS)
#else
#define IOTDATA_POSITION_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field DATETIME
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_DATETIME)
#define IOTDATA_DATETIME_FIELDS uint32_t datetime_secs;
#define IOTDATA_DATETIME_BITS   24
#define IOTDATA_DATETIME_RES    5
#define IOTDATA_DATETIME_MAX    ((1 << IOTDATA_DATETIME_BITS) - 1)
#else
#define IOTDATA_DATETIME_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field FLAGS
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_FLAGS)
#define IOTDATA_FLAGS_FIELDS uint8_t flags;
#define IOTDATA_FLAGS_BITS   8
#else
#define IOTDATA_FLAGS_FIELDS
#endif

/* ---------------------------------------------------------------------------
 * Field TLV
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_TLV)

#define IOTDATA_TLV_MAX         8

#define IOTDATA_TLV_DATA_MAX    255
#define IOTDATA_TLV_STR_LEN_MAX 255

#if !defined(IOTDATA_NO_ENCODE)
#define IOTDATA_TLV_FIELDS_ENCODE \
    uint8_t tlv_count; \
    struct { \
        uint8_t format; \
        uint8_t type; \
        uint8_t length; \
        const uint8_t *data; \
        const char *str; \
    } tlv[IOTDATA_TLV_MAX];
#else
#define IOTDATA_TLV_FIELDS_ENCODE
#endif
#if !defined(IOTDATA_NO_DECODE)
#define IOTDATA_TLV_FIELDS_DECODE \
    uint8_t tlv_count; \
    struct { \
        uint8_t format; \
        uint8_t type; \
        uint8_t length; \
        union { \
            uint8_t raw[IOTDATA_TLV_DATA_MAX]; \
            char str[IOTDATA_TLV_STR_LEN_MAX + 1]; \
        }; \
    } tlv[IOTDATA_TLV_MAX];
#else
#define IOTDATA_TLV_FIELDS_DECODE
#endif

#define IOTDATA_TLV_FMT_BITS    1
#define IOTDATA_TLV_TYPE_BITS   6
#define IOTDATA_TLV_TYPE_MAX    63
#define IOTDATA_TLV_MORE_BITS   1
#define IOTDATA_TLV_LENGTH_BITS 8
#define IOTDATA_TLV_HEADER_BITS (IOTDATA_TLV_FMT_BITS + IOTDATA_TLV_TYPE_BITS + IOTDATA_TLV_MORE_BITS + IOTDATA_TLV_LENGTH_BITS)
#define IOTDATA_TLV_FMT_RAW     0
#define IOTDATA_TLV_FMT_STRING  1
#define IOTDATA_TLV_CHAR_BITS   6

#define IOTDATA_TLV_UPTIME      0x01
#define IOTDATA_TLV_RESETS      0x02
#define IOTDATA_TLV_LOGGER      0x03
#define IOTDATA_TLV_CONFIG      0x04
#define IOTDATA_TLV_USERDATA    0x05
#else
#define IOTDATA_TLV_FIELDS_ENCODE
#define IOTDATA_TLV_FIELDS_DECODE
#endif

/* ---------------------------------------------------------------------------
 * Type aliases for float/int mode selection
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_NO_FLOATING)
typedef int32_t iotdata_double_t; /* degrees * 1e7 */
typedef int32_t iotdata_float_t;  /* value * 100    */
#elif defined(IOTDATA_NO_FLOATING_DOUBLES)
typedef float iotdata_double_t;
typedef float iotdata_float_t;
#else
typedef double iotdata_double_t;
typedef float iotdata_float_t;
#endif

/* ---------------------------------------------------------------------------
 * Header: variant(4) + station(12) + sequence(16) = 32 bits
 * -------------------------------------------------------------------------*/

#define IOTDATA_VARIANT_BITS          4
#define IOTDATA_STATION_BITS          12
#define IOTDATA_SEQUENCE_BITS         16
#define IOTDATA_HEADER_BITS           (IOTDATA_VARIANT_BITS + IOTDATA_STATION_BITS + IOTDATA_SEQUENCE_BITS)

#define IOTDATA_VARIANT_MAX           14
#define IOTDATA_VARIANT_RESERVED      15
#define IOTDATA_STATION_MAX           4095
#define IOTDATA_SEQUENCE_MAX          65535

/* ---------------------------------------------------------------------------
 * Presence byte layout (N-byte extension chain)
 *
 * Byte 0:
 *   bit 7: extension (more presence bytes follow)
 *   bit 6: TLV Data present (even if not supported in encoder)
 *   bits 5-0: data slots [0]-[5]
 *
 * Byte 1..N:
 *   bit 7: extension (more presence bytes follow)
 *   bits 6-0: data slots, 7 per byte
 *
 * Total data slots = 6 + 7*(num_pres_bytes - 1)
 *
 * This implementation limits to a total of 4 presence bytes, i.e. 48 slots,
 * but that is not a limitation in the protocol.
 * -------------------------------------------------------------------------*/

#define IOTDATA_PRES_TLV              (1U << 6)
#define IOTDATA_PRES_EXT              (1U << 7)
#define IOTDATA_PRES0_DATA_FIELDS     6
#define IOTDATA_PRESN_DATA_FIELDS     7
#define IOTDATA_PRES_MINIMUM          1
#define IOTDATA_PRES_MAXIMUM          4
#define IOTDATA_MAX_DATA_FIELDS       (IOTDATA_PRES0_DATA_FIELDS + IOTDATA_PRESN_DATA_FIELDS * (IOTDATA_PRES_MAXIMUM - 1))

/* ---------------------------------------------------------------------------
 * Packet
 * -------------------------------------------------------------------------*/

#define IOTDATA_PACKET_MINIMUM        ((IOTDATA_HEADER_BITS / 8) + IOTDATA_PRES_MINIMUM)
#define IOTDATA_PACKET_MAXIMUM_NO_TLV (128) // XXX fix me

/* ---------------------------------------------------------------------------
 * Field types
 * -------------------------------------------------------------------------*/

typedef enum {
    IOTDATA_FIELD_NONE = -1,
#if defined(IOTDATA_ENABLE_BATTERY)
    IOTDATA_FIELD_BATTERY, /*  6 bits */
#endif
#if defined(IOTDATA_ENABLE_LINK)
    IOTDATA_FIELD_LINK, /*  6 bits */
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT)
    IOTDATA_FIELD_ENVIRONMENT, /* 24 bits */
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE)
    IOTDATA_FIELD_TEMPERATURE, /*  9 bits */
#endif
#if defined(IOTDATA_ENABLE_PRESSURE)
    IOTDATA_FIELD_PRESSURE, /*  8 bits */
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY)
    IOTDATA_FIELD_HUMIDITY, /*  7 bits */
#endif
#if defined(IOTDATA_ENABLE_WIND)
    IOTDATA_FIELD_WIND, /* 22 bits */
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED)
    IOTDATA_FIELD_WIND_SPEED, /*  7 bits */
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION)
    IOTDATA_FIELD_WIND_DIRECTION, /*  8 bits */
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST)
    IOTDATA_FIELD_WIND_GUST, /*  7 bits */
#endif
#if defined(IOTDATA_ENABLE_RAIN)
    IOTDATA_FIELD_RAIN, /* 12 bits */
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE)
    IOTDATA_FIELD_RAIN_RATE, /*  8 bits */
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE)
    IOTDATA_FIELD_RAIN_SIZE, /*  4 bits */
#endif
#if defined(IOTDATA_ENABLE_SOLAR)
    IOTDATA_FIELD_SOLAR, /* 14 bits */
#endif
#if defined(IOTDATA_ENABLE_CLOUDS)
    IOTDATA_FIELD_CLOUDS, /*  4 bits */
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY)
    IOTDATA_FIELD_AIR_QUALITY, /*  9 bits */
#endif
#if defined(IOTDATA_ENABLE_RADIATION)
    IOTDATA_FIELD_RADIATION, /* 30 bits */
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM)
    IOTDATA_FIELD_RADIATION_CPM, /* 16 bits */
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE)
    IOTDATA_FIELD_RADIATION_DOSE, /* 14 bits */
#endif
#if defined(IOTDATA_ENABLE_DEPTH)
    IOTDATA_FIELD_DEPTH, /* 10 bits */
#endif
#if defined(IOTDATA_ENABLE_POSITION)
    IOTDATA_FIELD_POSITION, /* 48 bits */
#endif
#if defined(IOTDATA_ENABLE_DATETIME)
    IOTDATA_FIELD_DATETIME, /* 24 bits */
#endif
#if defined(IOTDATA_ENABLE_FLAGS)
    IOTDATA_FIELD_FLAGS, /*  8 bits */
#endif
    IOTDATA_FIELD_COUNT,
#if defined(IOTDATA_ENABLE_TLV)
    IOTDATA_FIELD_TLV = 31,
#endif
} iotdata_field_type_t;

_Static_assert(IOTDATA_FIELD_COUNT <= 32, "fields overflow");

#define IOTDATA_FIELD_EMPTY             (0)
#define IOTDATA_FIELD_BIT(id)           (1U << (id))
#define IOTDATA_FIELD_PRESENT(mask, id) (((mask) >> (id)) & 1U)
#define IOTDATA_FIELD_SET(mask, id)     ((mask) |= (1U << (id)))
#define IOTDATA_FIELD_CLR(mask, id)     ((mask) &= ~(1U << (id)))
#if defined(IOTDATA_ENABLE_TLV)
#define IOTDATA_FIELD_VALID(id) ((id) != IOTDATA_FIELD_NONE && (id) != IOTDATA_FIELD_TLV)
#else
#define IOTDATA_FIELD_VALID(id) ((id) != IOTDATA_FIELD_NONE)
#endif

/* ---------------------------------------------------------------------------
 * Variant definition — maps presence slots to field types
 *
 * Flat slot array covering all presence bytes. Entries 0..5 map to pres0,
 * entries 6..12 to pres1, entries 13..19 to pres2, etc.
 * Unused trailing slots should be IOTDATA_FIELD_NONE / NULL.
 * -------------------------------------------------------------------------*/

typedef struct {
    iotdata_field_type_t type;
    const char *label;
} iotdata_field_def_t;

typedef struct {
    const char *name;
    uint8_t num_pres_bytes;
    iotdata_field_def_t fields[IOTDATA_MAX_DATA_FIELDS];
} iotdata_variant_def_t;

const iotdata_variant_def_t *iotdata_get_variant(uint8_t variant);

#define IOTDATA_MAX_PACKET_SIZE 256

/* ---------------------------------------------------------------------------
 * Status/error codes
 * -------------------------------------------------------------------------*/

typedef enum {
    IOTDATA_OK = 0,

#if !defined(IOTDATA_NO_ENCODE)
    IOTDATA_ERR_CTX_NULL,
    IOTDATA_ERR_CTX_NOT_BEGUN,
    IOTDATA_ERR_CTX_ALREADY_BEGUN,
    IOTDATA_ERR_CTX_ALREADY_ENDED,
    IOTDATA_ERR_CTX_DUPLICATE_FIELD,
    IOTDATA_ERR_BUF_NULL,
    IOTDATA_ERR_BUF_OVERFLOW,
    IOTDATA_ERR_BUF_TOO_SMALL,
#elif !defined(IOTDATA_NO_DUMP)
    IOTDATA_ERR_CTX_NULL,
#endif

#if !defined(IOTDATA_NO_DECODE)
    IOTDATA_ERR_DECODE_SHORT,
    IOTDATA_ERR_DECODE_VARIANT,
    IOTDATA_ERR_DECODE_TRUNCATED,
#elif !defined(IOTDATA_NO_DUMP)
    IOTDATA_ERR_DECODE_SHORT,
#endif

#if !defined(IOTDATA_NO_DUMP)
    IOTDATA_ERR_DUMP_ALLOC,
#endif

#if !defined(IOTDATA_NO_PRINT)
    IOTDATA_ERR_PRINT_ALLOC,
#endif

#if !defined(IOTDATA_NO_JSON)
    IOTDATA_ERR_JSON_PARSE,
    IOTDATA_ERR_JSON_ALLOC,
    IOTDATA_ERR_JSON_MISSING_FIELD,
#endif

    IOTDATA_ERR_HDR_VARIANT_HIGH,
    IOTDATA_ERR_HDR_VARIANT_RESERVED,
    IOTDATA_ERR_HDR_VARIANT_UNKNOWN,
    IOTDATA_ERR_HDR_STATION_HIGH,

#if defined(IOTDATA_ENABLE_TLV)
    IOTDATA_ERR_TLV_TYPE_HIGH,
    IOTDATA_ERR_TLV_DATA_NULL,
    IOTDATA_ERR_TLV_LEN_HIGH,
    IOTDATA_ERR_TLV_FULL,
    IOTDATA_ERR_TLV_STR_NULL,
    IOTDATA_ERR_TLV_STR_LEN_HIGH,
    IOTDATA_ERR_TLV_STR_CHAR_INVALID,
#endif

//

#if defined(IOTDATA_ENABLE_BATTERY)
    IOTDATA_ERR_BATTERY_LEVEL_HIGH,
#endif

#if defined(IOTDATA_ENABLE_LINK)
    IOTDATA_ERR_LINK_RSSI_LOW,
    IOTDATA_ERR_LINK_RSSI_HIGH,
    IOTDATA_ERR_LINK_SNR_LOW,
    IOTDATA_ERR_LINK_SNR_HIGH,
#endif

#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_TEMPERATURE)
    IOTDATA_ERR_TEMPERATURE_LOW,
    IOTDATA_ERR_TEMPERATURE_HIGH,
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_PRESSURE)
    IOTDATA_ERR_PRESSURE_LOW,
    IOTDATA_ERR_PRESSURE_HIGH,
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_HUMIDITY)
    IOTDATA_ERR_HUMIDITY_HIGH,
#endif

#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_SPEED)
    IOTDATA_ERR_WIND_SPEED_HIGH,
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_DIRECTION)
    IOTDATA_ERR_WIND_DIRECTION_HIGH,
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_GUST)
    IOTDATA_ERR_WIND_GUST_HIGH,
#endif

#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_RATE)
    IOTDATA_ERR_RAIN_RATE_HIGH,
#endif
#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_SIZE)
    IOTDATA_ERR_RAIN_SIZE_HIGH,
#endif

#if defined(IOTDATA_ENABLE_SOLAR)
    IOTDATA_ERR_SOLAR_IRRADIATION_HIGH,
    IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH,
#endif

#if defined(IOTDATA_ENABLE_CLOUDS)
    IOTDATA_ERR_CLOUDS_HIGH,
#endif

#if defined(IOTDATA_ENABLE_AIR_QUALITY)
    IOTDATA_ERR_AIR_QUALITY_HIGH,
#endif

#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_CPM)
    IOTDATA_ERR_RADIATION_CPM_HIGH,
#endif
#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_DOSE)
    IOTDATA_ERR_RADIATION_DOSE_HIGH,
#endif

#if defined(IOTDATA_ENABLE_DEPTH)
    IOTDATA_ERR_DEPTH_HIGH,
#endif

#if defined(IOTDATA_ENABLE_POSITION)
    IOTDATA_ERR_POSITION_LAT_LOW,
    IOTDATA_ERR_POSITION_LAT_HIGH,
    IOTDATA_ERR_POSITION_LON_LOW,
    IOTDATA_ERR_POSITION_LON_HIGH,
#endif

#if defined(IOTDATA_ENABLE_DATETIME)
    IOTDATA_ERR_DATETIME_HIGH,
#endif

} iotdata_status_t;

typedef enum {
    IOTDATA_STATE_IDLE = 0,
    IOTDATA_STATE_BEGUN,
    IOTDATA_STATE_ENDED,
} iotdata_state_t;

typedef uint32_t iotdata_field_t;

/* ---------------------------------------------------------------------------
 * Encoder context
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_ENCODE)
typedef struct {
    uint8_t *buf;
    size_t buf_size;
    iotdata_state_t state;

    size_t packed_bits;
    size_t packed_bytes;

    uint8_t variant;
    uint16_t station;
    uint16_t sequence;
    iotdata_field_t fields;

    IOTDATA_BATTERY_FIELDS
    IOTDATA_LINK_FIELDS
    IOTDATA_ENVIRONMENT_FIELDS
    IOTDATA_WIND_FIELDS
    IOTDATA_RAIN_FIELDS
    IOTDATA_SOLAR_FIELDS
    IOTDATA_CLOUDS_FIELDS
    IOTDATA_AIR_QUALITY_FIELDS
    IOTDATA_RADIATION_FIELDS
    IOTDATA_DEPTH_FIELDS
    IOTDATA_POSITION_FIELDS
    IOTDATA_DATETIME_FIELDS
    IOTDATA_FLAGS_FIELDS

    IOTDATA_TLV_FIELDS_ENCODE
} iotdata_encoder_t;
#endif /* !IOTDATA_NO_ENCODE */

/* ---------------------------------------------------------------------------
 * Decoded content
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DECODE)
typedef struct {
    size_t packed_bits;
    size_t packed_bytes;

    uint8_t variant;
    uint16_t station;
    uint16_t sequence;
    iotdata_field_t fields;

    IOTDATA_BATTERY_FIELDS
    IOTDATA_LINK_FIELDS
    IOTDATA_ENVIRONMENT_FIELDS
    IOTDATA_WIND_FIELDS
    IOTDATA_RAIN_FIELDS
    IOTDATA_SOLAR_FIELDS
    IOTDATA_CLOUDS_FIELDS
    IOTDATA_AIR_QUALITY_FIELDS
    IOTDATA_RADIATION_FIELDS
    IOTDATA_DEPTH_FIELDS
    IOTDATA_POSITION_FIELDS
    IOTDATA_DATETIME_FIELDS
    IOTDATA_FLAGS_FIELDS

    IOTDATA_TLV_FIELDS_DECODE
} iotdata_decoded_t;
#endif /* !IOTDATA_NO_DECODE */

/* ---------------------------------------------------------------------------
 * Encoder
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_begin(iotdata_encoder_t *enc, uint8_t *buf, size_t buf_size, uint8_t variant, uint16_t station, uint16_t sequence);
iotdata_status_t iotdata_encode_end(iotdata_encoder_t *enc, size_t *out_bytes);
#if defined(IOTDATA_ENABLE_TLV)
iotdata_status_t iotdata_encode_tlv(iotdata_encoder_t *enc, uint8_t type, const uint8_t *data, uint8_t length);
iotdata_status_t iotdata_encode_tlv_string(iotdata_encoder_t *enc, uint8_t type, const char *str);
#endif
#if defined(IOTDATA_ENABLE_BATTERY)
iotdata_status_t iotdata_encode_battery(iotdata_encoder_t *enc, uint8_t level_percent, bool charging);
#endif
#if defined(IOTDATA_ENABLE_LINK)
iotdata_status_t iotdata_encode_link(iotdata_encoder_t *enc, int16_t rssi_dbm, iotdata_float_t snr_db);
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT)
iotdata_status_t iotdata_encode_environment(iotdata_encoder_t *enc, iotdata_float_t temp_c, uint16_t pressure_hpa, uint8_t humidity_pct);
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE)
iotdata_status_t iotdata_encode_temperature(iotdata_encoder_t *enc, iotdata_float_t temperature_c);
#endif
#if defined(IOTDATA_ENABLE_PRESSURE)
iotdata_status_t iotdata_encode_pressure(iotdata_encoder_t *enc, uint16_t pressure_hpa);
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY)
iotdata_status_t iotdata_encode_humidity(iotdata_encoder_t *enc, uint8_t humidity_pct);
#endif
#if defined(IOTDATA_ENABLE_WIND)
iotdata_status_t iotdata_encode_wind(iotdata_encoder_t *enc, iotdata_float_t speed_ms, uint16_t direction_deg, iotdata_float_t gust_ms);
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED)
iotdata_status_t iotdata_encode_wind_speed(iotdata_encoder_t *enc, iotdata_float_t speed_ms);
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION)
iotdata_status_t iotdata_encode_wind_direction(iotdata_encoder_t *enc, uint16_t direction_deg);
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST)
iotdata_status_t iotdata_encode_wind_gust(iotdata_encoder_t *enc, iotdata_float_t gust_ms);
#endif
#if defined(IOTDATA_ENABLE_RAIN)
iotdata_status_t iotdata_encode_rain(iotdata_encoder_t *enc, uint8_t rate_mmhr, uint8_t size_mmd);
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE)
iotdata_status_t iotdata_encode_rain_rate(iotdata_encoder_t *enc, uint8_t rate_mmhr);
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE)
iotdata_status_t iotdata_encode_rain_size(iotdata_encoder_t *enc, uint8_t size_mmd);
#endif
#if defined(IOTDATA_ENABLE_SOLAR)
iotdata_status_t iotdata_encode_solar(iotdata_encoder_t *enc, uint16_t irradiance_wm2, uint8_t ultraviolet_index);
#endif
#if defined(IOTDATA_ENABLE_CLOUDS)
iotdata_status_t iotdata_encode_clouds(iotdata_encoder_t *enc, uint8_t okta);
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY)
iotdata_status_t iotdata_encode_air_quality(iotdata_encoder_t *enc, uint16_t aqi);
#endif
#if defined(IOTDATA_ENABLE_RADIATION)
iotdata_status_t iotdata_encode_radiation(iotdata_encoder_t *enc, uint16_t cpm, iotdata_float_t usvh);
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM)
iotdata_status_t iotdata_encode_radiation_cpm(iotdata_encoder_t *enc, uint16_t cpm);
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE)
iotdata_status_t iotdata_encode_radiation_dose(iotdata_encoder_t *enc, iotdata_float_t usvh);
#endif
#if defined(IOTDATA_ENABLE_DEPTH)
iotdata_status_t iotdata_encode_depth(iotdata_encoder_t *enc, uint16_t depth_cm);
#endif
#if defined(IOTDATA_ENABLE_POSITION)
iotdata_status_t iotdata_encode_position(iotdata_encoder_t *enc, iotdata_double_t latitude, iotdata_double_t longitude);
#endif
#if defined(IOTDATA_ENABLE_DATETIME)
iotdata_status_t iotdata_encode_datetime(iotdata_encoder_t *enc, uint32_t seconds_from_year_start);
#endif
#if defined(IOTDATA_ENABLE_FLAGS)
iotdata_status_t iotdata_encode_flags(iotdata_encoder_t *enc, uint8_t flags);
#endif
#endif /* !IOTDATA_NO_ENCODE */

/* ---------------------------------------------------------------------------
 * Decoder
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DECODE)
iotdata_status_t iotdata_peek(const uint8_t *buf, size_t len, uint8_t *variant, uint16_t *station, uint16_t *sequence);
iotdata_status_t iotdata_decode(const uint8_t *buf, size_t len, iotdata_decoded_t *out);
#endif /* !IOTDATA_NO_DECODE */

/* ---------------------------------------------------------------------------
 * Dump, Print (requires decoder), JSON (requires encoder/decoder)
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DUMP)
iotdata_status_t iotdata_dump_to_file(const uint8_t *buf, size_t len, FILE *fp, bool verbose);
iotdata_status_t iotdata_dump_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size, bool verbose);
#endif /* !IOTDATA_NO_DUMP */

#if !defined(IOTDATA_NO_PRINT)
#if !defined(IOTDATA_NO_DECODE)
iotdata_status_t iotdata_print_decoded_to_file(const iotdata_decoded_t *dec, FILE *fp);
iotdata_status_t iotdata_print_decoded_to_string(const iotdata_decoded_t *dec, char *out, size_t out_size);
#endif
iotdata_status_t iotdata_print_to_file(const uint8_t *buf, size_t len, FILE *fp);
iotdata_status_t iotdata_print_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size);
#endif /* !IOTDATA_NO_PRINT */

#if !defined(IOTDATA_NO_JSON)
#if !defined(IOTDATA_NO_DECODE)
iotdata_status_t iotdata_decode_to_json(const uint8_t *buf, size_t len, char **json_out);
#endif /* !IOTDATA_NO_DECODE */
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_from_json(const char *json, uint8_t *buf, size_t buf_size, size_t *out_bytes);
#endif /* !IOTDATA_NO_ENCODE */
#endif /* !IOTDATA_NO_JSON */

/* ---------------------------------------------------------------------------
 * Ancillary
 * -------------------------------------------------------------------------*/

#if !defined(IODATA_NO_ERROR_STRINGS)
const char *iotdata_strerror(iotdata_status_t status);
#endif

#ifdef __cplusplus
}
#endif

#endif /* IOTDATA_H */
