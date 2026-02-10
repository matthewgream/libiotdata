/*
 * iotdata.h - IoT Sensor Telemetry Protocol
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
 *   IOTDATA_ENCODE_ONLY            Exclude decoder/JSON/print/dump
 *   IOTDATA_DECODE_ONLY            Exclude encoder
 *   IOTDATA_NO_PRINT               Exclude Print output support
 *   IOTDATA_NO_DUMP                Exclude Dump output support
 *   IOTDATA_NO_JSON                Exclude JSON support
 *   IOTDATA_NO_ERROR_STRINGS       Exclude error strings (iotdata_strerror)
 *   IOTDATA_NO_FLOATING_DOUBLES    Use float instead of double for position
 *   IOTDATA_NO_FLOATING            Integer-only mode (no float/double)
 */

#ifndef IOTDATA_H
#define IOTDATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !(defined(IOTDATA_NO_DUMP) && defined(IOTDATA_NO_PRINT))
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
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
 * Header: variant(4) + station_id(12) + sequence(16) = 32 bits
 * -------------------------------------------------------------------------*/

#define IOTDATA_VARIANT_BITS      4
#define IOTDATA_STATION_ID_BITS   12
#define IOTDATA_SEQUENCE_BITS     16
#define IOTDATA_HEADER_BITS       (IOTDATA_VARIANT_BITS + IOTDATA_STATION_ID_BITS + IOTDATA_SEQUENCE_BITS)

#define IOTDATA_VARIANT_MAX       14
#define IOTDATA_VARIANT_RESERVED  15
#define IOTDATA_STATION_ID_MAX    4095
#define IOTDATA_SEQUENCE_MAX      65535

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
 * This implementation limits to a total of 4 presence bytes, i.e. 48 slots
 * -------------------------------------------------------------------------*/

#define IOTDATA_PRES_TLV          (1U << 6)
#define IOTDATA_PRES_EXTENSION    (1U << 7)
#define IOTDATA_PRES0_DATA_FIELDS 6
#define IOTDATA_PRESN_DATA_FIELDS 7
#define IOTDATA_MAX_PRES_BYTES    4
#define IOTDATA_MAX_DATA_FIELDS   (IOTDATA_PRES0_DATA_FIELDS + IOTDATA_PRESN_DATA_FIELDS * (IOTDATA_MAX_PRES_BYTES - 1))

/* ---------------------------------------------------------------------------
 * Field types — encoding types that can occupy a presence slot
 * -------------------------------------------------------------------------*/

typedef enum {
    IOTDATA_FIELD_NONE = 0,
    IOTDATA_FIELD_TLV = (1U << 0),
    IOTDATA_FIELD_BATTERY = (1U << 1),         /*  6 bits */
    IOTDATA_FIELD_LINK = (1U << 2),            /*  6 bits */
    IOTDATA_FIELD_ENVIRONMENT = (1U << 3),     /* 24 bits */
    IOTDATA_FIELD_TEMPERATURE = (1U << 4),     /*  9 bits */
    IOTDATA_FIELD_PRESSURE = (1U << 5),        /*  8 bits */
    IOTDATA_FIELD_HUMIDITY = (1U << 6),        /*  7 bits */
    IOTDATA_FIELD_WIND = (1U << 7),            /* 22 bits */
    IOTDATA_FIELD_WIND_SPEED = (1U << 8),      /*  7 bits */
    IOTDATA_FIELD_WIND_DIRECTION = (1U << 9),  /*  8 bits */
    IOTDATA_FIELD_WIND_GUST = (1U << 10),      /*  7 bits */
    IOTDATA_FIELD_RAIN = (1U << 11),           /* 16 bits */
    IOTDATA_FIELD_RAIN_RATE = (1U << 12),      /*  8 bits */
    IOTDATA_FIELD_RAIN_SIZE = (1U << 13),      /*  8 bits */
    IOTDATA_FIELD_SOLAR = (1U << 14),          /* 14 bits */
    IOTDATA_FIELD_CLOUDS = (1U << 15),         /*  4 bits */
    IOTDATA_FIELD_AIR_QUALITY = (1U << 16),    /*  9 bits */
    IOTDATA_FIELD_RADIATION = (1U << 17),      /* 30 bits */
    IOTDATA_FIELD_RADIATION_CPM = (1U << 18),  /* 16 bits */
    IOTDATA_FIELD_RADIATION_DOSE = (1U << 19), /* 14 bits */
    IOTDATA_FIELD_DEPTH = (1U << 20),          /* 10 bits */
    IOTDATA_FIELD_POSITION = (1U << 21),       /* 48 bits */
    IOTDATA_FIELD_DATETIME = (1U << 22),       /* 24 bits */
    IOTDATA_FIELD_FLAGS = (1U << 23),          /*  8 bits */
    IOTDATA_FIELD_COUNT
} iotdata_field_type_t;

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

/* ---------------------------------------------------------------------------
 * Field encoding constants
 * -------------------------------------------------------------------------*/

/* Battery */
#define IOTDATA_BATTERY_LEVEL_MAX   100
#define IOTDATA_BATTERY_LEVEL_BITS  5
#define IOTDATA_BATTERY_CHARGE_BITS 1
#define IOTDATA_BATTERY_BITS        (IOTDATA_BATTERY_LEVEL_BITS + IOTDATA_BATTERY_CHARGE_BITS)

/* Link */
#define IOTDATA_LINK_RSSI_MIN       (-120)
#define IOTDATA_LINK_RSSI_MAX       (-60)
#define IOTDATA_LINK_RSSI_STEP      (4)
#define IOTDATA_LINK_RSSI_BITS      (4)
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
#define IOTDATA_LINK_BITS     (IOTDATA_LINK_RSSI_BITS + IOTDATA_LINK_SNR_BITS)

/* Environment (temp + pressure + humidity) */
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
#define IOTDATA_PRESSURE_MIN     850
#define IOTDATA_PRESSURE_MAX     1105
#define IOTDATA_PRESSURE_BITS    8
#define IOTDATA_HUMIDITY_MAX     100
#define IOTDATA_HUMIDITY_BITS    7
#define IOTDATA_BITS             (IOTDATA_TEMPERATURE_BITS + IOTDATA_PRESSURE_BITS + IOTDATA_HUMIDITY_BITS)

/* Wind */
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_WIND_SPEED_RES (0.5f)
#define IOTDATA_WIND_SPEED_MAX (63.5f)
#else
#define IOTDATA_WIND_SPEED_RES (50)
#define IOTDATA_WIND_SPEED_MAX (6350)
#endif
#define IOTDATA_WIND_SPEED_BITS        7
#define IOTDATA_WIND_DIRECTION_MAX     359
#define IOTDATA_WIND_DIRECTION_BITS    8
#define IOTDATA_WIND_GUST_BITS         7
#define IOTDATA_WIND_BITS              (IOTDATA_WIND_SPEED_BITS + IOTDATA_WIND_DIRECTION_BITS + IOTDATA_WIND_GUST_BITS)

/* Rain */
#define IOTDATA_RAIN_RATE_MAX          255
#define IOTDATA_RAIN_RATE_BITS         8
#define IOTDATA_RAIN_SIZE_MAX          15
#define IOTDATA_RAIN_SIZE_BITS         4
#define IOTDATA_RAIN_SIZE_SCALE        4

/* Solar/UV */
#define IOTDATA_SOLAR_IRRADIATION_MAX  1023
#define IOTDATA_SOLAR_IRRADIATION_BITS 10
#define IOTDATA_SOLAR_ULTRAVIOLET_MAX  15
#define IOTDATA_SOLAR_ULTRAVIOLET_BITS 4
#define IOTDATA_SOLAR_BITS             (IOTDATA_SOLAR_IRRADIATION_BITS + IOTDATA_SOLAR_ULTRAVIOLET_BITS)

/* Cloud (okta) */
#define IOTDATA_CLOUD_MAX              8
#define IOTDATA_CLOUD_BITS             4

/* Air quality (EPA AQI) */
#define IOTDATA_AIR_QUALITY_MAX        500
#define IOTDATA_AIR_QUALITY_BITS       9

/* Radiation CPM / Dose (µSv/h) */
#define IOTDATA_RADIATION_CPM_MAX      16383
#define IOTDATA_RADIATION_CPM_BITS     14
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_RADIATION_DOSE_MAX_RAW 16383
#define IOTDATA_RADIATION_DOSE_RES     (0.01f)
#define IOTDATA_RADIATION_DOSE_MAX     (163.83f)
#else
#define IOTDATA_RADIATION_DOSE_MAX 16383
#endif
#define IOTDATA_RADIATION_DOSE_BITS 14

/* Depth (snow, ice, water level) */
#define IOTDATA_DEPTH_MAX           1023
#define IOTDATA_DEPTH_BITS          10

/* Position */
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
#define IOTDATA_POS_LAT_BITS    24
#define IOTDATA_POS_LON_BITS    24
#define IOTDATA_POS_SCALE       16777215 /* (1 << 24) - 1 */
#define IOTDATA_POS_BITS        (IOTDATA_POS_LAT_BITS + IOTDATA_POS_LON_BITS)

/* Datetime */
#define IOTDATA_DATETIME_BITS   24
#define IOTDATA_DATETIME_RES    5
#define IOTDATA_DATETIME_MAX    ((1 << IOTDATA_DATETIME_BITS) - 1)

/* Flags */
#define IOTDATA_FLAGS_BITS      8

/* TLV */
#define IOTDATA_TLV_FMT_BITS    1
#define IOTDATA_TLV_TYPE_BITS   6
#define IOTDATA_TLV_TYPE_MAX    63
#define IOTDATA_TLV_MORE_BITS   1
#define IOTDATA_TLV_LENGTH_BITS 8
#define IOTDATA_TLV_HEADER_BITS (IOTDATA_TLV_FMT_BITS + IOTDATA_TLV_TYPE_BITS + IOTDATA_TLV_MORE_BITS + IOTDATA_TLV_LENGTH_BITS)
#define IOTDATA_TLV_DATA_MAX    255
#define IOTDATA_MAX_TLV         8
#define IOTDATA_TLV_FMT_RAW     0
#define IOTDATA_TLV_FMT_STRING  1
#define IOTDATA_TLV_CHAR_BITS   6
#define IOTDATA_TLV_STR_LEN_MAX 255

/* Well-known TLV types */
#define IOTDATA_TLV_UPTIME      0x01
#define IOTDATA_TLV_RESETS      0x02
#define IOTDATA_TLV_LOGGER      0x03
#define IOTDATA_TLV_CONFIG      0x04
#define IOTDATA_TLV_USERDATA    0x05

/* Buffer sizing */
#define IOTDATA_MAX_PACKET_SIZE 256

/* ---------------------------------------------------------------------------
 * Element-level compilation control
 * -------------------------------------------------------------------------*/

#ifdef IOTDATA_VARIANT_MAPS_DEFAULT
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

#ifndef IOTDATA_ENABLE_SELECTIVE
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
 * Status/error codes
 * -------------------------------------------------------------------------*/

typedef enum {
    IOTDATA_OK = 0,

    IOTDATA_ERR_CTX_NULL = 100,
    IOTDATA_ERR_CTX_NOT_BEGUN,
    IOTDATA_ERR_CTX_ALREADY_BEGUN,
    IOTDATA_ERR_CTX_ALREADY_ENDED,
    IOTDATA_ERR_CTX_DUPLICATE_FIELD,

    IOTDATA_ERR_BUF_NULL = 200,
    IOTDATA_ERR_BUF_OVERFLOW,
    IOTDATA_ERR_BUF_TOO_SMALL,

    IOTDATA_ERR_DECODE_SHORT = 300,
    IOTDATA_ERR_DECODE_VARIANT,
    IOTDATA_ERR_DECODE_TRUNCATED,

    IOTDATA_ERR_JSON_PARSE = 400,
    IOTDATA_ERR_JSON_ALLOC,
    IOTDATA_ERR_JSON_MISSING_FIELD,

    IOTDATA_ERR_HDR_VARIANT_HIGH = 500,
    IOTDATA_ERR_HDR_VARIANT_RESERVED,
    IOTDATA_ERR_HDR_VARIANT_UNKNOWN,
    IOTDATA_ERR_HDR_STATION_HIGH,

    IOTDATA_ERR_TLV_TYPE_HIGH = 600,
    IOTDATA_ERR_TLV_DATA_NULL,
    IOTDATA_ERR_TLV_LEN_HIGH,
    IOTDATA_ERR_TLV_FULL,
    IOTDATA_ERR_TLV_STR_NULL,
    IOTDATA_ERR_TLV_STR_LEN_HIGH,
    IOTDATA_ERR_TLV_STR_CHAR_INVALID,

    //

    IOTDATA_ERR_BATTERY_LEVEL_HIGH = 700,

    IOTDATA_ERR_LINK_RSSI_LOW = 800,
    IOTDATA_ERR_LINK_RSSI_HIGH,
    IOTDATA_ERR_LINK_SNR_LOW,
    IOTDATA_ERR_LINK_SNR_HIGH,

    IOTDATA_ERR_TEMPERATURE_LOW = 900,
    IOTDATA_ERR_TEMPERATURE_HIGH,
    IOTDATA_ERR_PRESSURE_LOW,
    IOTDATA_ERR_PRESSURE_HIGH,
    IOTDATA_ERR_HUMIDITY_HIGH,

    IOTDATA_ERR_WIND_SPEED_HIGH = 1000,
    IOTDATA_ERR_WIND_DIRECTION_HIGH,
    IOTDATA_ERR_WIND_GUST_HIGH,

    IOTDATA_ERR_RAIN_RATE_HIGH = 1100,
    IOTDATA_ERR_RAIN_SIZE_HIGH,

    IOTDATA_ERR_SOLAR_IRRADIATION_HIGH = 1200,
    IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH,

    IOTDATA_ERR_CLOUDS_HIGH = 1300,

    IOTDATA_ERR_AIR_QUALITY_HIGH = 1400,

    IOTDATA_ERR_RADIATION_CPM_HIGH = 1500,
    IOTDATA_ERR_RADIATION_DOSE_HIGH,

    IOTDATA_ERR_DEPTH_HIGH = 1600,

    IOTDATA_ERR_POSITION_LAT_LOW = 1700,
    IOTDATA_ERR_POSITION_LAT_HIGH,
    IOTDATA_ERR_POSITION_LON_LOW,
    IOTDATA_ERR_POSITION_LON_HIGH,

    IOTDATA_ERR_DATETIME_HIGH = 1800,
} iotdata_status_t;

typedef enum {
    IOTDATA_STATE_IDLE = 0,
    IOTDATA_STATE_BEGUN,
    IOTDATA_STATE_ENDED,
} iotdata_state_t;

/* ---------------------------------------------------------------------------
 * Encoder context
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_DECODE_ONLY)
#define IOTDATA_MAGIC 0xDA7AB175
typedef struct {
    uint32_t _magic;

    uint8_t *buf;
    size_t buf_size;
    iotdata_state_t state;
    uint32_t fields_present;

    uint8_t variant;
    uint16_t station_id;
    uint16_t sequence;

#ifdef IOTDATA_ENABLE_BATTERY
    uint8_t battery_level;
    bool battery_charging;
#endif

#ifdef IOTDATA_ENABLE_LINK
    int16_t link_rssi;
    iotdata_float_t link_snr;
#endif

#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_TEMPERATURE)
    iotdata_float_t temperature;
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_PRESSURE)
    uint16_t pressure;
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_HUMIDITY)
    uint8_t humidity;
#endif

#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_SPEED)
    iotdata_float_t wind_speed;
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_DIRECTION)
    uint16_t wind_direction;
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_GUST)
    iotdata_float_t wind_gust;
#endif

#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_RATE)
    uint8_t rain_rate;
#endif
#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_SIZE)
    uint8_t rain_size10;
#endif

#ifdef IOTDATA_ENABLE_SOLAR
    uint16_t solar_irradiance;
    uint8_t solar_ultraviolet;
#endif

#ifdef IOTDATA_ENABLE_CLOUDS
    uint8_t clouds;
#endif

#ifdef IOTDATA_ENABLE_AIR_QUALITY
    uint16_t air_quality;
#endif

#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_CPM)
    uint16_t radiation_cpm;
#endif
#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_DOSE)
    iotdata_float_t radiation_dose;
#endif

#ifdef IOTDATA_ENABLE_DEPTH
    uint16_t depth;
#endif

#ifdef IOTDATA_ENABLE_POSITION
    iotdata_double_t position_lat;
    iotdata_double_t position_lon;
#endif

#ifdef IOTDATA_ENABLE_DATETIME
    uint32_t datetime_ticks;
#endif

#ifdef IOTDATA_ENABLE_FLAGS
    uint8_t flags;
#endif

#ifdef IOTDATA_ENABLE_TLV
    uint8_t tlv_count;
    struct {
        uint8_t format;
        uint8_t type;
        uint8_t length;
        const uint8_t *data;
        const char *str;
    } tlv[IOTDATA_MAX_TLV];
#endif

    size_t packed_bits;
    size_t packed_bytes;
} iotdata_enc_ctx_t;
#endif /* !IOTDATA_DECODE_ONLY */

/* ---------------------------------------------------------------------------
 * Decoded packet (always has all fields — no element guards)
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_ENCODE_ONLY)
typedef struct {
    uint8_t variant;
    uint16_t station_id;
    uint16_t sequence;
    uint32_t fields_present;

    uint8_t battery_level;
    bool battery_charging;

    int16_t link_rssi;
    iotdata_float_t link_snr;

    iotdata_float_t temperature;
    uint16_t pressure;
    uint8_t humidity;

    iotdata_float_t wind_speed;
    uint16_t wind_direction;
    iotdata_float_t wind_gust;

    uint8_t rain_rate;
    uint8_t rain_size10;

    uint16_t solar_irradiance;
    uint8_t solar_ultraviolet;

    uint8_t clouds;

    uint16_t air_quality;

    uint16_t radiation_cpm;
    iotdata_float_t radiation_dose;

    uint16_t depth;

    iotdata_double_t position_lat;
    iotdata_double_t position_lon;

    uint32_t datetime_seconds;

    uint8_t flags;

    uint8_t tlv_count;
    struct {
        uint8_t format;
        uint8_t type;
        uint8_t length;
        uint8_t raw[256];
        char str[256];
    } tlv[IOTDATA_MAX_TLV];

    size_t total_bits;
    size_t total_bytes;
} iotdata_decoded_t;
#endif /* !IOTDATA_ENCODE_ONLY */

#if !defined(IOTDATA_NO_DUMP)
typedef struct {
    size_t bit_offset;
    size_t bit_length;
    char field_name[32];
    uint32_t raw_value;
    char decoded_str[64];
    char range_str[80];
} iotdata_dump_entry_t;

#define IOTDATA_MAX_DUMP_ENTRIES 64

typedef struct {
    iotdata_dump_entry_t entries[IOTDATA_MAX_DUMP_ENTRIES];
    size_t count;
    size_t total_bits;
    size_t total_bytes;
} iotdata_dump_t;
#endif /* !IOTDATA_DUMP_ONLY */

/* ---------------------------------------------------------------------------
 * Encoder
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_DECODE_ONLY)
iotdata_status_t iotdata_encode_begin(iotdata_enc_ctx_t *ctx, uint8_t *buf, size_t buf_size, uint8_t variant, uint16_t station_id, uint16_t sequence);
iotdata_status_t iotdata_encode_end(iotdata_enc_ctx_t *ctx, size_t *out_bytes);
#ifdef IOTDATA_ENABLE_TLV
iotdata_status_t iotdata_encode_tlv(iotdata_enc_ctx_t *ctx, uint8_t type, const uint8_t *data, uint8_t length);
iotdata_status_t iotdata_encode_tlv_string(iotdata_enc_ctx_t *ctx, uint8_t type, const char *str);
#endif
#ifdef IOTDATA_ENABLE_BATTERY
iotdata_status_t iotdata_encode_battery(iotdata_enc_ctx_t *ctx, uint8_t level_percent, bool charging);
#endif
#ifdef IOTDATA_ENABLE_LINK
iotdata_status_t iotdata_encode_link(iotdata_enc_ctx_t *ctx, int16_t rssi_dbm, iotdata_float_t snr_db);
#endif
#ifdef IOTDATA_ENABLE_ENVIRONMENT
iotdata_status_t iotdata_encode_environment(iotdata_enc_ctx_t *ctx, iotdata_float_t temp_c, uint16_t pressure_hpa, uint8_t humidity_pct);
#endif
#ifdef IOTDATA_ENABLE_TEMPERATURE
iotdata_status_t iotdata_encode_temperature(iotdata_enc_ctx_t *ctx, iotdata_float_t temperature_c);
#endif
#ifdef IOTDATA_ENABLE_PRESSURE
iotdata_status_t iotdata_encode_pressure(iotdata_enc_ctx_t *ctx, uint16_t pressure_hpa);
#endif
#ifdef IOTDATA_ENABLE_HUMIDITY
iotdata_status_t iotdata_encode_humidity(iotdata_enc_ctx_t *ctx, uint8_t humidity_pct);
#endif
#ifdef IOTDATA_ENABLE_WIND
iotdata_status_t iotdata_encode_wind(iotdata_enc_ctx_t *ctx, iotdata_float_t speed_ms, uint16_t direction_deg, iotdata_float_t gust_ms);
#endif
#ifdef IOTDATA_ENABLE_WIND_SPEED
iotdata_status_t iotdata_encode_wind_speed(iotdata_enc_ctx_t *ctx, iotdata_float_t speed_ms);
#endif
#ifdef IOTDATA_ENABLE_WIND_DIRECTION
iotdata_status_t iotdata_encode_wind_direction(iotdata_enc_ctx_t *ctx, uint16_t direction_deg);
#endif
#ifdef IOTDATA_ENABLE_WIND_GUST
iotdata_status_t iotdata_encode_wind_gust(iotdata_enc_ctx_t *ctx, iotdata_float_t gust_ms);
#endif
#ifdef IOTDATA_ENABLE_RAIN
iotdata_status_t iotdata_encode_rain(iotdata_enc_ctx_t *ctx, uint8_t rate_mmhr, uint8_t size_mmd);
#endif
#ifdef IOTDATA_ENABLE_RAIN_RATE
iotdata_status_t iotdata_encode_rain_rate(iotdata_enc_ctx_t *ctx, uint8_t rate_mmhr);
#endif
#ifdef IOTDATA_ENABLE_RAIN_SIZE
iotdata_status_t iotdata_encode_rain_size(iotdata_enc_ctx_t *ctx, uint8_t size_mmd);
#endif
#ifdef IOTDATA_ENABLE_SOLAR
iotdata_status_t iotdata_encode_solar(iotdata_enc_ctx_t *ctx, uint16_t irradiance_wm2, uint8_t ultraviolet_index);
#endif
#ifdef IOTDATA_ENABLE_CLOUDS
iotdata_status_t iotdata_encode_clouds(iotdata_enc_ctx_t *ctx, uint8_t okta);
#endif
#ifdef IOTDATA_ENABLE_AIR_QUALITY
iotdata_status_t iotdata_encode_air_quality(iotdata_enc_ctx_t *ctx, uint16_t aqi);
#endif
#ifdef IOTDATA_ENABLE_RADIATION
iotdata_status_t iotdata_encode_radiation(iotdata_enc_ctx_t *ctx, uint16_t cpm, iotdata_float_t usvh);
#endif
#ifdef IOTDATA_ENABLE_RADIATION_CPM
iotdata_status_t iotdata_encode_radiation_cpm(iotdata_enc_ctx_t *ctx, uint16_t cpm);
#endif
#ifdef IOTDATA_ENABLE_RADIATION_DOSE
iotdata_status_t iotdata_encode_radiation_dose(iotdata_enc_ctx_t *ctx, iotdata_float_t usvh);
#endif
#ifdef IOTDATA_ENABLE_DEPTH
iotdata_status_t iotdata_encode_depth(iotdata_enc_ctx_t *ctx, uint16_t depth_cm);
#endif
#ifdef IOTDATA_ENABLE_POSITION
iotdata_status_t iotdata_encode_position(iotdata_enc_ctx_t *ctx, iotdata_double_t latitude, iotdata_double_t longitude);
#endif
#ifdef IOTDATA_ENABLE_DATETIME
iotdata_status_t iotdata_encode_datetime(iotdata_enc_ctx_t *ctx, uint32_t seconds_from_year_start);
#endif
#ifdef IOTDATA_ENABLE_FLAGS
iotdata_status_t iotdata_encode_flags(iotdata_enc_ctx_t *ctx, uint8_t flags);
#endif
#endif /* !IOTDATA_DECODE_ONLY */

/* ---------------------------------------------------------------------------
 * Decoder
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_ENCODE_ONLY)
iotdata_status_t iotdata_decode(const uint8_t *buf, size_t len, iotdata_decoded_t *out);
#endif /* !IOTDATA_ENCODE_ONLY */

/* ---------------------------------------------------------------------------
 * JSON, Print, Dump
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DUMP)
iotdata_status_t iotdata_dump(const uint8_t *buf, size_t len, FILE *fp);
iotdata_status_t iotdata_dump_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size);
iotdata_status_t iotdata_dump_build(const uint8_t *buf, size_t len, iotdata_dump_t *dump);
#endif /* !IOTDATA_NO_DUMP */

#if !defined(IOTDATA_NO_PRINT)
iotdata_status_t iotdata_print(const uint8_t *buf, size_t len, FILE *fp);
iotdata_status_t iotdata_print_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size);
#endif /* !IOTDATA_NO_PRINT */

#if !defined(IOTDATA_NO_JSON)
#if !defined(IOTDATA_ENCODE_ONLY)
iotdata_status_t iotdata_decode_to_json(const uint8_t *buf, size_t len, char **json_out);
#endif /* !IOTDATA_ENCODE_ONLY */
#if !defined(IOTDATA_DECODE_ONLY)
iotdata_status_t iotdata_encode_from_json(const char *json, uint8_t *buf, size_t buf_size, size_t *out_bytes);
#endif /* !IOTDATA_DECODE_ONLY */
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
