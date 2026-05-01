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
 *   IOTDATA_NO_TLV_SPECIFIC        Exclude TLV specific type handling
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

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Types
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
 * Fields
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
#define IOTDATA_ENABLE_AIR_QUALITY_INDEX
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
#define IOTDATA_ENABLE_AIR_QUALITY_INDEX
#define IOTDATA_ENABLE_AIR_QUALITY_PM
#define IOTDATA_ENABLE_AIR_QUALITY_GAS
#define IOTDATA_ENABLE_RADIATION
#define IOTDATA_ENABLE_RADIATION_CPM
#define IOTDATA_ENABLE_RADIATION_DOSE
#define IOTDATA_ENABLE_DEPTH
#define IOTDATA_ENABLE_POSITION
#define IOTDATA_ENABLE_DATETIME
#define IOTDATA_ENABLE_FLAGS
#define IOTDATA_ENABLE_BITS32
#define IOTDATA_ENABLE_IMAGE
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
enum iotdata_status_t_;
struct iotdata_encoder_t_;
struct iotdata_decoder_t_;
#include "iotdata_fields.h"
#pragma GCC diagnostic pop

/* ---------------------------------------------------------------------------
 * Header: variant(4) + station(12) + sequence(16) = 32 bits
 * -------------------------------------------------------------------------*/

#define IOTDATA_VARIANT_BITS      4
#define IOTDATA_STATION_BITS      12
#define IOTDATA_SEQUENCE_BITS     16
#define IOTDATA_HEADER_BITS       (IOTDATA_VARIANT_BITS + IOTDATA_STATION_BITS + IOTDATA_SEQUENCE_BITS)

#define IOTDATA_VARIANT_MAX       14
#define IOTDATA_VARIANT_RESERVED  15
#define IOTDATA_STATION_MAX       4095
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
 *
 * This implementation limits to a total of 4 presence bytes, i.e. 48 slots,
 * but that is not a limitation in the protocol.
 * -------------------------------------------------------------------------*/

#define IOTDATA_PRES_TLV          (1U << 6)
#define IOTDATA_PRES_EXT          (1U << 7)
#define IOTDATA_PRES0_DATA_FIELDS 6
#define IOTDATA_PRESN_DATA_FIELDS 7
#define IOTDATA_PRES_MINIMUM      1
#define IOTDATA_PRES_MAXIMUM      4
#define IOTDATA_MAX_DATA_FIELDS   (IOTDATA_PRES0_DATA_FIELDS + IOTDATA_PRESN_DATA_FIELDS * (IOTDATA_PRES_MAXIMUM - 1))

/* ---------------------------------------------------------------------------
 * Packet
 * -------------------------------------------------------------------------*/

#define IOTDATA_PACKET_MINIMUM    ((IOTDATA_HEADER_BITS / 8) + IOTDATA_PRES_MINIMUM)

/* ---------------------------------------------------------------------------
 * Field types
 * -------------------------------------------------------------------------*/

typedef enum {
    IOTDATA_FIELD_NONE = -1,
#define IOTDATA_FIELDS_SELECTED_VISITOR_(name) IOTDATA_FIELD_##name,
    IOTDATA_FIELDS_SELECTED(IOTDATA_FIELDS_SELECTED_VISITOR_)
#undef IOTDATA_FIELDS_SELECTED_VISITOR_
        IOTDATA_FIELD_COUNT,
#define IOTDATA_FIELDS_RESERVED_VISITOR_(name, value) IOTDATA_FIELD_##name = value,
    IOTDATA_FIELDS_RESERVED(IOTDATA_FIELDS_RESERVED_VISITOR_)
#undef IOTDATA_FIELDS_RESERVED_VISITOR_
} iotdata_field_type_t;

_Static_assert(IOTDATA_FIELD_COUNT <= 32, "fields overflow");

#define IOTDATA_FIELD_EMPTY             (0)
#define IOTDATA_FIELD_BIT(id)           (1U << (id))
#define IOTDATA_FIELD_PRESENT(mask, id) (((mask) >> (id)) & 1U)
#define IOTDATA_FIELD_SET(mask, id)     ((mask) |= (1U << (id)))
#define IOTDATA_FIELD_CLR(mask, id)     ((mask) &= ~(1U << (id)))
#define IOTDATA_FIELD_VALID(id)         ((id) != IOTDATA_FIELD_NONE && !IOTDATA_FIELD_RESERVED(id))

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

typedef enum iotdata_status_t_ {
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
    IOTDATA_ERR_BUF_NULL,
#endif
#if !defined(IOTDATA_NO_DECODE)
    IOTDATA_ERR_DECODE_SHORT,
    IOTDATA_ERR_DECODE_TRUNCATED,
    IOTDATA_ERR_DECODE_VARIANT,
#elif !defined(IOTDATA_NO_DUMP)
    IOTDATA_ERR_DECODE_SHORT,
    IOTDATA_ERR_DECODE_TRUNCATED,
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
#define IOTDATA_FIELDS_ERR_IDENTS_VISITOR_(name) name,
    IOTDATA_FIELDS_ERR_IDENTS(IOTDATA_FIELDS_ERR_IDENTS_VISITOR_)
#undef IOTDATA_FIELDS_ERR_IDENTS_VISITOR_
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
typedef struct iotdata_encoder_t_ {
    uint8_t *buf;
    size_t buf_size;
    iotdata_state_t state;

    size_t packed_bits;
    size_t packed_bytes;

    uint8_t variant;
    uint16_t station;
    uint16_t sequence;
    iotdata_field_t fields;

    IOTDATA_FIELDS_DAT_ENCODER

} iotdata_encoder_t;
#endif /* !IOTDATA_NO_ENCODE */

/* ---------------------------------------------------------------------------
 * Decoded content
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DECODE)
typedef struct iotdata_decoder_t_ {
    size_t packed_bits;
    size_t packed_bytes;

    uint8_t variant;
    uint16_t station;
    uint16_t sequence;
    iotdata_field_t fields;

    IOTDATA_FIELDS_DAT_DECODER

} iotdata_decoder_t;
#endif /* !IOTDATA_NO_DECODE */

/* ---------------------------------------------------------------------------
 * Encode/Decode
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_begin(iotdata_encoder_t *enc, uint8_t *buf, size_t buf_size, uint8_t variant, uint16_t station, uint16_t sequence);
iotdata_status_t iotdata_encode_end(iotdata_encoder_t *enc, size_t *out_bytes);
#endif /* !IOTDATA_NO_ENCODE */

#if !defined(IOTDATA_NO_DECODE)
iotdata_status_t iotdata_peek(const uint8_t *buf, size_t len, uint8_t *variant, uint16_t *station, uint16_t *sequence);
iotdata_status_t iotdata_decode(const uint8_t *buf, size_t len, iotdata_decoder_t *out);
#endif /* !IOTDATA_NO_DECODE */

/* ---------------------------------------------------------------------------
 * Dump, Print (requires decoder), JSON (requires encoder/decoder)
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_DUMP)

#define IOTDATA_DUMP_RANGE_STR_STATIC

#define IOTDATA_DUMP_FIELD_NAME_MAX  32
#define IOTDATA_DUMP_DECODED_STR_MAX 32
#define IOTDATA_DUMP_RANGE_STR_MAX   32

typedef struct {
    size_t bit_offset;
    size_t bit_length;
    char field_name[IOTDATA_DUMP_FIELD_NAME_MAX];
    uint32_t raw_value;
    char decoded_str[IOTDATA_DUMP_DECODED_STR_MAX];
#ifdef IOTDATA_DUMP_RANGE_STR_STATIC
    const char *range_str;
#else
    char range_str[IOTDATA_DUMP_RANGE_STR_MAX];
#endif
} iotdata_dump_entry_t;

#define IOTDATA_MAX_DUMP_ENTRIES 48

typedef struct {
    char _name_buf[IOTDATA_DUMP_FIELD_NAME_MAX];
    char _dec_buf[IOTDATA_DUMP_DECODED_STR_MAX];
    iotdata_dump_entry_t entries[IOTDATA_MAX_DUMP_ENTRIES];
    size_t count;
    size_t packed_bits;
    size_t packed_bytes;
} iotdata_dump_t;

iotdata_status_t iotdata_dump_to_string(iotdata_dump_t *dump, const uint8_t *buf, size_t len, char *out, size_t out_size, bool verbose);
#endif /* !IOTDATA_NO_DUMP */

#if !defined(IOTDATA_NO_PRINT)
#if !defined(IOTDATA_NO_DECODE)
iotdata_status_t iotdata_print_decoder_to_string(const iotdata_decoder_t *dec, char *out, size_t out_size);
typedef struct {
    iotdata_decoder_t dec;
} iotdata_print_scratch_t;
iotdata_status_t iotdata_print_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size, iotdata_print_scratch_t *scratch);
#endif
#endif /* !IOTDATA_NO_PRINT */

#if !defined(IOTDATA_NO_JSON)
#if !defined(IOTDATA_NO_DECODE)
typedef struct {
    iotdata_decoder_t dec;
    union {
        bool _dummy;
        IOTDATA_FIELDS_DECODE_TO_JSON_SCRATCH
    };
} iotdata_decode_to_json_scratch_t;
iotdata_status_t iotdata_decode_to_json(const uint8_t *buf, size_t len, char **json_out, iotdata_decode_to_json_scratch_t *scratch);
#endif /* !IOTDATA_NO_DECODE */
#if !defined(IOTDATA_NO_ENCODE)
typedef struct {
    iotdata_encoder_t enc;
    union {
        bool _dummy;
        IOTDATA_FIELDS_ENCODE_FROM_JSON_SCRATCH
    };
} iotdata_encode_from_json_scratch_t;
iotdata_status_t iotdata_encode_from_json(const char *json, uint8_t *buf, size_t buf_size, size_t *out_bytes, iotdata_encode_from_json_scratch_t *scratch);
#endif /* !IOTDATA_NO_ENCODE */
#endif /* !IOTDATA_NO_JSON */

/* ---------------------------------------------------------------------------
 * Ancillary
 * -------------------------------------------------------------------------*/

#if !defined(IOTDATA_NO_ERROR_STRINGS)
const char *iotdata_strerror(iotdata_status_t status);
#endif

#ifdef __cplusplus
}
#endif

#endif /* IOTDATA_H */
