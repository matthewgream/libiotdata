/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * iotdata.c - reference implementation body
 *
 * Architecture:
 *   1. Per-field functions (pack, unpack, json_set, json_get, dump, print)
 *   2. Field dispatcher switches on field type, calls per-field functions
 *   3. Variant table maps field presence bit fields to field types
 *   4. Encoder/decoder iterate fields via variant table, supporting N presence bytes
 *
 * Per-field functions are static and guarded by IOTDATA_ENABLE_XYZ
 * defines to allow compile-time exclusion on constrained targets.
 */

#include "iotdata.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#if !defined(IOTDATA_NO_FLOATING)
#include <math.h>
#endif

#if !defined(IOTDATA_NO_JSON)
#include <cjson/cJSON.h>
#endif

/* =========================================================================
 * External Variant maps
 * ========================================================================= */

#if defined(IOTDATA_VARIANT_MAPS) && defined(IOTDATA_VARIANT_MAPS_COUNT)

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
extern const iotdata_variant_def_t IOTDATA_VARIANT_MAPS[];
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

const iotdata_variant_def_t *iotdata_get_variant(uint8_t variant) {
    if (variant < IOTDATA_VARIANT_MAPS_COUNT)
        return &IOTDATA_VARIANT_MAPS[variant];
    return NULL;
}

#elif !defined(IOTDATA_ENABLE_SELECTIVE)

#define IOTDATA_VARIANT_MAPS_DEFAULT_COUNT 1

static const iotdata_variant_def_t IOTDATA_DEFAULT_VARIANTS[IOTDATA_VARIANT_MAPS_DEFAULT_COUNT] = {
    /* Variant 0: weather station */
    [0] = {
        .name = "weather_station",
        .num_pres_bytes = 2,
        .fields = {
            /* --- pres0 (6 fields) --- */
            { IOTDATA_FIELD_BATTERY,         "battery" },
            { IOTDATA_FIELD_LINK,            "link" },
            { IOTDATA_FIELD_ENVIRONMENT,     "environment" },
            { IOTDATA_FIELD_WIND,            "wind" },
            { IOTDATA_FIELD_RAIN,            "rain" },
            { IOTDATA_FIELD_SOLAR,           "solar" },
            /* --- pres1 (6 fields) --- */
            { IOTDATA_FIELD_CLOUDS,          "clouds" },
            { IOTDATA_FIELD_AIR_QUALITY_INDEX,     "air_quality" },
            { IOTDATA_FIELD_RADIATION,       "radiation" },
            { IOTDATA_FIELD_POSITION,        "position" },
            { IOTDATA_FIELD_DATETIME,        "datetime" },
            { IOTDATA_FIELD_FLAGS,           "flags" },
            { IOTDATA_FIELD_NONE,            NULL },
        },
    },
};

const iotdata_variant_def_t *iotdata_get_variant(uint8_t variant) {
    if (variant < IOTDATA_VARIANT_MAPS_DEFAULT_COUNT)
        return &IOTDATA_DEFAULT_VARIANTS[variant];
    return NULL;
}

#endif /* IOTDATA_VARIANT_MAPS */

/* =========================================================================
 * Internal field operations table
 * ========================================================================= */

#define _IOTDATA_FIELD_OP_NAME // const char *name;
#define _IOTDATA_OP_NAME(nm)   // unused
#define _IOTDATA_FIELD_OP_BITS // uint16_t bits;
#define _IOTDATA_OP_BITS(bt)   // unused

#if !defined(IOTDATA_NO_ENCODE)
typedef bool (*iotdata_pack_fn)(uint8_t *buf, size_t bb, size_t *bp, const iotdata_encoder_t *enc);
#define _IOTDATA_FIELD_OP_PACK iotdata_pack_fn pack;
#define _IOTDATA_OP_PACK(fn)   .pack = (fn),
#else
#define _IOTDATA_FIELD_OP_PACK
#define _IOTDATA_OP_PACK(fn)
#endif

#if !defined(IOTDATA_NO_DECODE)
typedef bool (*iotdata_unpack_fn)(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *dec);
#define _IOTDATA_FIELD_OP_UNPACK iotdata_unpack_fn unpack;
#define _IOTDATA_OP_UNPACK(fn)   .unpack = (fn),
#else
#define _IOTDATA_FIELD_OP_UNPACK
#define _IOTDATA_OP_UNPACK(fn)
#endif

#if !defined(IOTDATA_NO_DUMP)
typedef int (*iotdata_dump_fn)(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label);
#define _IOTDATA_FIELD_OP_DUMP iotdata_dump_fn dump;
#define _IOTDATA_OP_DUMP(fn)   .dump = (fn),
#else
#define _IOTDATA_FIELD_OP_DUMP
#define _IOTDATA_OP_DUMP(fn)
#endif

#if !defined(IOTDATA_NO_PRINT) || !defined(IOTDATA_NO_DUMP)
typedef struct {
    char *buf;
    size_t size;
    size_t pos;
} iotdata_buf_t;
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static int
bprintf(iotdata_buf_t *b, const char *fmt, ...) {
    if (b->pos >= b->size)
        return 0;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(b->buf + b->pos, b->size - b->pos, fmt, ap);
    va_end(ap);
    if (n > 0)
        b->pos += (size_t)n < (b->size - b->pos) ? (size_t)n : (b->size - b->pos);
    return n;
}
#endif

#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
typedef void (*iotdata_print_fn)(const iotdata_decoded_t *dec, iotdata_buf_t *bp, const char *label);
#define _IOTDATA_FIELD_OP_PRINT iotdata_print_fn print;
#define _IOTDATA_OP_PRINT(fn)   .print = (fn),
#else
#define _IOTDATA_FIELD_OP_PRINT
#define _IOTDATA_OP_PRINT(fn)
#endif

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
typedef void (*iotdata_json_set_fn)(cJSON *root, const iotdata_decoded_t *dec, const char *label, iotdata_decode_to_json_scratch_t *scratch);
#define _IOTDATA_FIELD_OP_JSON_SET iotdata_json_set_fn json_set;
#define _IOTDATA_OP_JSON_SET(fn)   .json_set = (iotdata_json_set_fn)(fn),
#else
#define _IOTDATA_FIELD_OP_JSON_SET
#define _IOTDATA_OP_JSON_SET(fn)
#endif

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
typedef iotdata_status_t (*iotdata_json_get_fn)(cJSON *root, const iotdata_encoder_t *enc, const char *label, iotdata_encode_from_json_scratch_t *scratch);
#define _IOTDATA_FIELD_OP_JSON_GET iotdata_json_get_fn json_get;
#define _IOTDATA_OP_JSON_GET(fn)   .json_get = (iotdata_json_get_fn)(fn),
#else
#define _IOTDATA_FIELD_OP_JSON_GET
#define _IOTDATA_OP_JSON_GET(fn)
#endif

typedef struct {
    _IOTDATA_FIELD_OP_NAME
    _IOTDATA_FIELD_OP_BITS
    _IOTDATA_FIELD_OP_PACK
    _IOTDATA_FIELD_OP_UNPACK
    _IOTDATA_FIELD_OP_DUMP
    _IOTDATA_FIELD_OP_PRINT
    _IOTDATA_FIELD_OP_JSON_SET
    _IOTDATA_FIELD_OP_JSON_GET
} iotdata_field_ops_t;

/* =========================================================================
 * Internal bit-packing (MSB-first / big-endian order)
 * ========================================================================= */

#if !defined(IOTDATA_NO_ENCODE) || !defined(IOTDATA_NO_DECODE) || !defined(IOTDATA_NO_DUMP)
static size_t bits_to_bytes(size_t bits) {
    return (bits + 7) / 8;
}
#endif

#if !defined(IOTDATA_NO_ENCODE)
static bool bits_write(uint8_t *buf, size_t buf_bits, size_t *bp, uint32_t value, uint8_t nbits) {
    if (*bp + nbits > buf_bits)
        return false;
    size_t pos = *bp;
    int rem = nbits, off = pos & 7;
    if (off) {
        const int n = rem < (8 - off) ? rem : (8 - off);
        buf[pos >> 3] = (buf[pos >> 3] & ~(uint8_t)(((1U << n) - 1) << ((8 - off) - n))) | (uint8_t)(((value >> (rem - n)) & ((1U << n) - 1)) << ((8 - off) - n));
        pos += (size_t)n;
        rem -= n;
    }
    while (rem >= 8) {
        rem -= 8;
        buf[pos >> 3] = (uint8_t)(value >> rem);
        pos += 8;
    }
    if (rem > 0) {
        buf[pos >> 3] = (buf[pos >> 3] & ~(uint8_t)(((1U << rem) - 1) << (8 - rem))) | (uint8_t)((value & ((1U << rem) - 1)) << (8 - rem));
        pos += (size_t)rem;
    }
    *bp = pos;
    return true;
}
#endif

#if !defined(IOTDATA_NO_DECODE) || !defined(IOTDATA_NO_DUMP)
static uint32_t bits_read(const uint8_t *buf, size_t buf_bits, size_t *bp, uint8_t nbits) {
    if (*bp + nbits > buf_bits) {
        uint32_t value = 0;
        for (int i = nbits - 1; i >= 0 && *bp < buf_bits; i--, (*bp)++)
            value |= ((uint32_t)((buf[*bp / 8] >> (7 - (*bp % 8))) & 1U) << i);
        return value;
    }
    uint32_t value = 0;
    size_t pos = *bp;
    int rem = nbits, off = pos & 7;
    if (off) {
        const int n = rem < (8 - off) ? rem : (8 - off);
        value = (buf[pos >> 3] >> ((8 - off) - n)) & ((1U << n) - 1);
        pos += (size_t)n;
        rem -= n;
    }
    while (rem >= 8) {
        value = (value << 8) | buf[pos >> 3];
        pos += 8;
        rem -= 8;
    }
    if (rem > 0) {
        value = (value << rem) | ((buf[pos >> 3] >> (8 - rem)) & ((1U << rem) - 1));
        pos += (size_t)rem;
    }
    *bp = pos;
    return value;
}
#endif

/* =========================================================================
 * Utilities
 * ========================================================================= */

#if defined(_IOTDATA_NEED_BASE64_ENCODE)
static const char _b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char *_b64_encode(const uint8_t *in, size_t in_len, char *out) {
    size_t i = 0, j = 0;
    while (i < in_len) {
        const uint32_t a = in[i++], b = (i < in_len) ? in[i++] : ~0U, c = (i < in_len) ? in[i++] : ~0U;
        const uint32_t x = (a << 16) | ((b & 0xFF) << 8) | (c & 0xFF);
        out[j++] = _b64_table[(x >> 18) & 0x3F];
        out[j++] = _b64_table[(x >> 12) & 0x3F];
        out[j++] = (b >> 8) ? '=' : _b64_table[(x >> 6) & 0x3F];
        out[j++] = (c >> 8) ? '=' : _b64_table[(x >> 0) & 0x3F];
    }
    out[j] = '\0';
    return out;
}
#endif

#if defined(_IOTDATA_NEED_BASE64_DECODE)
static int _b64_val(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    else if (c >= '0' && c <= '9')
        return c - '0' + 52;
    else if (c == '+')
        return 62;
    else if (c == '/')
        return 63;
    else
        return -1;
}
static size_t _b64_decode(const char *in, uint8_t *out, size_t out_max) {
    const size_t ilen = strlen(in);
    size_t op = 0;
    for (size_t i = 0; i + 3 < ilen && op < out_max; i += 4) {
        const int a = _b64_val(in[i + 0]), b = _b64_val(in[i + 1]);
        if (a < 0 || b < 0)
            break;
        out[op++] = (uint8_t)((a << 2) | (b >> 4));
        const int c = _b64_val(in[i + 2]), d = _b64_val(in[i + 3]);
        if (c >= 0 && op < out_max)
            out[op++] = (uint8_t)(((b & 0x0F) << 4) | (c >> 2));
        if (d >= 0 && op < out_max)
            out[op++] = (uint8_t)(((c & 0x03) << 6) | d);
    }
    return op;
}
#endif

#if defined(_IOTDATA_NEED_SIXBIT_ENCODE)
static int char_to_sixbit(char c) {
    if (c == ' ')
        return 0;
    else if (c >= 'a' && c <= 'z')
        return 1 + (c - 'a');
    else if (c >= '0' && c <= '9')
        return 27 + (c - '0');
    else if (c >= 'A' && c <= 'Z')
        return 37 + (c - 'A');
    else
        return -1;
}
#endif

#if defined(_IOTDATA_NEED_SIXBIT_DECODE)
static char sixbit_to_char(uint8_t val) {
    if (val == 0)
        return ' ';
    else if (val >= 1 && val <= 26)
        return 'a' + (char)(val - 1);
    else if (val >= 27 && val <= 36)
        return '0' + (char)(val - 27);
    else if (val >= 37 && val <= 62)
        return 'A' + (char)(val - 37);
    else
        return '?';
}
#endif

/* =========================================================================
 * Internal
 * ========================================================================= */

#if !defined(IOTDATA_NO_ENCODE)

#if !defined(IOTDATA_NO_CHECKS_STATE)
#define CHECK_CTX_ACTIVE(enc) \
    do { \
        if (!(enc)) \
            return IOTDATA_ERR_CTX_NULL; \
        if ((enc)->state == IOTDATA_STATE_ENDED) \
            return IOTDATA_ERR_CTX_ALREADY_ENDED; \
        if ((enc)->state != IOTDATA_STATE_BEGUN) \
            return IOTDATA_ERR_CTX_NOT_BEGUN; \
    } while (0)
#else
#define CHECK_CTX_ACTIVE(enc)
#endif

#if !defined(IOTDATA_NO_CHECKS_STATE)
#define CHECK_NOT_DUPLICATE(enc, field_index) \
    do { \
        if (IOTDATA_FIELD_PRESENT((enc)->fields, field_index)) \
            return IOTDATA_ERR_CTX_DUPLICATE_FIELD; \
    } while (0)
#else
#define CHECK_NOT_DUPLICATE(enc, field_index)
#endif

#endif

#if !defined(IOTDATA_NO_DUMP)
static int dump_add(iotdata_dump_t *dump, int n, size_t bit_offset, size_t bit_length, uint32_t raw_value, const char *decoded, const char *range, const char *name);
#endif

#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static const char _padd_spaces[] = "                        "; /* 24 spaces */
static const char *_padd(const char *label) {
    const int n = (int)sizeof(_padd_spaces) - (int)strlen(label);
    return &_padd_spaces[(int)sizeof(_padd_spaces) - (n > 0 ? n : 1)];
}
#endif

#if !defined(IOTDATA_NO_DUMP)
#if defined(IOTDATA_NO_FLOATING)
static int fmt_scaled(char *buf, size_t sz, int32_t val, int32_t divisor, const char *unit) {
    const uint32_t a = (val < 0) ? -(uint32_t)val : (uint32_t)val;
    return snprintf(buf, sz, "%s%" PRIu32 ".%01" PRIu32 "%s%s", val < 0 ? "-" : "", a / (uint32_t)divisor, a % (uint32_t)divisor, unit[0] ? " " : "", unit);
}
#endif
#endif

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE) && !defined(IOTDATA_NO_FLOATING)
// cJSON prints doubles with %1.15g/%1.17g, so rounding the double isn't enough.
// Format at the field's quantisation precision and insert as a raw JSON number.
static void json_add_number_fixed(cJSON *obj, const char *name, double v, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    cJSON_AddRawToObject(obj, name, buf);
}
#endif

/* =========================================================================
 * Fields
 * ========================================================================= */

#include "iotdata_fields.c"

/* =========================================================================
 * Internal field operations table
 * ========================================================================= */

// clang-format off

#define _IOTDATA_FIELD_OPS_VISIT(NAME, DEF) [IOTDATA_FIELD_##NAME] = &DEF,
static const iotdata_field_ops_t *_iotdata_field_ops[IOTDATA_FIELD_COUNT] = {
    IOTDATA_VISIT_FIELD_OPS(_IOTDATA_FIELD_OPS_VISIT)
};
#undef _IOTDATA_FIELD_OPS_VISIT

// clang-format on

/* =========================================================================
 * Internal header
 * ========================================================================= */

static int _iotdata_field_count(int num_pres_bytes) {
    if (num_pres_bytes <= 0)
        return 0;
    return IOTDATA_PRES0_DATA_FIELDS + IOTDATA_PRESN_DATA_FIELDS * (num_pres_bytes - 1);
}

static int _iotdata_field_pres_byte(int field_idx) {
    if (field_idx < IOTDATA_PRES0_DATA_FIELDS)
        return 0;
    return 1 + (field_idx - IOTDATA_PRES0_DATA_FIELDS) / IOTDATA_PRESN_DATA_FIELDS;
}

static int _iotdata_field_pres_bit(int field_idx) {
    if (field_idx < IOTDATA_PRES0_DATA_FIELDS)
        return 5 - field_idx;                                                       /* pres0: bits 5..0 */
    return 6 - (field_idx - IOTDATA_PRES0_DATA_FIELDS) % IOTDATA_PRESN_DATA_FIELDS; /* presN: bits 6..0 */
}

#define _IOTDATA_ERR_HDR \
    case IOTDATA_ERR_HDR_VARIANT_HIGH: \
        return "Variant above maximum (14)"; \
    case IOTDATA_ERR_HDR_VARIANT_RESERVED: \
        return "Variant 15 is reserved"; \
    case IOTDATA_ERR_HDR_VARIANT_UNKNOWN: \
        return "Variant unknown"; \
    case IOTDATA_ERR_HDR_STATION_HIGH: \
        return "Station ID above maximum (4095)";

/* =========================================================================
 * External ENCODER
 * ========================================================================= */

#if !defined(IOTDATA_NO_ENCODE)

static bool _iotdata_encode_pack_field(uint8_t *buf, size_t bb, size_t *bp, const iotdata_encoder_t *enc, iotdata_field_type_t type) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->pack)
        return ops->pack(buf, bb, bp, enc);
    return true;
}

iotdata_status_t iotdata_encode_begin(iotdata_encoder_t *enc, uint8_t *buf, size_t buf_size, uint8_t variant, uint16_t station, uint16_t sequence) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!enc)
        return IOTDATA_ERR_CTX_NULL;
    if (!buf)
        return IOTDATA_ERR_BUF_NULL;
    if (buf_size < IOTDATA_HEADER_BITS / 8 + 1)
        return IOTDATA_ERR_BUF_TOO_SMALL;
#endif
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (variant > IOTDATA_VARIANT_MAX) {
        if (variant == IOTDATA_VARIANT_RESERVED)
            return IOTDATA_ERR_HDR_VARIANT_RESERVED;
        return IOTDATA_ERR_HDR_VARIANT_HIGH;
    }
    if (station > IOTDATA_STATION_MAX)
        return IOTDATA_ERR_HDR_STATION_HIGH;
#endif

    enc->buf = buf;
    enc->buf_size = buf_size;
    enc->variant = variant;
    enc->station = station;
    enc->sequence = sequence;
    enc->state = IOTDATA_STATE_BEGUN;
    enc->fields = IOTDATA_FIELD_EMPTY;
    iotdata_encode_tlv_begin(enc);
    return IOTDATA_OK;
}

iotdata_status_t iotdata_encode_end(iotdata_encoder_t *enc, size_t *out_bytes) {
    CHECK_CTX_ACTIVE(enc);

    const iotdata_variant_def_t *vdef = iotdata_get_variant(enc->variant);
    if (vdef == NULL)
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;
    size_t bb = enc->buf_size * 8, bp = 0;

    /* Header */
    if (!bits_write(enc->buf, bb, &bp, enc->variant, IOTDATA_VARIANT_BITS) || !bits_write(enc->buf, bb, &bp, enc->station, IOTDATA_STATION_BITS) || !bits_write(enc->buf, bb, &bp, enc->sequence, IOTDATA_SEQUENCE_BITS))
        return IOTDATA_ERR_BUF_TOO_SMALL;

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    int max_pres_needed = 1; /* always have pres0 */
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(enc->fields, vdef->fields[si].type)) {
            const int pb = _iotdata_field_pres_byte(si);
            pres[pb] |= (1U << _iotdata_field_pres_bit(si));
            if (pb >= max_pres_needed)
                max_pres_needed = pb + 1;
        }
    iotdata_encode_tlv_pres(enc, pres);
    for (int i = 0; i < max_pres_needed; i++)
        if (!bits_write(enc->buf, bb, &bp, pres[i] | (i < (max_pres_needed - 1) ? IOTDATA_PRES_EXT : 0), 8))
            return IOTDATA_ERR_BUF_TOO_SMALL;

    /* Fields + TLV */
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type)) {
            const int pb = _iotdata_field_pres_byte(si);
            if (pb < max_pres_needed && pres[pb] & (1U << _iotdata_field_pres_bit(si)))
                if (!_iotdata_encode_pack_field(enc->buf, bb, &bp, enc, vdef->fields[si].type))
                    return IOTDATA_ERR_BUF_TOO_SMALL;
        }
    if (!iotdata_encode_tlv_data(enc->buf, bb, &bp, enc))
        return IOTDATA_ERR_BUF_TOO_SMALL;

    enc->packed_bits = bp;
    enc->packed_bytes = bits_to_bytes(bp);
    enc->state = IOTDATA_STATE_ENDED;
    if (out_bytes)
        *out_bytes = enc->packed_bytes;
    return IOTDATA_OK;
}

#endif /* !IOTDATA_NO_ENCODE */

#if !defined(IOTDATA_NO_ENCODE)
#define _IOTDATA_ERR_ENCODE \
    case IOTDATA_ERR_CTX_NULL: \
        return "Encoding context pointer is NULL"; \
    case IOTDATA_ERR_CTX_NOT_BEGUN: \
        return "Encoding not started (call encode_begin first)"; \
    case IOTDATA_ERR_CTX_ALREADY_BEGUN: \
        return "Encoding already started"; \
    case IOTDATA_ERR_CTX_ALREADY_ENDED: \
        return "Encoding already ended"; \
    case IOTDATA_ERR_CTX_DUPLICATE_FIELD: \
        return "Encoding field already added"; \
    case IOTDATA_ERR_BUF_NULL: \
        return "Buffer pointer is NULL"; \
    case IOTDATA_ERR_BUF_OVERFLOW: \
        return "Buffer overflow during packing"; \
    case IOTDATA_ERR_BUF_TOO_SMALL: \
        return "Buffer too small for minimum packet";
#elif !defined(IOTDATA_NO_DUMP)
#define _IOTDATA_ERR_ENCODE \
    case IOTDATA_ERR_CTX_NULL: \
        return "Encoding context pointer is NULL"; \
    case IOTDATA_ERR_BUF_NULL: \
        return "Buffer pointer is NULL";
#else
#define _IOTDATA_ERR_ENCODE
#endif

/* =========================================================================
 * External DECODER
 * ========================================================================= */

#if !defined(IOTDATA_NO_DECODE)

static bool _iotdata_decode_unpack_field(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out, iotdata_field_type_t type) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->unpack)
        return ops->unpack(buf, bb, bp, out);
    return true;
}

iotdata_status_t iotdata_peek(const uint8_t *buf, size_t len, uint8_t *variant, uint16_t *station, uint16_t *sequence) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < IOTDATA_HEADER_BITS / 8 + 1)
        return IOTDATA_ERR_DECODE_SHORT;

    size_t bb = len * 8, bp = 0;

    const uint8_t h_variant = (uint8_t)bits_read(buf, bb, &bp, IOTDATA_VARIANT_BITS);
    const uint16_t h_station = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    const uint16_t h_sequence = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);
    if (variant) {
        *variant = h_variant;
        if (*variant == IOTDATA_VARIANT_RESERVED)
            return IOTDATA_ERR_DECODE_VARIANT;
    }
    if (station)
        *station = h_station;
    if (sequence)
        *sequence = h_sequence;

    return IOTDATA_OK;
}

iotdata_status_t iotdata_decode(const uint8_t *buf, size_t len, iotdata_decoded_t *dec) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf || !dec)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < IOTDATA_HEADER_BITS / 8 + 1)
        return IOTDATA_ERR_DECODE_SHORT;

    size_t bb = len * 8, bp = 0;

    /* Header */
    dec->variant = (uint8_t)bits_read(buf, bb, &bp, IOTDATA_VARIANT_BITS);
    dec->station = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    dec->sequence = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);
    if (dec->variant == IOTDATA_VARIANT_RESERVED)
        return IOTDATA_ERR_DECODE_VARIANT;

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    pres[0] = (uint8_t)bits_read(buf, bb, &bp, 8);
    int num_pres = 1;
    while (num_pres < IOTDATA_PRES_MAXIMUM && bp + 8 <= bb && (pres[num_pres - 1] & IOTDATA_PRES_EXT) != 0)
        pres[num_pres++] = (uint8_t)bits_read(buf, bb, &bp, 8);

    /* Fields + TLV */
    dec->fields = IOTDATA_FIELD_EMPTY;
    const iotdata_variant_def_t *vdef = iotdata_get_variant(dec->variant);
    if (vdef == NULL)
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;
    for (int si = 0; si < _iotdata_field_count(num_pres) && si < IOTDATA_MAX_DATA_FIELDS; si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && _iotdata_field_pres_byte(si) < num_pres && pres[_iotdata_field_pres_byte(si)] & (1U << _iotdata_field_pres_bit(si))) {
            IOTDATA_FIELD_SET(dec->fields, vdef->fields[si].type);
            if (!_iotdata_decode_unpack_field(buf, bb, &bp, dec, vdef->fields[si].type))
                return IOTDATA_ERR_DECODE_TRUNCATED;
        }
    if (!iotdata_decode_tlv_data(buf, bb, &bp, dec, pres[0]))
        return IOTDATA_ERR_DECODE_TRUNCATED;

    dec->packed_bits = bp;
    dec->packed_bytes = bits_to_bytes(bp);
    return IOTDATA_OK;
}

#endif /* !IOTDATA_NO_DECODE */

#if !defined(IOTDATA_NO_DECODE)
#define _IOTDATA_ERR_DECODE \
    case IOTDATA_ERR_DECODE_SHORT: \
        return "Decoding buffer too short for header"; \
    case IOTDATA_ERR_DECODE_TRUNCATED: \
        return "Decoding buffer too short for content"; \
    case IOTDATA_ERR_DECODE_VARIANT: \
        return "Decoding variant unsupported";
#elif !defined(IOTDATA_NO_DUMP)
#define _IOTDATA_ERR_DECODE \
    case IOTDATA_ERR_DECODE_SHORT: \
        return "Decoding buffer too short for header"; \
    case IOTDATA_ERR_DECODE_TRUNCATED: \
        return "Decoding buffer too short for content";
#else
#define _IOTDATA_ERR_DECODE
#endif

/* =========================================================================
 * External JSON
 * ========================================================================= */

#if !defined(IOTDATA_NO_JSON)
#if !defined(IOTDATA_NO_DECODE)

static void _iotdata_decode_to_json_set_field(cJSON *root, const iotdata_decoded_t *dec, iotdata_field_type_t type, const char *label, iotdata_decode_to_json_scratch_t *scratch) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->json_set)
        ops->json_set(root, dec, label, scratch);
}

iotdata_status_t iotdata_decode_to_json(const uint8_t *buf, size_t len, char **json_out, iotdata_decode_to_json_scratch_t *scratch) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!json_out)
        return IOTDATA_ERR_CTX_NULL;
    if (!scratch)
        return IOTDATA_ERR_BUF_NULL;
#endif

    iotdata_decoded_t *dec = &scratch->dec;
    iotdata_status_t rc;
    if ((rc = iotdata_decode(buf, len, dec)) != IOTDATA_OK)
        return rc;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return IOTDATA_ERR_JSON_ALLOC;

    cJSON_AddNumberToObject(root, "variant", dec->variant);
    cJSON_AddNumberToObject(root, "station", dec->station);
    cJSON_AddNumberToObject(root, "sequence", dec->sequence);
    cJSON_AddNumberToObject(root, "packed_bits", (uint32_t)dec->packed_bits);
    cJSON_AddNumberToObject(root, "packed_bytes", (uint32_t)dec->packed_bytes);

    /* Fields + TLV */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(dec->variant);
    if (vdef == NULL) {
        cJSON_Delete(root);
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;
    }
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(dec->fields, vdef->fields[si].type))
            _iotdata_decode_to_json_set_field(root, dec, vdef->fields[si].type, vdef->fields[si].label, scratch);
    iotdata_decode_tlv_json_set(root, dec, "data", scratch);

    *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!*json_out)
        return IOTDATA_ERR_JSON_ALLOC;
    return IOTDATA_OK;
}

#endif /* !IOTDATA_NO_DECODE */

#if !defined(IOTDATA_NO_ENCODE)

static iotdata_status_t _iotdata_encode_from_json_get_field(cJSON *root, iotdata_encoder_t *enc, iotdata_field_type_t type, const char *label, iotdata_encode_from_json_scratch_t *scratch) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->json_get)
        return ops->json_get(root, enc, label, scratch);
    return IOTDATA_OK;
}

iotdata_status_t iotdata_encode_from_json(const char *json, uint8_t *buf, size_t buf_size, size_t *out_bytes, iotdata_encode_from_json_scratch_t *scratch) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!scratch)
        return IOTDATA_ERR_BUF_NULL;
#endif
    cJSON *root = cJSON_Parse(json);
    if (!root)
        return IOTDATA_ERR_JSON_PARSE;

    cJSON *j_var = cJSON_GetObjectItem(root, "variant");
    cJSON *j_sid = cJSON_GetObjectItem(root, "station");
    cJSON *j_seq = cJSON_GetObjectItem(root, "sequence");
    if (!j_var || !j_sid || !j_seq) {
        cJSON_Delete(root);
        return IOTDATA_ERR_JSON_MISSING_FIELD;
    }

    iotdata_encoder_t *enc = &scratch->enc;
    iotdata_status_t rc;
    if ((rc = iotdata_encode_begin(enc, buf, buf_size, (uint8_t)j_var->valueint, (uint16_t)j_sid->valueint, (uint16_t)j_seq->valueint)) != IOTDATA_OK) {
        cJSON_Delete(root);
        return rc;
    }

    /* Fields + TLV */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(enc->variant);
    if (vdef == NULL) {
        cJSON_Delete(root);
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;
    }
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type))
            if ((rc = _iotdata_encode_from_json_get_field(root, enc, vdef->fields[si].type, vdef->fields[si].label, scratch)) != IOTDATA_OK) {
                cJSON_Delete(root);
                return rc;
            }
    if ((rc = iotdata_encode_tlv_json_get(root, enc, "data", scratch)) != IOTDATA_OK) {
        cJSON_Delete(root);
        return rc;
    }

    rc = iotdata_encode_end(enc, out_bytes);
    cJSON_Delete(root);
    return rc;
}

#endif /* !IOTDATA_NO_ENCODE */
#endif /* !IOTDATA_NO_JSON */

#if !defined(IOTDATA_NO_JSON)
#define _IOTDATA_ERR_JSON \
    case IOTDATA_ERR_JSON_PARSE: \
        return "JSON parse error"; \
    case IOTDATA_ERR_JSON_ALLOC: \
        return "JSON allocation error"; \
    case IOTDATA_ERR_JSON_MISSING_FIELD: \
        return "JSON mandatory field missing";
#else
#define _IOTDATA_ERR_JSON
#endif

/* =========================================================================
 * External DUMP
 * ========================================================================= */

#if !defined(IOTDATA_NO_DUMP)

static int dump_add(iotdata_dump_t *dump, int n, size_t bit_offset, size_t bit_length, uint32_t raw_value, const char *decoded, const char *range, const char *name) {
    // XXX silent overflow
    if (n >= IOTDATA_MAX_DUMP_ENTRIES)
        return n;
    iotdata_dump_entry_t *e = &dump->entries[n];
    e->bit_offset = bit_offset;
    e->bit_length = bit_length;
    e->raw_value = raw_value;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wrestrict"
    snprintf(e->field_name, sizeof(e->field_name), "%s", name);
    snprintf(e->decoded_str, sizeof(e->decoded_str), "%s", decoded);
#ifdef IOTDATA_DUMP_RANGE_STR_STATIC
    e->range_str = range;
#else
    snprintf(e->range_str, sizeof(e->range_str), "%s", range);
#endif
#pragma GCC diagnostic pop
    return n + 1;
}

static int _iotdata_dump_build_field(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->dump)
        return ops->dump(buf, bb, bp, dump, n, label);
    return n;
}

static iotdata_status_t _iotdata_dump_build(iotdata_dump_t *dump, const uint8_t *buf, size_t len) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf || !dump)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < IOTDATA_HEADER_BITS / 8 + 1)
        return IOTDATA_ERR_DECODE_SHORT;
    // XXX should check the rest for TRUNCATED ...

    size_t bb = len * 8, bp = 0;
    int n = 0;
    size_t s;
    uint32_t raw;

    dump->count = 0;
    dump->packed_bits = 0;
    dump->packed_bytes = 0;

    /* Header */
    s = bp;
    raw = bits_read(buf, bb, &bp, IOTDATA_VARIANT_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%" PRIu32, raw);
    n = dump_add(dump, n, s, IOTDATA_VARIANT_BITS, raw, dump->_dec_buf, "0-14 (15=rsvd)", "variant");
    const uint8_t variant = (uint8_t)raw;
    s = bp;
    raw = bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%" PRIu32, raw);
    n = dump_add(dump, n, s, IOTDATA_STATION_BITS, raw, dump->_dec_buf, "0-4095", "station");
    s = bp;
    raw = bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%" PRIu32, raw);
    n = dump_add(dump, n, s, IOTDATA_SEQUENCE_BITS, raw, dump->_dec_buf, "0-65535", "sequence");

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    s = bp;
    pres[0] = (uint8_t)bits_read(buf, bb, &bp, 8);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02" PRIX8, pres[0]);
    n = dump_add(dump, n, s, 8, pres[0], dump->_dec_buf, "ext|tlv|6 fields", "presence[0]");
    int num_pres = 1;
    while (num_pres < IOTDATA_PRES_MAXIMUM && bp + 8 <= bb && (pres[num_pres - 1] & IOTDATA_PRES_EXT) != 0) {
        s = bp;
        pres[num_pres] = (uint8_t)bits_read(buf, bb, &bp, 8);
        char pname[24];
        snprintf(pname, sizeof(pname), "presence[%d]", num_pres);
        snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02" PRIX8, pres[num_pres]);
        n = dump_add(dump, n, s, 8, pres[num_pres], dump->_dec_buf, "ext|7 fields", pname);
        num_pres++;
    }

    /* Fields + TLV */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant);
    if (vdef == NULL)
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;
    for (int si = 0; si < _iotdata_field_count(num_pres) && si < IOTDATA_MAX_DATA_FIELDS; si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type))
            if (_iotdata_field_pres_byte(si) < num_pres && pres[_iotdata_field_pres_byte(si)] & (1U << _iotdata_field_pres_bit(si)))
                n = _iotdata_dump_build_field(buf, bb, &bp, dump, n, vdef->fields[si].type, vdef->fields[si].label);
    n = iotdata_dump_tlv(buf, bb, &bp, dump, n, "tlv", pres[0]);

    dump->count = (size_t)n;
    dump->packed_bits = bp;
    dump->packed_bytes = bits_to_bytes(bp);
    return IOTDATA_OK;
}

static iotdata_status_t _iotdata_dump_decoded(const iotdata_dump_t *dump, iotdata_buf_t *bp) {
    bprintf(bp, "%12s  %6s  %-24s  %10s  %-28s  %s\n", "Offset", "Len", "Field", "Raw", "Decoded", "Range");
    bprintf(bp, "%12s  %6s  %-24s  %10s  %-28s  %s\n", "------", "---", "-----", "---", "-------", "-----");
    for (size_t i = 0; i < dump->count; i++) {
        const iotdata_dump_entry_t *e = &dump->entries[i];
        bprintf(bp, "%12" PRIu32 "  %6" PRIu32 "  %-24s  %10" PRIu32 "  %-28s  %s\n", (uint32_t)e->bit_offset, (uint32_t)e->bit_length, e->field_name, e->raw_value, e->decoded_str, e->range_str);
    }
    bprintf(bp, "\nTotal: %" PRIu32 " bits (%" PRIu32 " bytes)\n", (uint32_t)dump->packed_bits, (uint32_t)dump->packed_bytes);
    return IOTDATA_OK;
}

static iotdata_status_t _iotdata_dump_oneline(const iotdata_dump_t *dump, iotdata_buf_t *bp) {
    for (size_t i = 0; i < dump->count; i++) {
        const iotdata_dump_entry_t *e = &dump->entries[i];
        bprintf(bp, "%s%s=%s", (i > 0 ? ", " : ""), e->field_name, e->decoded_str);
    }
    return IOTDATA_OK;
}

iotdata_status_t iotdata_dump_to_string(iotdata_dump_t *dump, const uint8_t *buf, size_t len, char *out, size_t out_size, bool verbose) {
    iotdata_status_t rc;
    if ((rc = _iotdata_dump_build(dump, buf, len)) != IOTDATA_OK)
        return rc;
    iotdata_buf_t bp = { out, out_size, 0 };
    rc = verbose ? _iotdata_dump_decoded(dump, &bp) : _iotdata_dump_oneline(dump, &bp);
    if (bp.pos < bp.size)
        bp.buf[bp.pos] = '\0';
    return rc;
}

#endif /* !IOTDATA_NO_DUMP */

#if !defined(IOTDATA_NO_DUMP)
#define _IOTDATA_ERR_DUMP \
    case IOTDATA_ERR_DUMP_ALLOC: \
        return "Dump allocation error";
#else
#define _IOTDATA_ERR_DUMP
#endif

/* =========================================================================
 * External PRINT
 * ========================================================================= */

#if !defined(IOTDATA_NO_PRINT)
#if !defined(IOTDATA_NO_DECODE)

static void _iotdata_print_field(const iotdata_decoded_t *dec, iotdata_buf_t *bp, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->print)
        ops->print(dec, bp, label);
}

static iotdata_status_t _iotdata_print_decoded(const iotdata_decoded_t *dec, iotdata_buf_t *bp) {
    const iotdata_variant_def_t *vdef = iotdata_get_variant(dec->variant);
    if (vdef == NULL)
        return IOTDATA_ERR_HDR_VARIANT_UNKNOWN;

    bprintf(bp, "Station %" PRIu16 " seq=%" PRIu16 " var=%" PRIu8 " (%s) [%" PRIu32 " bits, %" PRIu32 " bytes]\n", dec->station, dec->sequence, dec->variant, vdef->name, (uint32_t)dec->packed_bits, (uint32_t)dec->packed_bytes);

    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(dec->fields, vdef->fields[si].type))
            _iotdata_print_field(dec, bp, vdef->fields[si].type, vdef->fields[si].label);
    iotdata_print_tlv(dec, bp, "Data");

    return IOTDATA_OK;
}

iotdata_status_t iotdata_print_decoded_to_string(const iotdata_decoded_t *dec, char *out, size_t out_size) {
    iotdata_buf_t bp = { out, out_size, 0 };
    iotdata_status_t rc = _iotdata_print_decoded(dec, &bp);
    if (bp.pos < bp.size)
        bp.buf[bp.pos] = '\0';
    return rc;
}

iotdata_status_t iotdata_print_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size, iotdata_print_scratch_t *scratch) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!scratch)
        return IOTDATA_ERR_BUF_NULL;
#endif
    iotdata_status_t rc;
    if ((rc = iotdata_decode(buf, len, &scratch->dec)) != IOTDATA_OK)
        return rc;
    iotdata_buf_t bp = { out, out_size, 0 };
    rc = _iotdata_print_decoded(&scratch->dec, &bp);
    if (bp.pos < bp.size)
        bp.buf[bp.pos] = '\0';
    return rc;
}

#endif /* !IOTDATA_NO_DECODE */
#endif /* !IOTDATA_NO_PRINT */

#if !defined(IOTDATA_NO_PRINT)
#define _IOTDATA_ERR_PRINT \
    case IOTDATA_ERR_PRINT_ALLOC: \
        return "Print allocation error";
#else
#define _IOTDATA_ERR_PRINT
#endif

/* =========================================================================
 * External error strings
 * ========================================================================= */

#if !defined(IOTDATA_NO_ERROR_STRINGS)

const char *iotdata_strerror(iotdata_status_t status) {
    switch (status) {

    case IOTDATA_OK:
        return "OK";

        _IOTDATA_ERR_HDR
        _IOTDATA_ERR_ENCODE
        _IOTDATA_ERR_DECODE
        _IOTDATA_ERR_DUMP
        _IOTDATA_ERR_PRINT
        _IOTDATA_ERR_JSON

#define _IOTDATA_ERR_DESC_VISIT(CODE, MSG) \
    case CODE: \
        return MSG;
        IOTDATA_VISIT_ERR_DESCRIPTIONS(_IOTDATA_ERR_DESC_VISIT)
#undef _IOTDATA_ERR_DESC_VISIT

    default:
        return "Unknown error";
    }
}

#endif /* !IOTDATA_NO_ERROR_STRINGS */

/* =========================================================================
 * End
 * ========================================================================= */
