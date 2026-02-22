/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * iotdata.c - reference implementation body
 *
 * Architecture:
 *   1. Per-field inline functions (pack, unpack, json_set, json_get, dump, print)
 *   2. Field dispatcher switches on field type, calls per-field functions
 *   3. Variant table maps field presence bit fields to field types
 *   4. Encoder/decoder iterate fields via variant table, supporting N presence bytes
 *
 * Per-field functions are static inline and guarded by IOTDATA_ENABLE_xxx
 * defines to allow compile-time exclusion on constrained targets.
 */

#include "iotdata.h"

#include <stdlib.h>
#include <string.h>

#if !defined(IOTDATA_NO_FLOATING)
#include <math.h>
#endif

#if !defined(IOTDATA_NO_JSON)
#include <cjson/cJSON.h>
#endif

/* =========================================================================
 * Dependencies
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_IMAGE) || defined(IOTDATA_ENABLE_TLV)
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
#define _IOTDATA_NEED_BASE64_DECODE
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
#define _IOTDATA_NEED_BASE64_ENCODE
#endif
#endif

#if defined(IOTDATA_ENABLE_TLV)
#if !defined(IOTDATA_NO_ENCODE)
#define _IOTDATA_NEED_SIXBIT_ENCODE
#endif
#if !defined(IOTDATA_NO_DECODE)
#define _IOTDATA_NEED_SIXBIT_DECODE
#endif
#endif

/* =========================================================================
 * External Variant maps
 * ========================================================================= */

#if defined(IOTDATA_VARIANT_MAPS) && defined(IOTDATA_VARIANT_MAPS_COUNT)

extern const iotdata_variant_def_t IOTDATA_VARIANT_MAPS[];

const iotdata_variant_def_t *iotdata_get_variant(uint8_t variant) {
    if (variant < IOTDATA_VARIANT_MAPS_COUNT)
        return &IOTDATA_VARIANT_MAPS[variant];
    return &IOTDATA_VARIANT_MAPS[0];
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
    return &IOTDATA_DEFAULT_VARIANTS[0];
}

#endif /* IOTDATA_VARIANT_MAPS */

/* =========================================================================
 * Internal dump structures
 * ========================================================================= */

#if !defined(IOTDATA_NO_DUMP)
#define IOTDATA_DUMP_FIELD_NAME_MAX  32
#define IOTDATA_DUMP_DECODED_STR_MAX 32
#define IOTDATA_DUMP_RANGE_STR_MAX   32

typedef struct {
    size_t bit_offset;
    size_t bit_length;
    char field_name[IOTDATA_DUMP_FIELD_NAME_MAX];
    uint32_t raw_value;
    char decoded_str[IOTDATA_DUMP_DECODED_STR_MAX];
    char range_str[IOTDATA_DUMP_RANGE_STR_MAX];
} iotdata_dump_entry_t;

#define IOTDATA_MAX_DUMP_ENTRIES 48

typedef struct {
    char _dec_buf[IOTDATA_DUMP_DECODED_STR_MAX];
    iotdata_dump_entry_t entries[IOTDATA_MAX_DUMP_ENTRIES];
    size_t count;
    size_t packed_bits;
    size_t packed_bytes;
} iotdata_dump_t;
#endif /* !IOTDATA_NO_DUMP */

/* =========================================================================
 * Internal field operations table
 * ========================================================================= */

#define _IOTDATA_FIELD_OP_NAME // const char *name;
#define _IOTDATA_OP_NAME(nm)   // unused
#define _IOTDATA_FIELD_OP_BITS // uint16_t bits;
#define _IOTDATA_OP_BITS(bt)   // unused

#if !defined(IOTDATA_NO_ENCODE)
typedef void (*iotdata_pack_fn)(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc);
#define _IOTDATA_FIELD_OP_PACK iotdata_pack_fn pack;
#define _IOTDATA_OP_PACK(fn)   .pack = (fn),
#else
#define _IOTDATA_FIELD_OP_PACK
#define _IOTDATA_OP_PACK(fn)
#endif

#if !defined(IOTDATA_NO_DECODE)
typedef void (*iotdata_unpack_fn)(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out);
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

#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
typedef void (*iotdata_print_fn)(const iotdata_decoded_t *d, FILE *fp, const char *l);
#define _IOTDATA_FIELD_OP_PRINT iotdata_print_fn print;
#define _IOTDATA_OP_PRINT(fn)   .print = (fn),
#else
#define _IOTDATA_FIELD_OP_PRINT
#define _IOTDATA_OP_PRINT(fn)
#endif

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
typedef void (*iotdata_json_set_fn)(void *root, const void *d, const char *label);
#define _IOTDATA_FIELD_OP_JSON_SET iotdata_json_set_fn json_set;
#define _IOTDATA_OP_JSON_SET(fn)   .json_set = (iotdata_json_set_fn)(fn),
#else
#define _IOTDATA_FIELD_OP_JSON_SET
#define _IOTDATA_OP_JSON_SET(fn)
#endif

#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
typedef iotdata_status_t (*iotdata_json_get_fn)(void *root, void *enc, const char *label);
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

static inline size_t bits_to_bytes(size_t bits) {
    return (bits + 7) / 8;
}

static inline void bits_write(uint8_t *buf, size_t *bp, uint32_t value, uint8_t nbits) {
    for (int i = nbits - 1; i >= 0; i--, (*bp)++)
        if (value & (1U << i))
            buf[*bp / 8] |= (1U << (7 - (*bp % 8)));
        else
            buf[*bp / 8] &= (uint8_t)~(1U << (7 - (*bp % 8)));
}

#if !defined(IOTDATA_NO_DECODE) || !defined(IOTDATA_NO_DUMP)
static inline uint32_t bits_read(const uint8_t *buf, size_t buf_bits, size_t *bp, uint8_t nbits) {
    uint32_t value = 0;
    for (int i = nbits - 1; i >= 0 && *bp < buf_bits; i--, (*bp)++)
        if (buf[*bp / 8] & (1U << (7 - (*bp % 8))))
            value |= (1U << i);
    return value;
}
#endif

/* =========================================================================
 * Utilities
 * ========================================================================= */

#if defined(_IOTDATA_NEED_BASE64_ENCODE)
static inline void _b64_encode(const uint8_t *in, size_t in_len, char *out) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, j = 0;
    while (i < in_len) {
        const uint32_t a = in[i++], b = (i < in_len) ? in[i++] : 0, c = (i < in_len) ? in[i++] : 0;
        const uint32_t trip = (a << 16) | (b << 8) | c;
        out[j++] = t[(trip >> 18) & 0x3F];
        out[j++] = t[(trip >> 12) & 0x3F];
        out[j++] = t[(trip >> 6) & 0x3F];
        out[j++] = t[(trip >> 0) & 0x3F];
    }
    if (in_len % 3 == 1)
        out[j - 2] = '=';
    if (in_len % 3 == 1 || in_len % 3 == 2)
        out[j - 1] = '=';
    out[j] = '\0';
}
#endif

#if defined(_IOTDATA_NEED_BASE64_DECODE)
static inline int _b64_val(char c) {
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
static inline size_t _b64_decode(const char *in, uint8_t *out, size_t out_max) {
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
static inline int char_to_sixbit(char c) {
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
static inline char sixbit_to_char(uint8_t val) {
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
static inline const char *_padd(const char *label) {
    static const char spaces[] = "                    "; /* 20 spaces */
    const int n = 20 - (int)strlen(label);
    return &spaces[20 - (n > 0 ? n : 1)];
}
#endif

#if !(defined(IOTDATA_NO_DUMP) || defined(IOTDATA_NO_PRINT))
#if defined(IOTDATA_NO_FLOATING)
static inline int fmt_scaled10(char *buf, size_t sz, int32_t val, const char *unit) {
    const int32_t a = val < 0 ? -val : val;
    return snprintf(buf, sz, "%s%d.%01d%s%s", val < 0 ? "-" : "", (int)(a / 10), (int)(a % 10), unit[0] ? " " : "", unit);
}
static inline int fmt_scaled100(char *buf, size_t sz, int32_t val, const char *unit) {
    const int32_t a = val < 0 ? -val : val;
    return snprintf(buf, sz, "%s%d.%02d%s%s", val < 0 ? "-" : "", (int)(a / 100), (int)(a % 100), unit[0] ? " " : "", unit);
}
static inline int fmt_scaled10000000(char *buf, size_t sz, int32_t val, const char *unit) {
    const int32_t a = val < 0 ? -val : val;
    return snprintf(buf, sz, "%s%d.%06d%s%s", val < 0 ? "-" : "", (int)(a / 10000000), (int)(a % 10000000), unit[0] ? " " : "", unit);
}
#endif /* !IOTDATA_NO_FLOATING */
#endif /* !IOTDATA_NO_DUMP */

/* =========================================================================
 * Field BATTERY
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_BATTERY)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_battery(iotdata_encoder_t *enc, uint8_t level_percent, bool charging) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_BATTERY);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (level_percent > IOTDATA_BATTERY_LEVEL_MAX)
        return IOTDATA_ERR_BATTERY_LEVEL_HIGH;
#endif
    enc->battery_level = level_percent;
    enc->battery_charging = charging;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_BATTERY);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_battery_level(uint8_t pct) {
    return (uint32_t)(((uint32_t)pct * ((1 << IOTDATA_BATTERY_LEVEL_BITS) - 1) + 50) / IOTDATA_BATTERY_LEVEL_MAX);
}
static inline uint8_t dequantise_battery_level(uint32_t raw) {
    return (uint8_t)((raw * IOTDATA_BATTERY_LEVEL_MAX + 15) / ((1 << IOTDATA_BATTERY_LEVEL_BITS) - 1));
}
static inline uint32_t quantise_battery_state(bool charging) {
    return (uint32_t)(charging ? 1 : 0);
}
static inline bool dequantise_battery_state(uint32_t raw) {
    return (bool)(raw & 1);
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_battery(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_battery_level(enc->battery_level), IOTDATA_BATTERY_LEVEL_BITS);
    bits_write(buf, bp, quantise_battery_state(enc->battery_charging), IOTDATA_BATTERY_CHARGE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_battery(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->battery_level = dequantise_battery_level(bits_read(buf, bb, bp, IOTDATA_BATTERY_LEVEL_BITS));
    out->battery_charging = dequantise_battery_state(bits_read(buf, bb, bp, IOTDATA_BATTERY_CHARGE_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_battery(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_battery(enc, (uint8_t)cJSON_GetObjectItem(j, "level")->valueint, cJSON_IsTrue(cJSON_GetObjectItem(j, "charging")));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_battery(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    cJSON_AddNumberToObject(obj, "level", d->battery_level);
    cJSON_AddBoolToObject(obj, "charging", d->battery_charging);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_battery(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_BATTERY_LEVEL_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u%%", dequantise_battery_level(r));
    n = dump_add(dump, n, s, IOTDATA_BATTERY_LEVEL_BITS, r, dump->_dec_buf, "0..100%%, 5b quant", "battery_level");
    s = *bp;
    r = bits_read(buf, bb, bp, IOTDATA_BATTERY_CHARGE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%s", dequantise_battery_state(r) ? "charging" : "discharging");
    n = dump_add(dump, n, s, IOTDATA_BATTERY_CHARGE_BITS, r, dump->_dec_buf, "0/1", "battery_charging");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_battery(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u%% %s\n", l, _padd(l), d->battery_level, d->battery_charging ? "(charging)" : "(discharging)");
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_battery = {
    _IOTDATA_OP_NAME("battery")
    _IOTDATA_OP_BITS(IOTDATA_BATTERY_LEVEL_BITS + IOTDATA_BATTERY_CHARGE_BITS)
    _IOTDATA_OP_PACK(pack_battery)
    _IOTDATA_OP_UNPACK(unpack_battery)
    _IOTDATA_OP_DUMP(dump_battery)
    _IOTDATA_OP_PRINT(print_battery)
    _IOTDATA_OP_JSON_SET(json_set_battery)
    _IOTDATA_OP_JSON_GET(json_get_battery)
};
#define _IOTDATA_ENT_BATTERY [IOTDATA_FIELD_BATTERY] = &_iotdata_field_def_battery,
#define _IOTDATA_ERR_BATTERY \
    case IOTDATA_ERR_BATTERY_LEVEL_HIGH: \
        return "Battery level above 100%"; 
#else
#define _IOTDATA_ENT_BATTERY
#define _IOTDATA_ERR_BATTERY
#endif
// clang-format on

/* =========================================================================
 * Field LINK
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_LINK)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_link(iotdata_encoder_t *enc, int16_t rssi_dbm, iotdata_float_t snr_db) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_LINK);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (rssi_dbm < IOTDATA_LINK_RSSI_MIN)
        return IOTDATA_ERR_LINK_RSSI_LOW;
    if (rssi_dbm > IOTDATA_LINK_RSSI_MAX)
        return IOTDATA_ERR_LINK_RSSI_HIGH;
    if (snr_db < IOTDATA_LINK_SNR_MIN)
        return IOTDATA_ERR_LINK_SNR_LOW;
    if (snr_db > IOTDATA_LINK_SNR_MAX)
        return IOTDATA_ERR_LINK_SNR_HIGH;
#endif
    enc->link_rssi = rssi_dbm;
    enc->link_snr = snr_db;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_LINK);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_link_rssi(int16_t rssi) {
    return (uint32_t)(((rssi < IOTDATA_LINK_RSSI_MIN ? IOTDATA_LINK_RSSI_MIN : rssi > IOTDATA_LINK_RSSI_MAX ? IOTDATA_LINK_RSSI_MAX : rssi) - IOTDATA_LINK_RSSI_MIN) / IOTDATA_LINK_RSSI_STEP);
}
static inline int16_t dequantise_link_rssi(uint32_t raw) {
    return (int16_t)(IOTDATA_LINK_RSSI_MIN + (int)raw * IOTDATA_LINK_RSSI_STEP);
}
#if !defined(IOTDATA_NO_FLOATING)
static inline uint32_t quantise_link_snr(float snr) {
    return (uint32_t)roundf(((snr < IOTDATA_LINK_SNR_MIN ? IOTDATA_LINK_SNR_MIN : snr > IOTDATA_LINK_SNR_MAX ? IOTDATA_LINK_SNR_MAX : snr) - IOTDATA_LINK_SNR_MIN) / IOTDATA_LINK_SNR_STEP);
}
static inline float dequantise_link_snr(uint32_t raw) {
    return IOTDATA_LINK_SNR_MIN + (float)raw * IOTDATA_LINK_SNR_STEP;
}
#else
static inline uint32_t quantise_link_snr(int32_t snr10) {
    return (uint32_t)(((snr10 < IOTDATA_LINK_SNR_MIN ? IOTDATA_LINK_SNR_MIN : snr10 > IOTDATA_LINK_SNR_MAX ? IOTDATA_LINK_SNR_MAX : snr10) - IOTDATA_LINK_SNR_MIN + (IOTDATA_LINK_SNR_STEP / 2)) / IOTDATA_LINK_SNR_STEP);
}
static inline int32_t dequantise_link_snr(uint32_t raw) {
    return IOTDATA_LINK_SNR_MIN + (int32_t)raw * IOTDATA_LINK_SNR_STEP;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_link(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_link_rssi(enc->link_rssi), IOTDATA_LINK_RSSI_BITS);
    bits_write(buf, bp, quantise_link_snr(enc->link_snr), IOTDATA_LINK_SNR_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_link(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->link_rssi = dequantise_link_rssi(bits_read(buf, bb, bp, IOTDATA_LINK_RSSI_BITS));
    out->link_snr = dequantise_link_snr(bits_read(buf, bb, bp, IOTDATA_LINK_SNR_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_link(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_link(enc, (int16_t)cJSON_GetObjectItem(j, "rssi")->valueint, (iotdata_float_t)cJSON_GetObjectItem(j, "snr")->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_link(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    cJSON_AddNumberToObject(obj, "rssi", d->link_rssi);
    cJSON_AddNumberToObject(obj, "snr", d->link_snr);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_link(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_LINK_RSSI_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%d dBm", dequantise_link_rssi(r));
    n = dump_add(dump, n, s, IOTDATA_LINK_RSSI_BITS, r, dump->_dec_buf, "-120..-60, 4dBm", "link_rssi");
    s = *bp;
    r = bits_read(buf, bb, bp, IOTDATA_LINK_SNR_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.0f dB", dequantise_link_snr(r));
#else
    fmt_scaled10(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_link_snr(r), "dB");
#endif
    n = dump_add(dump, n, s, IOTDATA_LINK_SNR_BITS, r, dump->_dec_buf, "-20..+10, 10dB", "link_snr");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_link(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %d dBm RSSI, %.0f dB SNR\n", l, _padd(l), d->link_rssi, d->link_snr);
#else
    fprintf(fp, "  %s:%s %d dBm RSSI, %d.%d dB SNR\n", l, _padd(l), d->link_rssi, d->link_snr / 10, d->link_snr % 10);
#endif
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_link = {
    _IOTDATA_OP_NAME("link")
    _IOTDATA_OP_BITS(IOTDATA_LINK_RSSI_BITS + IOTDATA_LINK_SNR_BITS)
    _IOTDATA_OP_PACK(pack_link)
    _IOTDATA_OP_UNPACK(unpack_link)
    _IOTDATA_OP_DUMP(dump_link)
    _IOTDATA_OP_PRINT(print_link)
    _IOTDATA_OP_JSON_SET(json_set_link)
    _IOTDATA_OP_JSON_GET(json_get_link)
};
#define _IOTDATA_ENT_LINK [IOTDATA_FIELD_LINK] = &_iotdata_field_def_link,
#define _IOTDATA_ERR_LINK \
    case IOTDATA_ERR_LINK_RSSI_LOW: \
        return "RSSI below -120 dBm"; \
    case IOTDATA_ERR_LINK_RSSI_HIGH: \
        return "RSSI above -60 dBm"; \
    case IOTDATA_ERR_LINK_SNR_LOW: \
        return "SNR below -20 dB"; \
    case IOTDATA_ERR_LINK_SNR_HIGH: \
        return "SNR above +10 dB";
#else
#define _IOTDATA_ENT_LINK
#define _IOTDATA_ERR_LINK
#endif
// clang-format on

/* =========================================================================
 * Field ENVIRONMENT, TEMPERATURE, PRESSURE, HUMIDITY
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_TEMPERATURE) || defined(IOTDATA_ENABLE_ENVIRONMENT)
#if defined(IOTDATA_ENABLE_TEMPERATURE) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_temperature(iotdata_encoder_t *enc, iotdata_float_t temperature_c) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_TEMPERATURE);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (temperature_c < IOTDATA_TEMPERATURE_MIN)
        return IOTDATA_ERR_TEMPERATURE_LOW;
    if (temperature_c > IOTDATA_TEMPERATURE_MAX)
        return IOTDATA_ERR_TEMPERATURE_HIGH;
#endif
    enc->temperature = temperature_c;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_TEMPERATURE);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_FLOATING)
static inline uint32_t quantise_temperature(float temperature) {
    return (uint32_t)roundf((temperature - IOTDATA_TEMPERATURE_MIN) / IOTDATA_TEMPERATURE_RES);
}
static inline float dequantise_temperature(uint32_t raw) {
    return IOTDATA_TEMPERATURE_MIN + (float)raw * IOTDATA_TEMPERATURE_RES;
}
#else
static inline uint32_t quantise_temperature(int32_t temperature100) {
    return (uint32_t)((temperature100 - IOTDATA_TEMPERATURE_MIN + (IOTDATA_TEMPERATURE_RES / 2)) / IOTDATA_TEMPERATURE_RES);
}
static inline int32_t dequantise_temperature(uint32_t raw) {
    return (int32_t)raw * IOTDATA_TEMPERATURE_RES + IOTDATA_TEMPERATURE_MIN;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_temperature(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_temperature(enc->temperature), IOTDATA_TEMPERATURE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_temperature(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->temperature = dequantise_temperature(bits_read(buf, bb, bp, IOTDATA_TEMPERATURE_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_temperature(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_temperature(enc, (iotdata_float_t)j->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_temperature(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->temperature);
}
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE) && !defined(IOTDATA_NO_DUMP)
static inline int dump_temperature(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_TEMPERATURE_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.2f C", dequantise_temperature(r));
#else
    fmt_scaled100(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_temperature(r), "C");
#endif
    n = dump_add(dump, n, s, IOTDATA_TEMPERATURE_BITS, r, dump->_dec_buf, "-40..+80C, 0.25C", "temperature");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_temperature(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.2f C\n", l, _padd(l), d->temperature);
#else
    const int32_t ta = d->temperature < 0 ? -(d->temperature) : d->temperature;
    fprintf(fp, "  %s:%s %s%d.%02d C\n", l, _padd(l), d->temperature < 0 ? "-" : "", ta / 100, ta % 100);
#endif
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_TEMPERATURE)
static const iotdata_field_ops_t _iotdata_field_def_temperature = {
    _IOTDATA_OP_NAME("temperature")
    _IOTDATA_OP_BITS(IOTDATA_TEMPERATURE_BITS)
    _IOTDATA_OP_PACK(pack_temperature)
    _IOTDATA_OP_UNPACK(unpack_temperature)
    _IOTDATA_OP_DUMP(dump_temperature)
    _IOTDATA_OP_PRINT(print_temperature)
    _IOTDATA_OP_JSON_SET(json_set_temperature)
    _IOTDATA_OP_JSON_GET(json_get_temperature)
};
#define _IOTDATA_ENT_TEMPERATURE [IOTDATA_FIELD_TEMPERATURE] = &_iotdata_field_def_temperature,
#else
#define _IOTDATA_ENT_TEMPERATURE
#endif
#define _IOTDATA_ERR_TEMPERATURE \
    case IOTDATA_ERR_TEMPERATURE_LOW: \
        return "Temperature below -40C"; \
    case IOTDATA_ERR_TEMPERATURE_HIGH: \
        return "Temperature above +80C";
#else
#define _IOTDATA_ENT_TEMPERATURE
#define _IOTDATA_ERR_TEMPERATURE
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_PRESSURE) || defined(IOTDATA_ENABLE_ENVIRONMENT)
#if defined(IOTDATA_ENABLE_PRESSURE) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_pressure(iotdata_encoder_t *enc, uint16_t pressure_hpa) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_PRESSURE);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (pressure_hpa < IOTDATA_PRESSURE_MIN)
        return IOTDATA_ERR_PRESSURE_LOW;
    if (pressure_hpa > IOTDATA_PRESSURE_MAX)
        return IOTDATA_ERR_PRESSURE_HIGH;
#endif
    enc->pressure = pressure_hpa;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_PRESSURE);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_pressure(uint16_t pressure) {
    return (uint32_t)(pressure - IOTDATA_PRESSURE_MIN);
}
static inline uint16_t dequantise_pressure(uint32_t raw) {
    return (uint16_t)(raw + IOTDATA_PRESSURE_MIN);
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_pressure(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_pressure(enc->pressure), IOTDATA_PRESSURE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_pressure(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->pressure = dequantise_pressure(bits_read(buf, bb, bp, IOTDATA_PRESSURE_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_PRESSURE) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_pressure(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_pressure(enc, (uint16_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_pressure(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->pressure);
}
#endif
#if defined(IOTDATA_ENABLE_PRESSURE) && !defined(IOTDATA_NO_DUMP)
static inline int dump_pressure(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_PRESSURE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u hPa", dequantise_pressure(r));
    n = dump_add(dump, n, s, IOTDATA_PRESSURE_BITS, r, dump->_dec_buf, "850..1105 hPa", "pressure");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_PRESSURE) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_pressure(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u hPa\n", l, _padd(l), d->pressure);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_PRESSURE)
static const iotdata_field_ops_t _iotdata_field_def_pressure = {
    _IOTDATA_OP_NAME("pressure")
    _IOTDATA_OP_BITS(IOTDATA_PRESSURE_BITS)
    _IOTDATA_OP_PACK(pack_pressure)
    _IOTDATA_OP_UNPACK(unpack_pressure)
    _IOTDATA_OP_DUMP(dump_pressure)
    _IOTDATA_OP_PRINT(print_pressure)
    _IOTDATA_OP_JSON_SET(json_set_pressure)
    _IOTDATA_OP_JSON_GET(json_get_pressure)
};
#define _IOTDATA_ENT_PRESSURE [IOTDATA_FIELD_PRESSURE] = &_iotdata_field_def_pressure,
#else
#define _IOTDATA_ENT_PRESSURE
#endif
#define _IOTDATA_ERR_PRESSURE \
    case IOTDATA_ERR_PRESSURE_LOW: \
        return "Pressure below 850 hPa"; \
    case IOTDATA_ERR_PRESSURE_HIGH: \
        return "Pressure above 1105 hPa";
#else
#define _IOTDATA_ENT_PRESSURE
#define _IOTDATA_ERR_PRESSURE
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_HUMIDITY) || defined(IOTDATA_ENABLE_ENVIRONMENT)
#if defined(IOTDATA_ENABLE_HUMIDITY) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_humidity(iotdata_encoder_t *enc, uint8_t humidity_pct) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_HUMIDITY);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (humidity_pct > IOTDATA_HUMIDITY_MAX)
        return IOTDATA_ERR_HUMIDITY_HIGH;
#endif
    enc->humidity = humidity_pct;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_HUMIDITY);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_humidity(uint8_t humidity) {
    return (uint32_t)humidity;
}
static inline uint8_t dequantise_humidity(uint32_t raw) {
    return (uint8_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_humidity(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_humidity(enc->humidity), IOTDATA_HUMIDITY_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_humidity(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->humidity = dequantise_humidity(bits_read(buf, bb, bp, IOTDATA_HUMIDITY_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_humidity(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_humidity(enc, (uint8_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_humidity(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->humidity);
}
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY) && !defined(IOTDATA_NO_DUMP)
static inline int dump_humidity(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_HUMIDITY_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u%%", dequantise_humidity(r));
    n = dump_add(dump, n, s, IOTDATA_HUMIDITY_BITS, r, dump->_dec_buf, "0..100%%", "humidity");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_humidity(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u%%\n", l, _padd(l), d->humidity);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_HUMIDITY)
static const iotdata_field_ops_t _iotdata_field_def_humidity = {
    _IOTDATA_OP_NAME("humidity")
    _IOTDATA_OP_BITS(IOTDATA_HUMIDITY_BITS)
    _IOTDATA_OP_PACK(pack_humidity)
    _IOTDATA_OP_UNPACK(unpack_humidity)
    _IOTDATA_OP_DUMP(dump_humidity)
    _IOTDATA_OP_PRINT(print_humidity)
    _IOTDATA_OP_JSON_SET(json_set_humidity)
    _IOTDATA_OP_JSON_GET(json_get_humidity)
};
#define _IOTDATA_ENT_HUMIDITY [IOTDATA_FIELD_HUMIDITY] = &_iotdata_field_def_humidity,
#else
#define _IOTDATA_ENT_HUMIDITY
#endif
#define _IOTDATA_ERR_HUMIDITY \
    case IOTDATA_ERR_HUMIDITY_HIGH: \
        return "Humidity above 100%";
#else
#define _IOTDATA_ENT_HUMIDITY
#define _IOTDATA_ERR_HUMIDITY
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_ENVIRONMENT)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_environment(iotdata_encoder_t *enc, iotdata_float_t temperature_c, uint16_t pressure_hpa, uint8_t humidity_pct) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_ENVIRONMENT);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (temperature_c < IOTDATA_TEMPERATURE_MIN)
        return IOTDATA_ERR_TEMPERATURE_LOW;
    if (temperature_c > IOTDATA_TEMPERATURE_MAX)
        return IOTDATA_ERR_TEMPERATURE_HIGH;
    if (pressure_hpa < IOTDATA_PRESSURE_MIN)
        return IOTDATA_ERR_PRESSURE_LOW;
    if (pressure_hpa > IOTDATA_PRESSURE_MAX)
        return IOTDATA_ERR_PRESSURE_HIGH;
    if (humidity_pct > IOTDATA_HUMIDITY_MAX)
        return IOTDATA_ERR_HUMIDITY_HIGH;
#endif
    enc->temperature = temperature_c;
    enc->pressure = pressure_hpa;
    enc->humidity = humidity_pct;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_ENVIRONMENT);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_environment(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    pack_temperature(buf, bp, enc);
    pack_pressure(buf, bp, enc);
    pack_humidity(buf, bp, enc);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_environment(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    unpack_temperature(buf, bb, bp, out);
    unpack_pressure(buf, bb, bp, out);
    unpack_humidity(buf, bb, bp, out);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_environment(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_environment(enc, (iotdata_float_t)cJSON_GetObjectItem(j, "temperature")->valuedouble, (uint16_t)cJSON_GetObjectItem(j, "pressure")->valueint, (uint8_t)cJSON_GetObjectItem(j, "humidity")->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_environment(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    json_set_temperature(obj, d, "temperature");
    json_set_pressure(obj, d, "pressure");
    json_set_humidity(obj, d, "humidity");
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_environment(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    n = dump_temperature(buf, bb, bp, dump, n, label);
    n = dump_pressure(buf, bb, bp, dump, n, label);
    n = dump_humidity(buf, bb, bp, dump, n, label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_environment(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.2f C, %u hPa, %u%%\n", l, _padd(l), d->temperature, d->pressure, d->humidity);
#else
    const int32_t ta = d->temperature < 0 ? -(d->temperature) : d->temperature;
    fprintf(fp, "  %s:%s %s%d.%02d C, %u hPa, %u%%\n", l, _padd(l), d->temperature < 0 ? "-" : "", ta / 100, ta % 100, d->pressure, d->humidity);
#endif
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_environment = {
    _IOTDATA_OP_NAME("environment")
    _IOTDATA_OP_BITS(IOTDATA_TEMPERATURE_BITS + IOTDATA_PRESSURE_BITS + IOTDATA_HUMIDITY_BITS)
    _IOTDATA_OP_PACK(pack_environment)
    _IOTDATA_OP_UNPACK(unpack_environment)
    _IOTDATA_OP_DUMP(dump_environment)
    _IOTDATA_OP_PRINT(print_environment)
    _IOTDATA_OP_JSON_SET(json_set_environment)
    _IOTDATA_OP_JSON_GET(json_get_environment)
};
#define _IOTDATA_ENT_ENVIRONMENT [IOTDATA_FIELD_ENVIRONMENT] = &_iotdata_field_def_environment,
#define _IOTDATA_ERR_ENVIRONMENT
#else
#define _IOTDATA_ENT_ENVIRONMENT
#define _IOTDATA_ERR_ENVIRONMENT
#endif
// clang-format on

/* =========================================================================
 * Field WIND, WIND_SPEED, WIND_DIRECTION, WIND_GUST
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_WIND_SPEED) || defined(IOTDATA_ENABLE_WIND_GUST) || defined(IOTDATA_ENABLE_WIND)
#if defined(IOTDATA_ENABLE_WIND_SPEED) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_wind_speed(iotdata_encoder_t *enc, iotdata_float_t speed_ms) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_WIND_SPEED);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (speed_ms < 0 || speed_ms > IOTDATA_WIND_SPEED_MAX)
        return IOTDATA_ERR_WIND_SPEED_HIGH;
#endif
    enc->wind_speed = speed_ms;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_WIND_SPEED);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_FLOATING)
static inline uint32_t quantise_wind_speed(float speed) {
    return (uint32_t)roundf(speed / IOTDATA_WIND_SPEED_RES);
}
static inline float dequantise_wind_speed(uint32_t raw) {
    return (float)raw * IOTDATA_WIND_SPEED_RES;
}
#else
static inline uint32_t quantise_wind_speed(int32_t speed100) {
    return (uint32_t)((speed100 + (IOTDATA_WIND_SPEED_RES / 2)) / IOTDATA_WIND_SPEED_RES);
}
static inline int32_t dequantise_wind_speed(uint32_t raw) {
    return (int32_t)raw * IOTDATA_WIND_SPEED_RES;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_wind_speed(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_wind_speed(enc->wind_speed), IOTDATA_WIND_SPEED_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_wind_speed(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->wind_speed = dequantise_wind_speed(bits_read(buf, bb, bp, IOTDATA_WIND_SPEED_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_wind_speed(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_wind_speed(enc, (iotdata_float_t)j->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_wind_speed(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->wind_speed);
}
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED) && !defined(IOTDATA_NO_DUMP)
static inline int dump_wind_speed(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_WIND_SPEED_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.1f m/s", dequantise_wind_speed(r));
#else
    fmt_scaled100(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_wind_speed(r), "m/s");
#endif
    n = dump_add(dump, n, s, IOTDATA_WIND_SPEED_BITS, r, dump->_dec_buf, "0..63.5, 0.5m/s", "wind_speed");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_wind_speed(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.1f m/s\n", l, _padd(l), d->wind_speed);
#else
    fprintf(fp, "  %s:%s %d.%02d m/s\n", l, _padd(l), d->wind_speed / 100, d->wind_speed % 100);
#endif
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_WIND_SPEED) 
static const iotdata_field_ops_t _iotdata_field_def_wind_speed = {
    _IOTDATA_OP_NAME("wind_speed")
    _IOTDATA_OP_BITS(IOTDATA_WIND_SPEED_BITS)
    _IOTDATA_OP_PACK(pack_wind_speed)
    _IOTDATA_OP_UNPACK(unpack_wind_speed)
    _IOTDATA_OP_DUMP(dump_wind_speed)
    _IOTDATA_OP_PRINT(print_wind_speed)
    _IOTDATA_OP_JSON_SET(json_set_wind_speed)
    _IOTDATA_OP_JSON_GET(json_get_wind_speed)
};
#define _IOTDATA_ENT_WIND_SPEED [IOTDATA_FIELD_WIND_SPEED] = &_iotdata_field_def_wind_speed,
#else
#define _IOTDATA_ENT_WIND_SPEED
#endif
#define _IOTDATA_ERR_WIND_SPEED \
    case IOTDATA_ERR_WIND_SPEED_HIGH: \
        return "Wind speed above 63.5 m/s";
#else
#define _IOTDATA_ENT_WIND_SPEED
#define _IOTDATA_ERR_WIND_SPEED
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_WIND_DIRECTION) || defined(IOTDATA_ENABLE_WIND)
#if defined(IOTDATA_ENABLE_WIND_DIRECTION) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_wind_direction(iotdata_encoder_t *enc, uint16_t direction_deg) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_WIND_DIRECTION);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (direction_deg > IOTDATA_WIND_DIRECTION_MAX)
        return IOTDATA_ERR_WIND_DIRECTION_HIGH;
#endif
    enc->wind_direction = direction_deg;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_WIND_DIRECTION);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_WIND_DIRECTION_SCALE (360.0f / 256.0f)
static inline uint32_t quantise_wind_direction(uint16_t deg) {
    return (uint32_t)roundf((float)deg / IOTDATA_WIND_DIRECTION_SCALE);
}
static inline uint16_t dequantise_wind_direction(uint32_t raw) {
    return (uint16_t)roundf((float)raw * IOTDATA_WIND_DIRECTION_SCALE);
}
#else
static inline uint32_t quantise_wind_direction(uint16_t deg) {
    return (uint32_t)((deg * (1 << IOTDATA_WIND_DIRECTION_BITS) + 180) / 360); // XXX
}
static inline uint16_t dequantise_wind_direction(uint32_t raw) {
    return (uint16_t)((raw * 360 + 128) / (1 << IOTDATA_WIND_DIRECTION_BITS)); // XXX
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_wind_direction(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_wind_direction(enc->wind_direction), IOTDATA_WIND_DIRECTION_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_wind_direction(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->wind_direction = dequantise_wind_direction(bits_read(buf, bb, bp, IOTDATA_WIND_DIRECTION_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_wind_direction(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_wind_direction(enc, (uint16_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_wind_direction(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->wind_direction);
}
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION) && !defined(IOTDATA_NO_DUMP)
static inline int dump_wind_direction(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_WIND_DIRECTION_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u deg", dequantise_wind_direction(r));
    n = dump_add(dump, n, s, IOTDATA_WIND_DIRECTION_BITS, r, dump->_dec_buf, "0..355, ~1.4deg", "wind_direction");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_wind_direction(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u deg\n", l, _padd(l), d->wind_direction);
}
#endif
// clang-format off
#if defined (IOTDATA_ENABLE_WIND_DIRECTION)
static const iotdata_field_ops_t _iotdata_field_def_wind_direction = {
    _IOTDATA_OP_NAME("wind_direction")
    _IOTDATA_OP_BITS(IOTDATA_WIND_DIRECTION_BITS)
    _IOTDATA_OP_PACK(pack_wind_direction)
    _IOTDATA_OP_UNPACK(unpack_wind_direction)
    _IOTDATA_OP_DUMP(dump_wind_direction)
    _IOTDATA_OP_PRINT(print_wind_direction)
    _IOTDATA_OP_JSON_SET(json_set_wind_direction)
    _IOTDATA_OP_JSON_GET(json_get_wind_direction)
};
#define _IOTDATA_ENT_WIND_DIRECTION [IOTDATA_FIELD_WIND_DIRECTION] = &_iotdata_field_def_wind_direction,
#else
#define _IOTDATA_ENT_WIND_DIRECTION
#endif
#define _IOTDATA_ERR_WIND_DIRECTION \
    case IOTDATA_ERR_WIND_DIRECTION_HIGH: \
        return "Wind direction above 359 degrees";
#else
#define _IOTDATA_ENT_WIND_DIRECTION
#define _IOTDATA_ERR_WIND_DIRECTION
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_WIND_GUST) || defined(IOTDATA_ENABLE_WIND)
#if defined(IOTDATA_ENABLE_WIND_GUST) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_wind_gust(iotdata_encoder_t *enc, iotdata_float_t gust_ms) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_WIND_GUST);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (gust_ms < 0 || gust_ms > IOTDATA_WIND_SPEED_MAX)
        return IOTDATA_ERR_WIND_GUST_HIGH;
#endif
    enc->wind_gust = gust_ms;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_WIND_GUST);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_wind_gust(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_wind_speed(enc->wind_gust), IOTDATA_WIND_GUST_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_wind_gust(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->wind_gust = dequantise_wind_speed(bits_read(buf, bb, bp, IOTDATA_WIND_GUST_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_wind_gust(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_wind_gust(enc, (iotdata_float_t)j->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_wind_gust(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->wind_gust);
}
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST) && !defined(IOTDATA_NO_DUMP)
static inline int dump_wind_gust(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_WIND_GUST_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.1f m/s", dequantise_wind_speed(r));
#else
    fmt_scaled100(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_wind_speed(r), "m/s");
#endif
    n = dump_add(dump, n, s, IOTDATA_WIND_GUST_BITS, r, dump->_dec_buf, "0..63.5, 0.5m/s", "wind_gust");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_wind_gust(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.1f m/s\n", l, _padd(l), d->wind_gust);
#else
    fprintf(fp, "  %s:%s %d.%02d m/s\n", l, _padd(l), d->wind_gust / 100, d->wind_gust % 100);
#endif
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_WIND_GUST) 
static const iotdata_field_ops_t _iotdata_field_def_wind_gust = {
    _IOTDATA_OP_NAME("wind_gust")
    _IOTDATA_OP_BITS(IOTDATA_WIND_GUST_BITS)
    _IOTDATA_OP_PACK(pack_wind_gust)
    _IOTDATA_OP_UNPACK(unpack_wind_gust)
    _IOTDATA_OP_DUMP(dump_wind_gust)
    _IOTDATA_OP_PRINT(print_wind_gust)
    _IOTDATA_OP_JSON_SET(json_set_wind_gust)
    _IOTDATA_OP_JSON_GET(json_get_wind_gust)
};
#define _IOTDATA_ENT_WIND_GUST [IOTDATA_FIELD_WIND_GUST] = &_iotdata_field_def_wind_gust,
#else
#define _IOTDATA_ENT_WIND_GUST
#endif
#define _IOTDATA_ERR_WIND_GUST \
    case IOTDATA_ERR_WIND_GUST_HIGH: \
        return "Wind gust above 63.5 m/s";
#else
#define _IOTDATA_ENT_WIND_GUST
#define _IOTDATA_ERR_WIND_GUST
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_WIND)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_wind(iotdata_encoder_t *enc, iotdata_float_t speed_ms, uint16_t direction_deg, iotdata_float_t gust_ms) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_WIND);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (speed_ms < 0 || speed_ms > IOTDATA_WIND_SPEED_MAX)
        return IOTDATA_ERR_WIND_SPEED_HIGH;
    if (direction_deg > IOTDATA_WIND_DIRECTION_MAX)
        return IOTDATA_ERR_WIND_DIRECTION_HIGH;
    if (gust_ms < 0 || gust_ms > IOTDATA_WIND_SPEED_MAX)
        return IOTDATA_ERR_WIND_GUST_HIGH;
#endif
    enc->wind_speed = speed_ms;
    enc->wind_direction = direction_deg;
    enc->wind_gust = gust_ms;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_WIND);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_wind(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    pack_wind_speed(buf, bp, enc);
    pack_wind_direction(buf, bp, enc);
    pack_wind_gust(buf, bp, enc);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_wind(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    unpack_wind_speed(buf, bb, bp, out);
    unpack_wind_direction(buf, bb, bp, out);
    unpack_wind_gust(buf, bb, bp, out);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_wind(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_wind(enc, (iotdata_float_t)cJSON_GetObjectItem(j, "speed")->valuedouble, (uint16_t)cJSON_GetObjectItem(j, "direction")->valueint, (iotdata_float_t)cJSON_GetObjectItem(j, "gust")->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_wind(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    json_set_wind_speed(obj, d, "speed");
    json_set_wind_direction(obj, d, "direction");
    json_set_wind_gust(obj, d, "gust");
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_wind(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    n = dump_wind_speed(buf, bb, bp, dump, n, label);
    n = dump_wind_direction(buf, bb, bp, dump, n, label);
    n = dump_wind_gust(buf, bb, bp, dump, n, label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_wind(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.1f m/s, %u deg, gust %.1f m/s\n", l, _padd(l), d->wind_speed, d->wind_direction, d->wind_gust);
#else
    fprintf(fp, "  %s:%s %d.%02d m/s, %u deg, gust %d.%02d m/s\n", l, _padd(l), d->wind_speed / 100, d->wind_speed % 100, d->wind_direction, d->wind_gust / 100, d->wind_gust % 100);
#endif
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_wind = {
    _IOTDATA_OP_NAME("wind")
    _IOTDATA_OP_BITS(IOTDATA_WIND_SPEED_BITS + IOTDATA_WIND_DIRECTION_BITS + IOTDATA_WIND_GUST_BITS)
    _IOTDATA_OP_PACK(pack_wind)
    _IOTDATA_OP_UNPACK(unpack_wind)
    _IOTDATA_OP_DUMP(dump_wind)
    _IOTDATA_OP_PRINT(print_wind)
    _IOTDATA_OP_JSON_SET(json_set_wind)
    _IOTDATA_OP_JSON_GET(json_get_wind)
};
#define _IOTDATA_ENT_WIND [IOTDATA_FIELD_WIND] = &_iotdata_field_def_wind,
#define _IOTDATA_ERR_WIND
#else
#define _IOTDATA_ENT_WIND
#define _IOTDATA_ERR_WIND
#endif
// clang-format on

/* =========================================================================
 * Field RAIN, RAIN_RATE, RAIN_SIZE
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_RAIN_RATE) || defined(IOTDATA_ENABLE_RAIN)
#if defined(IOTDATA_ENABLE_RAIN_RATE) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_rain_rate(iotdata_encoder_t *enc, uint8_t rate_mmhr) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RAIN_RATE);
    enc->rain_rate = rate_mmhr;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RAIN_RATE);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_rain_rate(uint8_t rain_rate) {
    return (uint32_t)rain_rate;
}
static inline uint8_t dequantise_rain_rate(uint32_t raw) {
    return (uint8_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_rain_rate(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_rain_rate(enc->rain_rate), IOTDATA_RAIN_RATE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_rain_rate(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->rain_rate = dequantise_rain_rate(bits_read(buf, bb, bp, IOTDATA_RAIN_RATE_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_rain_rate(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_rain_rate(enc, (uint8_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_rain_rate(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->rain_rate);
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE) && !defined(IOTDATA_NO_DUMP)
static inline int dump_rain_rate(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_RAIN_RATE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u mm/hr", dequantise_rain_rate(r));
    n = dump_add(dump, n, s, IOTDATA_RAIN_RATE_BITS, r, dump->_dec_buf, "0..255 mm/hr", "rain_rate");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_rain_rate(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u mm/hr\n", l, _padd(l), d->rain_rate);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_RAIN_RATE)
static const iotdata_field_ops_t _iotdata_field_def_rain_rate = {
    _IOTDATA_OP_NAME("rain_rate")
    _IOTDATA_OP_BITS(IOTDATA_RAIN_RATE_BITS)
    _IOTDATA_OP_PACK(pack_rain_rate)
    _IOTDATA_OP_UNPACK(unpack_rain_rate)
    _IOTDATA_OP_DUMP(dump_rain_rate)
    _IOTDATA_OP_PRINT(print_rain_rate)
    _IOTDATA_OP_JSON_SET(json_set_rain_rate)
    _IOTDATA_OP_JSON_GET(json_get_rain_rate)
};
#define _IOTDATA_ENT_RAIN_RATE [IOTDATA_FIELD_RAIN_RATE] = &_iotdata_field_def_rain_rate,
#else
#define _IOTDATA_ENT_RAIN_RATE
#endif
#define _IOTDATA_ERR_RAIN_RATE \
    case IOTDATA_ERR_RAIN_RATE_HIGH: \
        return "Rain rate above 255 mm/hr";
#else
#define _IOTDATA_ENT_RAIN_RATE
#define _IOTDATA_ERR_RAIN_RATE
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_RAIN_SIZE) || defined(IOTDATA_ENABLE_RAIN)
#if defined(IOTDATA_ENABLE_RAIN_SIZE) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_rain_size(iotdata_encoder_t *enc, uint8_t size10_mmd) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RAIN_SIZE);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (size10_mmd > IOTDATA_RAIN_SIZE_MAX * IOTDATA_RAIN_SIZE_SCALE)
        return IOTDATA_ERR_RAIN_SIZE_HIGH;
#endif
    enc->rain_size10 = size10_mmd;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RAIN_SIZE);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_rain_size(uint8_t rain_size10) {
    return (uint32_t)(rain_size10 / IOTDATA_RAIN_SIZE_SCALE);
}
static inline uint8_t dequantise_rain_size(uint32_t raw) {
    return (uint8_t)(raw * IOTDATA_RAIN_SIZE_SCALE);
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_rain_size(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_rain_size(enc->rain_size10), IOTDATA_RAIN_SIZE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_rain_size(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->rain_size10 = dequantise_rain_size(bits_read(buf, bb, bp, IOTDATA_RAIN_SIZE_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_rain_size(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_rain_size(enc, (uint8_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_rain_size(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->rain_size10);
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE) && !defined(IOTDATA_NO_DUMP)
static inline int dump_rain_size(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_RAIN_SIZE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u.%u mm/d", dequantise_rain_size(r) / 10, dequantise_rain_size(r) % 10);
    n = dump_add(dump, n, s, IOTDATA_RAIN_SIZE_BITS, r, dump->_dec_buf, "0..6.3 mm/d", "rain_size");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_rain_size(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u.%u mm/d\n", l, _padd(l), d->rain_size10 / 10, d->rain_size10 % 10);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_RAIN_SIZE)
static const iotdata_field_ops_t _iotdata_field_def_rain_size = {
    _IOTDATA_OP_NAME("rain_size")
    _IOTDATA_OP_BITS(IOTDATA_RAIN_SIZE_BITS)
    _IOTDATA_OP_PACK(pack_rain_size)
    _IOTDATA_OP_UNPACK(unpack_rain_size)
    _IOTDATA_OP_DUMP(dump_rain_size)
    _IOTDATA_OP_PRINT(print_rain_size)
    _IOTDATA_OP_JSON_SET(json_set_rain_size)
    _IOTDATA_OP_JSON_GET(json_get_rain_size)
};
#define _IOTDATA_ENT_RAIN_SIZE [IOTDATA_FIELD_RAIN_SIZE] = &_iotdata_field_def_rain_size,
#else
#define _IOTDATA_ENT_RAIN_SIZE
#endif
#define _IOTDATA_ERR_RAIN_SIZE \
    case IOTDATA_ERR_RAIN_SIZE_HIGH: \
        return "Rain size above 6.0 mm/d";
#else
#define _IOTDATA_ENT_RAIN_SIZE
#define _IOTDATA_ERR_RAIN_SIZE
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_RAIN)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_rain(iotdata_encoder_t *enc, uint8_t rate_mmhr, uint8_t size10_mmd) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RAIN);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (size10_mmd > IOTDATA_RAIN_SIZE_MAX * IOTDATA_RAIN_SIZE_SCALE)
        return IOTDATA_ERR_RAIN_SIZE_HIGH;
#endif
    enc->rain_rate = rate_mmhr;
    enc->rain_size10 = size10_mmd;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RAIN);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_rain(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    pack_rain_rate(buf, bp, enc);
    pack_rain_size(buf, bp, enc);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_rain(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    unpack_rain_rate(buf, bb, bp, out);
    unpack_rain_size(buf, bb, bp, out);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_rain(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_rain(enc, (uint8_t)cJSON_GetObjectItem(j, "rate")->valueint, (uint8_t)cJSON_GetObjectItem(j, "size")->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_rain(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    json_set_rain_rate(obj, d, "rate");
    json_set_rain_size(obj, d, "size");
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_rain(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    n = dump_rain_rate(buf, bb, bp, dump, n, label);
    n = dump_rain_size(buf, bb, bp, dump, n, label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_rain(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u mm/hr, %u.%u mm/d\n", l, _padd(l), d->rain_rate, d->rain_size10 / 10, d->rain_size10 % 10);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_rain = {
    _IOTDATA_OP_NAME("rain")
    _IOTDATA_OP_BITS(IOTDATA_RAIN_RATE_BITS + IOTDATA_RAIN_SIZE_BITS)
    _IOTDATA_OP_PACK(pack_rain)
    _IOTDATA_OP_UNPACK(unpack_rain)
    _IOTDATA_OP_DUMP(dump_rain)
    _IOTDATA_OP_PRINT(print_rain)
    _IOTDATA_OP_JSON_SET(json_set_rain)
    _IOTDATA_OP_JSON_GET(json_get_rain)
};
#define _IOTDATA_ENT_RAIN [IOTDATA_FIELD_RAIN] = &_iotdata_field_def_rain,
#define _IOTDATA_ERR_RAIN
#else
#define _IOTDATA_ENT_RAIN
#define _IOTDATA_ERR_RAIN
#endif
// clang-format on

/* =========================================================================
 * Field SOLAR, SOLAR_IRRADIANCE, SOLAR_ULTRAVIOLET
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_SOLAR)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_solar(iotdata_encoder_t *enc, uint16_t irradiance_wm2, uint8_t ultraviolet_index) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_SOLAR);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (irradiance_wm2 > IOTDATA_SOLAR_IRRADIATION_MAX)
        return IOTDATA_ERR_SOLAR_IRRADIATION_HIGH;
    if (ultraviolet_index > IOTDATA_SOLAR_ULTRAVIOLET_MAX)
        return IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH;
#endif
    enc->solar_irradiance = irradiance_wm2;
    enc->solar_ultraviolet = ultraviolet_index;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_SOLAR);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_solar_irradiance(uint16_t solar_irradiance) {
    return (uint32_t)solar_irradiance;
}
static inline uint16_t dequantise_solar_irradiance(uint32_t raw) {
    return (uint16_t)raw;
}
static inline uint32_t quantise_solar_ultraviolet(uint8_t solar_ultraviolet) {
    return (uint32_t)solar_ultraviolet;
}
static inline uint8_t dequantise_solar_ultraviolet(uint32_t raw) {
    return (uint8_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_solar(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_solar_irradiance(enc->solar_irradiance), IOTDATA_SOLAR_IRRADIATION_BITS);
    bits_write(buf, bp, quantise_solar_ultraviolet(enc->solar_ultraviolet), IOTDATA_SOLAR_ULTRAVIOLET_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_solar(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->solar_irradiance = dequantise_solar_irradiance(bits_read(buf, bb, bp, IOTDATA_SOLAR_IRRADIATION_BITS));
    out->solar_ultraviolet = dequantise_solar_ultraviolet(bits_read(buf, bb, bp, IOTDATA_SOLAR_ULTRAVIOLET_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_solar(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_solar(enc, (uint16_t)cJSON_GetObjectItem(j, "irradiance")->valueint, (uint8_t)cJSON_GetObjectItem(j, "ultraviolet")->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_solar(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    cJSON_AddNumberToObject(obj, "irradiance", d->solar_irradiance);
    cJSON_AddNumberToObject(obj, "ultraviolet", d->solar_ultraviolet);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_solar(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_SOLAR_IRRADIATION_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u W/m2", dequantise_solar_irradiance(r));
    n = dump_add(dump, n, s, IOTDATA_SOLAR_IRRADIATION_BITS, r, dump->_dec_buf, "0..1023 W/m2", "solar_irradiance");
    s = *bp;
    r = bits_read(buf, bb, bp, IOTDATA_SOLAR_ULTRAVIOLET_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u", dequantise_solar_ultraviolet(r));
    n = dump_add(dump, n, s, IOTDATA_SOLAR_ULTRAVIOLET_BITS, r, dump->_dec_buf, "0..15", "solar_ultraviolet");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_solar(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u W/m2, UV %u\n", l, _padd(l), d->solar_irradiance, d->solar_ultraviolet);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_solar = {
    _IOTDATA_OP_NAME("solar")
    _IOTDATA_OP_BITS(IOTDATA_SOLAR_IRRADIATION_BITS + IOTDATA_SOLAR_ULTRAVIOLET_BITS)
    _IOTDATA_OP_PACK(pack_solar)
    _IOTDATA_OP_UNPACK(unpack_solar)
    _IOTDATA_OP_DUMP(dump_solar)
    _IOTDATA_OP_PRINT(print_solar)
    _IOTDATA_OP_JSON_SET(json_set_solar)
    _IOTDATA_OP_JSON_GET(json_get_solar)
};
#define _IOTDATA_ENT_SOLAR [IOTDATA_FIELD_SOLAR] = &_iotdata_field_def_solar,
#define _IOTDATA_ERR_SOLAR \
    case IOTDATA_ERR_SOLAR_IRRADIATION_HIGH: \
        return "Solar irradiance above 1023 W/m2"; \
    case IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH: \
        return "Solar ultraviolet index above 15";
#else
#define _IOTDATA_ENT_SOLAR
#define _IOTDATA_ERR_SOLAR
#endif
// clang-format on

/* =========================================================================
 * Field CLOUDS
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_CLOUDS)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_clouds(iotdata_encoder_t *enc, uint8_t okta) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_CLOUDS);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (okta > IOTDATA_CLOUDS_MAX)
        return IOTDATA_ERR_CLOUDS_HIGH;
#endif
    enc->clouds = okta;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_CLOUDS);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_clouds(uint8_t clouds) {
    return (uint32_t)clouds;
}
static inline uint8_t dequantise_clouds(uint32_t raw) {
    return (uint8_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_clouds(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_clouds(enc->clouds), IOTDATA_CLOUDS_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_clouds(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->clouds = dequantise_clouds(bits_read(buf, bb, bp, IOTDATA_CLOUDS_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_clouds(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_clouds(enc, (uint8_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_clouds(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->clouds);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_clouds(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_CLOUDS_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u okta", dequantise_clouds(r));
    n = dump_add(dump, n, s, IOTDATA_CLOUDS_BITS, r, dump->_dec_buf, "0..8 okta", "clouds");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_clouds(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u okta\n", l, _padd(l), d->clouds);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_clouds = {
    _IOTDATA_OP_NAME("clouds")
    _IOTDATA_OP_BITS(IOTDATA_CLOUDS_BITS)
    _IOTDATA_OP_PACK(pack_clouds)
    _IOTDATA_OP_UNPACK(unpack_clouds)
    _IOTDATA_OP_DUMP(dump_clouds)
    _IOTDATA_OP_PRINT(print_clouds)
    _IOTDATA_OP_JSON_SET(json_set_clouds)
_IOTDATA_OP_JSON_GET(json_get_clouds)
};
#define _IOTDATA_ENT_CLOUDS [IOTDATA_FIELD_CLOUDS] = &_iotdata_field_def_clouds,
#define _IOTDATA_ERR_CLOUDS \
    case IOTDATA_ERR_CLOUDS_HIGH: \
        return "Cloud cover above 8 okta";
#else
#define _IOTDATA_ENT_CLOUDS
#define _IOTDATA_ERR_CLOUDS
#endif
// clang-format on

/* =========================================================================
 * Field AIR_QUALITY, AIR_QUALITY_INDEX, AIR_QUALITY_PM, AIR_QUALITY_GAS
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM) || defined(IOTDATA_ENABLE_AIR_QUALITY)
static const char *_aq_pm_names[IOTDATA_AIR_QUALITY_PM_COUNT] = { "pm1", "pm25", "pm4", "pm10" };
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static const char *_aq_pm_labels[IOTDATA_AIR_QUALITY_PM_COUNT] = { "PM1", "PM2.5", "PM4", "PM10" };
#endif
#endif

#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS) || defined(IOTDATA_ENABLE_AIR_QUALITY)
static const uint8_t _aq_gas_bits[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    IOTDATA_AIR_QUALITY_GAS_BITS_VOC,  IOTDATA_AIR_QUALITY_GAS_BITS_NOX, IOTDATA_AIR_QUALITY_GAS_BITS_CO2,   IOTDATA_AIR_QUALITY_GAS_BITS_CO,
    IOTDATA_AIR_QUALITY_GAS_BITS_HCHO, IOTDATA_AIR_QUALITY_GAS_BITS_O3,  IOTDATA_AIR_QUALITY_GAS_BITS_RSVD6, IOTDATA_AIR_QUALITY_GAS_BITS_RSVD7,
};
static const uint16_t _aq_gas_res[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    IOTDATA_AIR_QUALITY_GAS_RES_VOC,  IOTDATA_AIR_QUALITY_GAS_RES_NOX, IOTDATA_AIR_QUALITY_GAS_RES_CO2,   IOTDATA_AIR_QUALITY_GAS_RES_CO,
    IOTDATA_AIR_QUALITY_GAS_RES_HCHO, IOTDATA_AIR_QUALITY_GAS_RES_O3,  IOTDATA_AIR_QUALITY_GAS_RES_RSVD6, IOTDATA_AIR_QUALITY_GAS_RES_RSVD7,
};
#if !defined(IOTDATA_NO_ENCODE)
static const uint16_t _aq_gas_max[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    IOTDATA_AIR_QUALITY_GAS_MAX_VOC,  IOTDATA_AIR_QUALITY_GAS_MAX_NOX, IOTDATA_AIR_QUALITY_GAS_MAX_CO2,   IOTDATA_AIR_QUALITY_GAS_MAX_CO,
    IOTDATA_AIR_QUALITY_GAS_MAX_HCHO, IOTDATA_AIR_QUALITY_GAS_MAX_O3,  IOTDATA_AIR_QUALITY_GAS_MAX_RSVD6, IOTDATA_AIR_QUALITY_GAS_MAX_RSVD7,
};
#endif
static const char *_aq_gas_names[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    "voc", "nox", "co2", "co", "hcho", "o3", "rsvd6", "rsvd7",
};
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static const char *_aq_gas_labels[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    "VOC", "NOx", "CO2", "CO", "HCHO", "O3", "rsvd6", "rsvd7",
};
#endif
static const char *_aq_gas_units[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    "idx", "idx", "ppm", "ppm", "ppb", "ppb", "", "",
};
#if !defined(IOTDATA_NO_DUMP)
static const char *_aq_gas_range[IOTDATA_AIR_QUALITY_GAS_COUNT] = {
    "0..510, 2 idx", "0..510, 2 idx", "0..51150, 50 ppm", "0..1023, 1 ppm", "0..5115, 5 ppb", "0..1023, 1 ppb", "reserved", "reserved",
};
#endif
#endif

#if defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX) || defined(IOTDATA_ENABLE_AIR_QUALITY)
#if defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_air_quality_index(iotdata_encoder_t *enc, uint16_t aq_index) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_AIR_QUALITY_INDEX);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (aq_index > IOTDATA_AIR_QUALITY_INDEX_MAX)
        return IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH;
#endif
    enc->aq_index = aq_index;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_AIR_QUALITY_INDEX);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_aq_index(uint16_t v) {
    return (uint32_t)v;
}
static inline uint16_t dequantise_aq_index(uint32_t r) {
    return (uint16_t)r;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_aq_index(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_aq_index(enc->aq_index), IOTDATA_AIR_QUALITY_INDEX_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_aq_index(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->aq_index = dequantise_aq_index(bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_INDEX_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_aq_index(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->aq_index);
}
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_aq_index(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_air_quality_index(enc, (uint16_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_aq_index(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_INDEX_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u AQI", dequantise_aq_index(r));
    n = dump_add(dump, n, s, IOTDATA_AIR_QUALITY_INDEX_BITS, r, dump->_dec_buf, "0..500 AQI", "aq_index");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_aq_index(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u AQI\n", l, _padd(l), d->aq_index);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX)
static const iotdata_field_ops_t _iotdata_field_def_aq_index = {
    _IOTDATA_OP_NAME("air_quality_index")
    _IOTDATA_OP_BITS(IOTDATA_AIR_QUALITY_INDEX_BITS)
    _IOTDATA_OP_PACK(pack_aq_index)
    _IOTDATA_OP_UNPACK(unpack_aq_index)
    _IOTDATA_OP_DUMP(dump_aq_index)
    _IOTDATA_OP_PRINT(print_aq_index)
    _IOTDATA_OP_JSON_SET(json_set_aq_index)
    _IOTDATA_OP_JSON_GET(json_get_aq_index)
};
#define _IOTDATA_ENT_AIR_QUALITY_INDEX [IOTDATA_FIELD_AIR_QUALITY_INDEX] = &_iotdata_field_def_aq_index,
#else
#define _IOTDATA_ENT_AIR_QUALITY_INDEX
#endif
#define _IOTDATA_ERR_AIR_QUALITY_INDEX \
    case IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH: \
        return "AQ index above 500 AQI";
#else
#define _IOTDATA_ENT_AIR_QUALITY_INDEX
#define _IOTDATA_ERR_AIR_QUALITY_INDEX
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM) || defined(IOTDATA_ENABLE_AIR_QUALITY)
#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_air_quality_pm(iotdata_encoder_t *enc, uint8_t pm_present, const uint16_t pm[4]) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_AIR_QUALITY_PM);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if ((pm_present & (1U << i)) && pm[i] > IOTDATA_AIR_QUALITY_PM_VALUE_MAX)
            return IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH;
#endif
    enc->aq_pm_present = pm_present & 0x0F;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        enc->aq_pm[i] = pm[i];
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_AIR_QUALITY_PM);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_aq_pm(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, enc->aq_pm_present, IOTDATA_AIR_QUALITY_PM_PRESENT_BITS);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if (enc->aq_pm_present & (1U << i))
            bits_write(buf, bp, enc->aq_pm[i] / IOTDATA_AIR_QUALITY_PM_VALUE_RES, IOTDATA_AIR_QUALITY_PM_VALUE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_aq_pm(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->aq_pm_present = (uint8_t)bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_PM_PRESENT_BITS);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        out->aq_pm[i] = out->aq_pm_present & (1U << i) ? (uint16_t)(bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_PM_VALUE_BITS) * IOTDATA_AIR_QUALITY_PM_VALUE_RES) : 0;
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_aq_pm(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if (d->aq_pm_present & (1U << i))
            cJSON_AddNumberToObject(obj, _aq_pm_names[i], d->aq_pm[i]);
}
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_aq_pm(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    uint8_t present = 0;
    uint16_t pm[IOTDATA_AIR_QUALITY_PM_COUNT] = { 0 };
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++) {
        cJSON *v = cJSON_GetObjectItem(j, _aq_pm_names[i]);
        if (v) {
            present |= (1U << i);
            pm[i] = (uint16_t)v->valueint;
        }
    }
    return iotdata_encode_air_quality_pm(enc, present, pm);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_aq_pm(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t present = bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_PM_PRESENT_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%X", (unsigned)present);
    n = dump_add(dump, n, s, IOTDATA_AIR_QUALITY_PM_PRESENT_BITS, present, dump->_dec_buf, "4-bit mask", "aq_pm_present");
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if (present & (1U << i)) {
            s = *bp;
            uint32_t r = bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_PM_VALUE_BITS);
            snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u ug/m3", (unsigned)(r * IOTDATA_AIR_QUALITY_PM_VALUE_RES));
            n = dump_add(dump, n, s, IOTDATA_AIR_QUALITY_PM_VALUE_BITS, r, dump->_dec_buf, "0..1275, 5 ug/m3", _aq_pm_names[i]);
        }
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_aq_pm(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s", l, _padd(l));
    for (int i = 0, first = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if (d->aq_pm_present & (1U << i))
            fprintf(fp, "%s %s=%u", first++ ? "," : "", _aq_pm_labels[i], d->aq_pm[i]);
    fprintf(fp, "%s\n", d->aq_pm_present ? " ug/m3" : "");
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM)
static const iotdata_field_ops_t _iotdata_field_def_aq_pm = {
    _IOTDATA_OP_NAME("air_quality_pm")
    _IOTDATA_OP_BITS(0)
    _IOTDATA_OP_PACK(pack_aq_pm)
    _IOTDATA_OP_UNPACK(unpack_aq_pm)
    _IOTDATA_OP_DUMP(dump_aq_pm)
    _IOTDATA_OP_PRINT(print_aq_pm)
    _IOTDATA_OP_JSON_SET(json_set_aq_pm)
    _IOTDATA_OP_JSON_GET(json_get_aq_pm)
};
#define _IOTDATA_ENT_AIR_QUALITY_PM [IOTDATA_FIELD_AIR_QUALITY_PM] = &_iotdata_field_def_aq_pm,
#else
#define _IOTDATA_ENT_AIR_QUALITY_PM
#endif
#define _IOTDATA_ERR_AIR_QUALITY_PM \
    case IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH: \
        return "AQ PM value above 1275 ug/m3";
#else
#define _IOTDATA_ENT_AIR_QUALITY_PM
#define _IOTDATA_ERR_AIR_QUALITY_PM
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS) || defined(IOTDATA_ENABLE_AIR_QUALITY)
#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_air_quality_gas(iotdata_encoder_t *enc, uint8_t gas_present, const uint16_t gas[8]) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_AIR_QUALITY_GAS);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        if ((gas_present & (1U << i)) && gas[i] > _aq_gas_max[i])
            return IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH;
#endif
    enc->aq_gas_present = gas_present;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        enc->aq_gas[i] = gas[i];
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_AIR_QUALITY_GAS);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_aq_gas(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, enc->aq_gas_present, IOTDATA_AIR_QUALITY_GAS_PRESENT_BITS);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        if (enc->aq_gas_present & (1U << i))
            bits_write(buf, bp, enc->aq_gas[i] / _aq_gas_res[i], _aq_gas_bits[i]);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_aq_gas(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->aq_gas_present = (uint8_t)bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_GAS_PRESENT_BITS);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        out->aq_gas[i] = out->aq_gas_present & (1U << i) ? (uint16_t)(bits_read(buf, bb, bp, _aq_gas_bits[i]) * _aq_gas_res[i]) : 0;
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_aq_gas(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        if (d->aq_gas_present & (1U << i))
            cJSON_AddNumberToObject(obj, _aq_gas_names[i], d->aq_gas[i]);
}
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_aq_gas(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    uint8_t present = 0;
    uint16_t gas[IOTDATA_AIR_QUALITY_GAS_COUNT] = { 0 };
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++) {
        cJSON *v = cJSON_GetObjectItem(j, _aq_gas_names[i]);
        if (v) {
            present |= (1U << i);
            gas[i] = (uint16_t)v->valueint;
        }
    }
    return iotdata_encode_air_quality_gas(enc, present, gas);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_aq_gas(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t present = bits_read(buf, bb, bp, IOTDATA_AIR_QUALITY_GAS_PRESENT_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02X", (unsigned)present);
    n = dump_add(dump, n, s, IOTDATA_AIR_QUALITY_GAS_PRESENT_BITS, present, dump->_dec_buf, "8-bit mask", "aq_gas_present");
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++) {
        if (present & (1U << i)) {
            s = *bp;
            uint32_t r = bits_read(buf, bb, bp, _aq_gas_bits[i]);
            snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u %s", r * _aq_gas_res[i], _aq_gas_units[i]);
            n = dump_add(dump, n, s, _aq_gas_bits[i], r, dump->_dec_buf, _aq_gas_range[i], _aq_gas_names[i]);
        }
    }
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_aq_gas(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s", l, _padd(l));
    for (int i = 0, first = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        if (d->aq_gas_present & (1U << i))
            fprintf(fp, "%s %s=%u%s%s", first++ ? "," : "", _aq_gas_labels[i], d->aq_gas[i], _aq_gas_units[i][0] ? " " : "", _aq_gas_units[i][0] ? _aq_gas_units[i] : "");
    fprintf(fp, "\n");
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS)
static const iotdata_field_ops_t _iotdata_field_def_aq_gas = {
    _IOTDATA_OP_NAME("air_quality_gas")
    _IOTDATA_OP_BITS(0)
    _IOTDATA_OP_PACK(pack_aq_gas)
    _IOTDATA_OP_UNPACK(unpack_aq_gas)
    _IOTDATA_OP_DUMP(dump_aq_gas)
    _IOTDATA_OP_PRINT(print_aq_gas)
    _IOTDATA_OP_JSON_SET(json_set_aq_gas)
    _IOTDATA_OP_JSON_GET(json_get_aq_gas)
};
#define _IOTDATA_ENT_AIR_QUALITY_GAS [IOTDATA_FIELD_AIR_QUALITY_GAS] = &_iotdata_field_def_aq_gas,
#else
#define _IOTDATA_ENT_AIR_QUALITY_GAS
#endif
#define _IOTDATA_ERR_AIR_QUALITY_GAS \
    case IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH: \
        return "AQ gas value above slot maximum";
#else
#define _IOTDATA_ENT_AIR_QUALITY_GAS
#define _IOTDATA_ERR_AIR_QUALITY_GAS
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_AIR_QUALITY)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_air_quality(iotdata_encoder_t *enc, uint16_t aq_index, uint8_t pm_present, const uint16_t pm[4], uint8_t gas_present, const uint16_t gas[8]) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_AIR_QUALITY);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (aq_index > IOTDATA_AIR_QUALITY_INDEX_MAX)
        return IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        if ((pm_present & (1U << i)) && pm[i] > IOTDATA_AIR_QUALITY_PM_VALUE_MAX)
            return IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        if ((gas_present & (1U << i)) && gas[i] > _aq_gas_max[i])
            return IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH;
#endif
    enc->aq_index = aq_index;
    enc->aq_pm_present = pm_present & 0x0F;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++)
        enc->aq_pm[i] = pm[i];
    enc->aq_gas_present = gas_present;
    for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++)
        enc->aq_gas[i] = gas[i];
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_AIR_QUALITY);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_air_quality(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    pack_aq_index(buf, bp, enc);
    pack_aq_pm(buf, bp, enc);
    pack_aq_gas(buf, bp, enc);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_air_quality(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    unpack_aq_index(buf, bb, bp, out);
    unpack_aq_pm(buf, bb, bp, out);
    unpack_aq_gas(buf, bb, bp, out);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_air_quality(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    /* Extract index */
    uint16_t idx = 0;
    cJSON *ji = cJSON_GetObjectItem(j, "index");
    if (ji)
        idx = (uint16_t)ji->valueint;
    /* Extract PM */
    uint8_t pm_present = 0;
    uint16_t pm[IOTDATA_AIR_QUALITY_PM_COUNT] = { 0 };
    cJSON *jp = cJSON_GetObjectItem(j, "pm");
    if (jp)
        for (int i = 0; i < IOTDATA_AIR_QUALITY_PM_COUNT; i++) {
            cJSON *v = cJSON_GetObjectItem(jp, _aq_pm_names[i]);
            if (v) {
                pm_present |= (1U << i);
                pm[i] = (uint16_t)v->valueint;
            }
        }
    /* Extract gas */
    uint8_t gas_present = 0;
    uint16_t gas[IOTDATA_AIR_QUALITY_GAS_COUNT] = { 0 };
    cJSON *jg = cJSON_GetObjectItem(j, "gas");
    if (jg)
        for (int i = 0; i < IOTDATA_AIR_QUALITY_GAS_COUNT; i++) {
            cJSON *v = cJSON_GetObjectItem(jg, _aq_gas_names[i]);
            if (v) {
                gas_present |= (1U << i);
                gas[i] = (uint16_t)v->valueint;
            }
        }
    return iotdata_encode_air_quality(enc, idx, pm_present, pm, gas_present, gas);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_air_quality(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    json_set_aq_index(obj, d, "index");
    json_set_aq_pm(obj, d, "pm");
    json_set_aq_gas(obj, d, "gas");
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_air_quality(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    n = dump_aq_index(buf, bb, bp, dump, n, label);
    n = dump_aq_pm(buf, bb, bp, dump, n, label);
    n = dump_aq_gas(buf, bb, bp, dump, n, label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_air_quality(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    print_aq_index(d, fp, l);
    print_aq_pm(d, fp, l);
    print_aq_gas(d, fp, l);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_air_quality = {
    _IOTDATA_OP_NAME("air_quality")
    _IOTDATA_OP_BITS(0)
    _IOTDATA_OP_PACK(pack_air_quality)
    _IOTDATA_OP_UNPACK(unpack_air_quality)
    _IOTDATA_OP_DUMP(dump_air_quality)
    _IOTDATA_OP_PRINT(print_air_quality)
    _IOTDATA_OP_JSON_SET(json_set_air_quality)
    _IOTDATA_OP_JSON_GET(json_get_air_quality)
};
#define _IOTDATA_ENT_AIR_QUALITY [IOTDATA_FIELD_AIR_QUALITY] = &_iotdata_field_def_air_quality,
#define _IOTDATA_ERR_AIR_QUALITY
#else
#define _IOTDATA_ENT_AIR_QUALITY
#define _IOTDATA_ERR_AIR_QUALITY
#endif
// clang-format on

/* =========================================================================
 * Field RADIATION, RADIATION_CPM, RADIATION_DOSE
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_RADIATION_CPM) || defined(IOTDATA_ENABLE_RADIATION)
#if defined(IOTDATA_ENABLE_RADIATION_CPM) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_radiation_cpm(iotdata_encoder_t *enc, uint16_t cpm) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RADIATION_CPM);
    enc->radiation_cpm = cpm;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RADIATION_CPM);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_radiation_cpm(uint16_t cpm) {
    return (uint32_t)cpm;
}
static inline uint16_t dequantise_radiation_cpm(uint32_t raw) {
    return (uint16_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_radiation_cpm(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_radiation_cpm(enc->radiation_cpm), IOTDATA_RADIATION_CPM_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_radiation_cpm(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->radiation_cpm = dequantise_radiation_cpm(bits_read(buf, bb, bp, IOTDATA_RADIATION_CPM_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_radiation_cpm(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_radiation_cpm(enc, (uint16_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_radiation_cpm(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->radiation_cpm);
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM) && !defined(IOTDATA_NO_DUMP)
static inline int dump_radiation_cpm(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_RADIATION_CPM_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u CPM", dequantise_radiation_cpm(r));
    n = dump_add(dump, n, s, IOTDATA_RADIATION_CPM_BITS, r, dump->_dec_buf, "0..65535 CPM", "radiation_cpm");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_radiation_cpm(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u CPM\n", l, _padd(l), d->radiation_cpm);
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_RADIATION_CPM) 
static const iotdata_field_ops_t _iotdata_field_def_radiation_cpm = {
    _IOTDATA_OP_NAME("radiation_cpm")
    _IOTDATA_OP_BITS(IOTDATA_RADIATION_CPM_BITS)
    _IOTDATA_OP_PACK(pack_radiation_cpm)
    _IOTDATA_OP_UNPACK(unpack_radiation_cpm)
    _IOTDATA_OP_DUMP(dump_radiation_cpm)
    _IOTDATA_OP_PRINT(print_radiation_cpm)
    _IOTDATA_OP_JSON_SET(json_set_radiation_cpm)
    _IOTDATA_OP_JSON_GET(json_get_radiation_cpm)
};
#define _IOTDATA_ENT_RADIATION_CPM [IOTDATA_FIELD_RADIATION_CPM] = &_iotdata_field_def_radiation_cpm,
#else
#define _IOTDATA_ENT_RADIATION_CPM
#endif
#define _IOTDATA_ERR_RADIATION_CPM \
    case IOTDATA_ERR_RADIATION_CPM_HIGH: \
        return "Radiation CPM above 65535";
#else
#define _IOTDATA_ENT_RADIATION_CPM
#define _IOTDATA_ERR_RADIATION_CPM
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_RADIATION_DOSE) || defined(IOTDATA_ENABLE_RADIATION)
#if defined(IOTDATA_ENABLE_RADIATION_DOSE) && !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_radiation_dose(iotdata_encoder_t *enc, iotdata_float_t usvh) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RADIATION_DOSE);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (usvh < 0 || usvh > IOTDATA_RADIATION_DOSE_MAX)
        return IOTDATA_ERR_RADIATION_DOSE_HIGH;
#endif
    enc->radiation_dose = usvh;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RADIATION_DOSE);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_FLOATING)
static inline uint32_t quantise_radiation_dose(float dose) {
    return (uint32_t)roundf(dose / IOTDATA_RADIATION_DOSE_RES);
}
static inline float dequantise_radiation_dose(uint32_t raw) {
    return (float)raw * IOTDATA_RADIATION_DOSE_RES;
}
#else
static inline uint32_t quantise_radiation_dose(int32_t dose100) {
    return (uint32_t)dose100;
}
static inline int32_t dequantise_radiation_dose(uint32_t raw) {
    return (int32_t)raw;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_radiation_dose(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_radiation_dose(enc->radiation_dose), IOTDATA_RADIATION_DOSE_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_radiation_dose(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->radiation_dose = dequantise_radiation_dose(bits_read(buf, bb, bp, IOTDATA_RADIATION_DOSE_BITS));
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE) && !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_radiation_dose(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_radiation_dose(enc, (iotdata_float_t)j->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_radiation_dose(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->radiation_dose);
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE) && !defined(IOTDATA_NO_DUMP)
static inline int dump_radiation_dose(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_RADIATION_DOSE_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.2f uSv/h", dequantise_radiation_dose(r));
#else
    fmt_scaled100(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_radiation_dose(r), "uSv/h");
#endif
    n = dump_add(dump, n, s, IOTDATA_RADIATION_DOSE_BITS, r, dump->_dec_buf, "0..163.83, 0.01", "radiation_dose");
    return n;
}
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE) && !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_radiation_dose(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.1f uSv/h\n", l, _padd(l), d->radiation_dose);
#else
    fprintf(fp, "  %s:%s %d.%02d uSv/h\n", l, _padd(l), d->radiation_dose / 100, d->radiation_dose % 100);
#endif
}
#endif
// clang-format off
#if defined(IOTDATA_ENABLE_RADIATION_DOSE)
static const iotdata_field_ops_t _iotdata_field_def_radiation_dose = {
    _IOTDATA_OP_NAME("radiation_dose")
    _IOTDATA_OP_BITS(IOTDATA_RADIATION_DOSE_BITS)
    _IOTDATA_OP_PACK(pack_radiation_dose)
    _IOTDATA_OP_UNPACK(unpack_radiation_dose)
    _IOTDATA_OP_DUMP(dump_radiation_dose)
    _IOTDATA_OP_PRINT(print_radiation_dose)
    _IOTDATA_OP_JSON_SET(json_set_radiation_dose)
    _IOTDATA_OP_JSON_GET(json_get_radiation_dose)
};
#define _IOTDATA_ENT_RADIATION_DOSE [IOTDATA_FIELD_RADIATION_DOSE] = &_iotdata_field_def_radiation_dose,
#else
#define _IOTDATA_ENT_RADIATION_DOSE
#endif
#define _IOTDATA_ERR_RADIATION_DOSE \
    case IOTDATA_ERR_RADIATION_DOSE_HIGH: \
        return "Radiation dose above 163.83 uSv/h";
#else
#define _IOTDATA_ENT_RADIATION_DOSE
#define _IOTDATA_ERR_RADIATION_DOSE
#endif
// clang-format on

#if defined(IOTDATA_ENABLE_RADIATION)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_radiation(iotdata_encoder_t *enc, uint16_t cpm, iotdata_float_t usvh) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_RADIATION);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (usvh < 0 || usvh > IOTDATA_RADIATION_DOSE_MAX)
        return IOTDATA_ERR_RADIATION_DOSE_HIGH;
#endif
    enc->radiation_cpm = cpm;
    enc->radiation_dose = usvh;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_RADIATION);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_radiation(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    pack_radiation_cpm(buf, bp, enc);
    pack_radiation_dose(buf, bp, enc);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_radiation(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    unpack_radiation_cpm(buf, bb, bp, out);
    unpack_radiation_dose(buf, bb, bp, out);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_radiation(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_radiation(enc, (uint16_t)cJSON_GetObjectItem(j, "cpm")->valueint, (iotdata_float_t)cJSON_GetObjectItem(j, "dose")->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_radiation(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    json_set_radiation_cpm(obj, d, "cpm");
    json_set_radiation_dose(obj, d, "dose");
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_radiation(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    n = dump_radiation_cpm(buf, bb, bp, dump, n, label);
    n = dump_radiation_dose(buf, bb, bp, dump, n, label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_radiation(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %u CPM, %.2f uSv/h\n", l, _padd(l), d->radiation_cpm, d->radiation_dose);
#else
    fprintf(fp, "  %s:%s %u CPM, %d.%02d uSv/h\n", l, _padd(l), d->radiation_cpm, d->radiation_dose / 100, d->radiation_dose % 100);
#endif
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_radiation = {
    _IOTDATA_OP_NAME("radiation")
    _IOTDATA_OP_BITS(IOTDATA_RADIATION_CPM_BITS + IOTDATA_RADIATION_DOSE_BITS)
    _IOTDATA_OP_PACK(pack_radiation)
    _IOTDATA_OP_UNPACK(unpack_radiation)
    _IOTDATA_OP_DUMP(dump_radiation)
    _IOTDATA_OP_PRINT(print_radiation)
    _IOTDATA_OP_JSON_SET(json_set_radiation)
    _IOTDATA_OP_JSON_GET(json_get_radiation)
};
#define _IOTDATA_ENT_RADIATION [IOTDATA_FIELD_RADIATION] = &_iotdata_field_def_radiation,
#define _IOTDATA_ERR_RADIATION
#else
#define _IOTDATA_ENT_RADIATION
#define _IOTDATA_ERR_RADIATION
#endif
// clang-format on

/* =========================================================================
 * Field DEPTH
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_DEPTH)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_depth(iotdata_encoder_t *enc, uint16_t depth_cm) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_DEPTH);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (depth_cm > IOTDATA_DEPTH_MAX)
        return IOTDATA_ERR_DEPTH_HIGH;
#endif
    enc->depth = depth_cm;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_DEPTH);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_depth(uint16_t depth) {
    return (uint32_t)depth;
}
static inline uint16_t dequantise_depth(uint32_t raw) {
    return (uint16_t)raw;
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_depth(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_depth(enc->depth), IOTDATA_DEPTH_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_depth(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->depth = dequantise_depth(bits_read(buf, bb, bp, IOTDATA_DEPTH_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_depth(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_depth(enc, (uint16_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_depth(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->depth);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_depth(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_DEPTH_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u cm", dequantise_depth(r));
    n = dump_add(dump, n, s, IOTDATA_DEPTH_BITS, r, dump->_dec_buf, "0..1023 cm", label);
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_depth(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %u cm\n", l, _padd(l), d->depth);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_depth = {
    _IOTDATA_OP_NAME("depth")
    _IOTDATA_OP_BITS(IOTDATA_DEPTH_BITS)
    _IOTDATA_OP_PACK(pack_depth)
    _IOTDATA_OP_UNPACK(unpack_depth)
    _IOTDATA_OP_DUMP(dump_depth)
    _IOTDATA_OP_PRINT(print_depth)
    _IOTDATA_OP_JSON_SET(json_set_depth)
    _IOTDATA_OP_JSON_GET(json_get_depth)
};
#define _IOTDATA_ENT_DEPTH [IOTDATA_FIELD_DEPTH] = &_iotdata_field_def_depth,
#define _IOTDATA_ERR_DEPTH \
    case IOTDATA_ERR_DEPTH_HIGH: \
        return "Depth above 1023 cm";
#else
#define _IOTDATA_ENT_DEPTH
#define _IOTDATA_ERR_DEPTH
#endif
// clang-format on

/* =========================================================================
 * Field POSITION (LATITUDE, LONGITUDE)
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_POSITION)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_position(iotdata_encoder_t *enc, iotdata_double_t latitude, iotdata_double_t longitude) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_POSITION);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (latitude < IOTDATA_POS_LAT_LOW)
        return IOTDATA_ERR_POSITION_LAT_LOW;
    if (latitude > IOTDATA_POS_LAT_HIGH)
        return IOTDATA_ERR_POSITION_LAT_HIGH;
    if (longitude < IOTDATA_POS_LON_LOW)
        return IOTDATA_ERR_POSITION_LON_LOW;
    if (longitude > IOTDATA_POS_LON_HIGH)
        return IOTDATA_ERR_POSITION_LON_HIGH;
#endif
    enc->position_lat = latitude;
    enc->position_lon = longitude;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_POSITION);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_FLOATING)
static inline uint32_t quantise_position_lat(iotdata_double_t lat) {
    return (uint32_t)round((lat - (iotdata_double_t)(-90.0)) / (iotdata_double_t)180.0 * (iotdata_double_t)IOTDATA_POS_SCALE);
}
static inline iotdata_double_t dequantise_position_lat(uint32_t raw) {
    return (iotdata_double_t)raw / (iotdata_double_t)IOTDATA_POS_SCALE * (iotdata_double_t)180.0 + (iotdata_double_t)(-90.0);
}
static inline uint32_t quantise_position_lon(iotdata_double_t lon) {
    return (uint32_t)round((lon - (iotdata_double_t)(-180.0)) / (iotdata_double_t)360.0 * (iotdata_double_t)IOTDATA_POS_SCALE);
}
static inline iotdata_double_t dequantise_position_lon(uint32_t raw) {
    return (iotdata_double_t)raw / (iotdata_double_t)IOTDATA_POS_SCALE * (iotdata_double_t)360.0 + (iotdata_double_t)(-180.0);
}
#else
static inline uint32_t quantise_position_lat(int32_t lat7) {
    return (uint32_t)((((int64_t)lat7 + 900000000LL) * IOTDATA_POS_SCALE + 900000000LL) / 1800000000LL);
}
static inline int32_t dequantise_position_lat(uint32_t raw) {
    return (int32_t)(((int64_t)raw * 1800000000LL + IOTDATA_POS_SCALE / 2) / IOTDATA_POS_SCALE - 900000000LL);
}
static inline uint32_t quantise_position_lon(int32_t lon7) {
    return (uint32_t)((((int64_t)lon7 + 1800000000LL) * IOTDATA_POS_SCALE + 1800000000LL) / 3600000000LL);
}
static inline int32_t dequantise_position_lon(uint32_t raw) {
    return (int32_t)(((int64_t)raw * 3600000000LL + IOTDATA_POS_SCALE / 2) / IOTDATA_POS_SCALE - 1800000000LL);
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_position(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_position_lat(enc->position_lat), IOTDATA_POS_LAT_BITS);
    bits_write(buf, bp, quantise_position_lon(enc->position_lon), IOTDATA_POS_LON_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_position(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->position_lat = dequantise_position_lat(bits_read(buf, bb, bp, IOTDATA_POS_LAT_BITS));
    out->position_lon = dequantise_position_lon(bits_read(buf, bb, bp, IOTDATA_POS_LON_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_position(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_position(enc, (iotdata_double_t)cJSON_GetObjectItem(j, "latitude")->valuedouble, (iotdata_double_t)cJSON_GetObjectItem(j, "longitude")->valuedouble);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_position(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    cJSON_AddNumberToObject(obj, "latitude", d->position_lat);
    cJSON_AddNumberToObject(obj, "longitude", d->position_lon);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_position(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_POS_LAT_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.6f", dequantise_position_lat(r));
#else
    fmt_scaled10000000(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_position_lat(r), "");
#endif
    n = dump_add(dump, n, s, IOTDATA_POS_LAT_BITS, r, dump->_dec_buf, "-90..+90", "latitude");
    s = *bp;
    r = bits_read(buf, bb, bp, IOTDATA_POS_LON_BITS);
#if !defined(IOTDATA_NO_FLOATING)
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%.6f", dequantise_position_lon(r));
#else
    fmt_scaled10000000(dump->_dec_buf, sizeof(dump->_dec_buf), dequantise_position_lon(r), "");
#endif
    n = dump_add(dump, n, s, IOTDATA_POS_LON_BITS, r, dump->_dec_buf, "-180..+180", "longitude");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_position(const iotdata_decoded_t *d, FILE *fp, const char *l) {
#if !defined(IOTDATA_NO_FLOATING)
    fprintf(fp, "  %s:%s %.6f, %.6f\n", l, _padd(l), d->position_lat, d->position_lon);
#else
    const int32_t lat = d->position_lat, la = lat < 0 ? -lat : lat;
    const int32_t lon = d->position_lon, lo = lon < 0 ? -lon : lon;
    fprintf(fp, "  %s:%s %s%d.%06d, %s%d.%06d\n", l, _padd(l), lat < 0 ? "-" : "", (int)(la / 10000000), (int)(la % 10000000), lon < 0 ? "-" : "", (int)(lo / 10000000), (int)(lo % 10000000));
#endif
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_position = {
    _IOTDATA_OP_NAME("position")
    _IOTDATA_OP_BITS(IOTDATA_POS_LAT_BITS + IOTDATA_POS_LON_BITS)
    _IOTDATA_OP_PACK(pack_position)
    _IOTDATA_OP_UNPACK(unpack_position)
    _IOTDATA_OP_DUMP(dump_position)
    _IOTDATA_OP_PRINT(print_position)
    _IOTDATA_OP_JSON_SET(json_set_position)
    _IOTDATA_OP_JSON_GET(json_get_position)
};
#define _IOTDATA_ENT_POSITION [IOTDATA_FIELD_POSITION] = &_iotdata_field_def_position,
#define _IOTDATA_ERR_POSITION \
    case IOTDATA_ERR_POSITION_LAT_LOW: \
        return "Latitude below -90"; \
    case IOTDATA_ERR_POSITION_LAT_HIGH: \
        return "Latitude above +90"; \
    case IOTDATA_ERR_POSITION_LON_LOW: \
        return "Longitude below -180"; \
    case IOTDATA_ERR_POSITION_LON_HIGH: \
        return "Longitude above +180";
#else
#define _IOTDATA_ENT_POSITION
#define _IOTDATA_ERR_POSITION
#endif
// clang-format on

/* =========================================================================
 * Field DATETIME
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_DATETIME)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_datetime(iotdata_encoder_t *enc, uint32_t seconds_from_year_start) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_DATETIME);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if ((seconds_from_year_start / IOTDATA_DATETIME_RES) > IOTDATA_DATETIME_MAX)
        return IOTDATA_ERR_DATETIME_HIGH;
#endif
    enc->datetime_secs = seconds_from_year_start;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_DATETIME);
    return IOTDATA_OK;
}
#endif
static inline uint32_t quantise_datetime(uint32_t datetime) {
    return (uint32_t)(datetime / IOTDATA_DATETIME_RES);
}
static inline uint32_t dequantise_datetime(uint32_t raw) {
    return (uint32_t)(raw * IOTDATA_DATETIME_RES);
}
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_datetime(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, quantise_datetime(enc->datetime_secs), IOTDATA_DATETIME_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_datetime(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->datetime_secs = dequantise_datetime(bits_read(buf, bb, bp, IOTDATA_DATETIME_BITS));
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_datetime(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_datetime(enc, (uint32_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_datetime(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->datetime_secs);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_datetime(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_DATETIME_BITS);
    const uint32_t secs = dequantise_datetime(r);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "day %u %02u:%02u:%02u (%us)", secs / 86400, (secs % 86400) / 3600, (secs % 3600) / 60, secs % 60, secs);
    n = dump_add(dump, n, s, IOTDATA_DATETIME_BITS, r, dump->_dec_buf, "5s res", "datetime");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_datetime(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s day %u %02u:%02u:%02u (%us)\n", l, _padd(l), d->datetime_secs / 86400, (d->datetime_secs % 86400) / 3600, (d->datetime_secs % 3600) / 60, d->datetime_secs % 60, d->datetime_secs);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_datetime = {
    _IOTDATA_OP_NAME("datetime")
    _IOTDATA_OP_BITS(IOTDATA_DATETIME_BITS)
    _IOTDATA_OP_PACK(pack_datetime)
    _IOTDATA_OP_UNPACK(unpack_datetime)
    _IOTDATA_OP_DUMP(dump_datetime)
    _IOTDATA_OP_PRINT(print_datetime)
    _IOTDATA_OP_JSON_SET(json_set_datetime)
    _IOTDATA_OP_JSON_GET(json_get_datetime)
};
#define _IOTDATA_ENT_DATETIME [IOTDATA_FIELD_DATETIME] = &_iotdata_field_def_datetime,
#define _IOTDATA_ERR_DATETIME \
    case IOTDATA_ERR_DATETIME_HIGH: \
        return "Datetime ticks above maximum";
#else
#define _IOTDATA_ENT_DATETIME
#define _IOTDATA_ERR_DATETIME
#endif
// clang-format on

/* =========================================================================
 * Field IMAGE
 *
 * Variable-length field: 8-bit length prefix + control byte + pixel data.
 * First variable-length data field in the protocol.
 *
 * Wire layout:
 *   [Length:8] [Control:8] [PixelData: Length-1 bytes]
 *
 * Length = number of bytes following (control + pixel data).
 * Total field size = 1 + Length bytes = (1 + Length) * 8 bits.
 *
 * Control byte:
 *   bits 7-6: pixel format (0=bilevel/1bpp, 1=grey4/2bpp, 2=grey16/4bpp)
 *   bits 5-4: size tier (0=24x18, 1=32x24, 2=48x36, 3=64x48)
 *   bits 3-2: compression (0=raw, 1=RLE, 2=heatshrink)
 *   bits 1-0: flags (bit1=fragment, bit0=invert)
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_IMAGE)
static const uint8_t _image_widths[] = { 24, 32, 48, 64 }, _image_heights[] = { 18, 24, 36, 48 }, _image_bits[] = { 1, 2, 4 };
size_t iotdata_image_pixel_count(uint8_t size_tier) {
    return size_tier > 3 ? 0 : (size_t)_image_widths[size_tier] * (size_t)_image_heights[size_tier];
}
uint8_t iotdata_image_bpp(uint8_t pixel_format) {
    return pixel_format <= 2 ? _image_bits[pixel_format] : 0;
}
static inline size_t _image_raw_bytes(uint8_t pixel_format, uint8_t size_tier) {
    return (iotdata_image_pixel_count(size_tier) * (size_t)iotdata_image_bpp(pixel_format) + 7) / 8;
}
static inline uint8_t _pixel_get(const uint8_t *buf, size_t idx, uint8_t bpp) {
    switch (bpp) {
    case 1:
        return (buf[idx / 8] >> (7 - (idx % 8))) & 1;
    case 2:
        return (buf[idx / 4] >> (6 - (idx % 4) * 2)) & 3;
    case 4:
        return (idx & 1) ? buf[idx / 2] & 0x0F : buf[idx / 2] >> 4;
    default:
        return 0;
    }
}
static inline void _pixel_set(uint8_t *buf, size_t idx, uint8_t val, uint8_t bpp) {
    switch (bpp) {
    case 1:
        buf[idx / 8] = (uint8_t)((buf[idx / 8] & ~(1U << (7 - (idx % 8)))) | ((val & 1) << (7 - (idx % 8))));
        break;
    case 2:
        buf[idx / 4] = (uint8_t)((buf[idx / 4] & ~(3U << ((idx % 4) * 2))) | ((val & 3) << ((idx % 4) * 2)));
        break;
    case 4:
        buf[idx / 2] = (uint8_t)(idx & 1 ? (buf[idx / 2] & 0xF0) | (val & 0x0F) : (buf[idx / 2] & 0x0F) | ((val & 0x0F) << 4));
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------------------
 * RLE compression/decompression
 *
 * Bilevel (1bpp):
 *   1-byte runs: bit7 = pixel value, bits 6-0 = count-1 (1..128 pixels)
 *
 * Greyscale (2bpp, 4bpp):
 *   2-byte runs: [value:8] [count-1:8] (1..256 pixels)
 * ------------------------------------------------------------------------- */
size_t iotdata_image_rle_compress(const uint8_t *pixels, size_t pixel_count, uint8_t bpp, uint8_t *out, size_t out_max) {
    if (!pixels || !out || pixel_count == 0 || bpp == 0)
        return 0;
    size_t op = 0;
    if (bpp == 1) {
        uint8_t cur = _pixel_get(pixels, 0, 1);
        size_t count = 1;
        for (size_t i = 1; i < pixel_count; i++) {
            const uint8_t px = _pixel_get(pixels, i, 1);
            if (px == cur && count < 128)
                count++;
            else {
                if (op >= out_max)
                    return 0;
                out[op++] = (uint8_t)((cur << 7) | (count - 1));
                cur = px;
                count = 1;
            }
        }
        if (op >= out_max)
            return 0;
        out[op++] = (uint8_t)((cur << 7) | (count - 1));
    } else {
        uint8_t cur = _pixel_get(pixels, 0, bpp);
        size_t count = 1;
        for (size_t i = 1; i < pixel_count; i++) {
            const uint8_t px = _pixel_get(pixels, i, bpp);
            if (px == cur && count < 256)
                count++;
            else {
                if (op + 2 > out_max)
                    return 0;
                out[op++] = cur;
                out[op++] = (uint8_t)(count - 1);
                cur = px;
                count = 1;
            }
        }
        if (op + 2 > out_max)
            return 0;
        out[op++] = cur;
        out[op++] = (uint8_t)(count - 1);
    }
    return op;
}
size_t iotdata_image_rle_decompress(const uint8_t *compressed, size_t comp_len, uint8_t bpp, uint8_t *pixels, size_t pixel_buf_bytes) {
    if (!compressed || !pixels || comp_len == 0 || bpp == 0)
        return 0;
    size_t px_idx = 0;
    if (bpp == 1)
        for (size_t ip = 0; ip < comp_len; ip++) {
            const uint8_t val = (compressed[ip] >> 7) & 1;
            const size_t count = (compressed[ip] & 0x7F) + 1;
            for (size_t j = 0; j < count && px_idx < (pixel_buf_bytes * 8) / bpp; j++)
                _pixel_set(pixels, px_idx++, val, 1);
        }
    else
        for (size_t ip = 0; ip + 1 < comp_len; ip += 2) {
            const uint8_t val = compressed[ip];
            const size_t count = (size_t)compressed[ip + 1] + 1;
            for (size_t j = 0; j < count && px_idx < (pixel_buf_bytes * 8) / bpp; j++)
                _pixel_set(pixels, px_idx++, val, bpp);
        }
    const size_t used_bits = px_idx * bpp;
    if ((used_bits % 8) > 0)
        pixels[used_bits / 8] &= (uint8_t)(0xFF << (8 - (used_bits % 8)));
    return px_idx;
}

/* -------------------------------------------------------------------------
 * Heatshrink LZSS compression/decompression
 *
 * Self-contained LZSS codec.  Fixed parameters:
 *   window_sz2  = 8  (256-byte window)
 *   lookahead_sz2 = 4  (16-byte lookahead)
 *
 * Bit stream (MSB-first packing, matching protocol convention):
 *   Flag 1 → backref: [index:8] [count:4]
 *     index = distance_back - 1  (0 = 1 byte back, 255 = 256 bytes back)
 *     count = match_length - 1   (0 = 1 byte, 15 = 16 bytes)
 *     Compressor emits backrefs only for match_length >= 2.
 *   Flag 0 → literal: [byte:8]
 *
 * Decoder RAM: ~256 bytes (output serves as window).
 * Encoder: brute-force search, O(N * W * L) — fine for <=384-byte inputs.
 * ------------------------------------------------------------------------- */
#define _HS_W      (1 << IOTDATA_IMAGE_HS_WINDOW_SZ2)    /* 256 */
#define _HS_L      (1 << IOTDATA_IMAGE_HS_LOOKAHEAD_SZ2) /* 16 */
#define _HS_W_BITS IOTDATA_IMAGE_HS_WINDOW_SZ2           /* 8 */
#define _HS_L_BITS IOTDATA_IMAGE_HS_LOOKAHEAD_SZ2        /* 4 */
typedef struct {
    uint8_t *buf;
    size_t max;
    size_t byte_idx;
    uint8_t bit_idx; /* 7 = MSB of current byte, 0 = LSB */
    bool overflow;
} _hs_bw_t;
static inline void _hs_bw_init(_hs_bw_t *bw, uint8_t *buf, size_t max) {
    bw->buf = buf;
    bw->max = max;
    bw->byte_idx = 0;
    bw->bit_idx = 7;
    bw->overflow = false;
    if (max > 0)
        buf[0] = 0;
}
static inline void _hs_bw_put(_hs_bw_t *bw, uint32_t value, uint8_t nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        if (bw->byte_idx >= bw->max) {
            bw->overflow = true;
            return;
        }
        if (value & (1U << i))
            bw->buf[bw->byte_idx] |= (1U << bw->bit_idx);
        if (bw->bit_idx == 0) {
            bw->bit_idx = 7;
            bw->byte_idx++;
            if (bw->byte_idx < bw->max)
                bw->buf[bw->byte_idx] = 0;
        } else {
            bw->bit_idx--;
        }
    }
}
static inline size_t _hs_bw_bytes(const _hs_bw_t *bw) {
    return bw->bit_idx == 7 ? bw->byte_idx : bw->byte_idx + 1;
}
typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t byte_idx;
    uint8_t bit_idx;
} _hs_br_t;
static inline void _hs_br_init(_hs_br_t *br, const uint8_t *buf, size_t len) {
    br->buf = buf;
    br->len = len;
    br->byte_idx = 0;
    br->bit_idx = 7;
}
static inline int _hs_br_get(_hs_br_t *br, uint8_t nbits) {
    uint32_t val = 0;
    for (int i = nbits - 1; i >= 0; i--) {
        if (br->byte_idx >= br->len)
            return -1;
        if (br->buf[br->byte_idx] & (1U << br->bit_idx))
            val |= (1U << i);
        if (br->bit_idx == 0) {
            br->bit_idx = 7;
            br->byte_idx++;
        } else {
            br->bit_idx--;
        }
    }
    return (int)val;
}
static inline bool _hs_br_done(const _hs_br_t *br) {
    return br->byte_idx >= br->len;
}
size_t iotdata_image_hs_compress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max) {
    if (!in || !out || in_len == 0 || out_max == 0)
        return 0;
    _hs_bw_t bw;
    _hs_bw_init(&bw, out, out_max);
    size_t ip = 0;
    while (ip < in_len && !bw.overflow) {
        /* Search for longest match in window */
        size_t best_len = 0, best_off = 0;
        const size_t max_match = (in_len - ip) < _HS_L ? (in_len - ip) : _HS_L;
        for (size_t off = ip > _HS_W ? ip - _HS_W : 0; off < ip; off++) {
            size_t ml = 0;
            while (ml < max_match && in[off + ml] == in[ip + ml])
                ml++;
            if (ml > best_len) {
                best_len = ml;
                best_off = ip - off;
                if (ml == max_match)
                    break;
            }
        }
        if (best_len >= 2) {
            /* Backref: flag(1) + index(W_BITS) + count(L_BITS) */
            _hs_bw_put(&bw, 1, 1);
            _hs_bw_put(&bw, (uint32_t)(best_off - 1), _HS_W_BITS);
            _hs_bw_put(&bw, (uint32_t)(best_len - 1), _HS_L_BITS);
            ip += best_len;
        } else {
            /* Literal: flag(0) + byte(8) */
            _hs_bw_put(&bw, 0, 1);
            _hs_bw_put(&bw, in[ip], 8);
            ip++;
        }
    }
    if (bw.overflow)
        return 0;
    return _hs_bw_bytes(&bw);
}
size_t iotdata_image_hs_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max) {
    if (!in || !out || in_len == 0 || out_max == 0)
        return 0;
    _hs_br_t br;
    _hs_br_init(&br, in, in_len);
    size_t op = 0;
    while (!_hs_br_done(&br) && op < out_max) {
        const int flag = _hs_br_get(&br, 1);
        if (flag < 0)
            break;
        if (flag == 0) {
            /* Literal */
            const int byte = _hs_br_get(&br, 8);
            if (byte < 0)
                break;
            out[op++] = (uint8_t)byte;
        } else {
            /* Backref */
            const int index = _hs_br_get(&br, _HS_W_BITS);
            if (index < 0)
                break;
            const int count = _hs_br_get(&br, _HS_L_BITS);
            if (count < 0)
                break;
            if (((size_t)index + 1) > op)
                break; /* invalid: references before start */
            for (size_t j = 0; j < ((size_t)count + 1) && op < out_max; j++) {
                out[op] = out[op - ((size_t)index + 1)];
                op++;
            }
        }
    }
    return op;
}

#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_image(iotdata_encoder_t *enc, uint8_t pixel_format, uint8_t size_tier, uint8_t compression, uint8_t flags, const uint8_t *data, uint8_t data_len) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_IMAGE);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (pixel_format > 2)
        return IOTDATA_ERR_IMAGE_FORMAT_HIGH;
    if (size_tier > 3)
        return IOTDATA_ERR_IMAGE_SIZE_HIGH;
    if (compression > 2)
        return IOTDATA_ERR_IMAGE_COMPRESSION_HIGH;
    if (!data && data_len > 0)
        return IOTDATA_ERR_IMAGE_DATA_NULL;
    if (data_len > IOTDATA_IMAGE_DATA_MAX)
        return IOTDATA_ERR_IMAGE_DATA_HIGH;
#endif
    enc->image_pixel_format = pixel_format;
    enc->image_size_tier = size_tier;
    enc->image_compression = compression;
    enc->image_flags = flags & 0x03;
    enc->image_data = data;
    enc->image_data_len = data_len;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_IMAGE);
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_image(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    /* Length = 1 (control byte) + pixel data bytes */
    bits_write(buf, bp, (uint8_t)(1 + enc->image_data_len), 8);
    /* Control byte: format(2) | size(2) | compression(2) | flags(2) */
    bits_write(buf, bp, (uint8_t)((enc->image_pixel_format << 6) | (enc->image_size_tier << 4) | (enc->image_compression << 2) | (enc->image_flags & 0x03)), 8);
    /* Pixel data (compressed or raw, as provided by caller) */
    for (int i = 0; i < enc->image_data_len; i++)
        bits_write(buf, bp, enc->image_data[i], 8);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_image(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    if (*bp + 16 > bb)
        return; /* need at least length + control */
    const uint8_t length = (uint8_t)bits_read(buf, bb, bp, 8);
    if (length < 1)
        return; /* control byte required */
    const uint8_t control = (uint8_t)bits_read(buf, bb, bp, 8);
    out->image_pixel_format = (control >> 6) & 0x03;
    out->image_size_tier = (control >> 4) & 0x03;
    out->image_compression = (control >> 2) & 0x03;
    out->image_flags = control & 0x03;
    out->image_data_len = (length > 1) ? (uint8_t)(length - 1) : 0;
    if (out->image_data_len > IOTDATA_IMAGE_DATA_MAX)
        out->image_data_len = IOTDATA_IMAGE_DATA_MAX;
    for (int i = 0; i < out->image_data_len && *bp + 8 <= bb; i++)
        out->image_data[i] = (uint8_t)bits_read(buf, bb, bp, 8);
}
#endif
static const char *_image_fmt_names[] = { "bilevel", "grey4", "grey16", "reserved" };
static const char *_image_size_names[] = { "24x18", "32x24", "48x36", "64x48" };
static const char *_image_comp_names[] = { "raw", "rle", "heatshrink", "reserved" };
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_image(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *obj = cJSON_AddObjectToObject(root, label);
    cJSON_AddStringToObject(obj, "format", _image_fmt_names[d->image_pixel_format & 0x03]);
    cJSON_AddStringToObject(obj, "size", _image_size_names[d->image_size_tier & 0x03]);
    cJSON_AddStringToObject(obj, "compression", _image_comp_names[d->image_compression & 0x03]);
    cJSON_AddBoolToObject(obj, "fragment", (d->image_flags & IOTDATA_IMAGE_FLAG_FRAGMENT) != 0);
    cJSON_AddBoolToObject(obj, "invert", (d->image_flags & IOTDATA_IMAGE_FLAG_INVERT) != 0);
    if (d->image_data_len > 0) {
        /* ((254+2)/3)*4+1 = 344 worst case */
        char b64[((IOTDATA_IMAGE_DATA_MAX + 2) / 3) * 4 + 1];
        _b64_encode(d->image_data, d->image_data_len, b64);
        cJSON_AddStringToObject(obj, "pixels", b64);
    }
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_image(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    /* Parse pixel_format */
    uint8_t fmt = 0;
    cJSON *jf = cJSON_GetObjectItem(j, "format");
    if (jf && jf->valuestring)
        for (int i = 0; i < 3; i++)
            if (strcmp(jf->valuestring, _image_fmt_names[i]) == 0) {
                fmt = (uint8_t)i;
                break;
            }
    /* Parse size */
    uint8_t sz = 0;
    cJSON *js = cJSON_GetObjectItem(j, "size");
    if (js && js->valuestring)
        for (int i = 0; i < 4; i++)
            if (strcmp(js->valuestring, _image_size_names[i]) == 0) {
                sz = (uint8_t)i;
                break;
            }
    /* Parse compression */
    uint8_t comp = 0;
    cJSON *jc = cJSON_GetObjectItem(j, "compression");
    if (jc && jc->valuestring)
        for (int i = 0; i < 3; i++)
            if (strcmp(jc->valuestring, _image_comp_names[i]) == 0) {
                comp = (uint8_t)i;
                break;
            }
    /* Parse flags */
    uint8_t flags = 0;
    if (cJSON_IsTrue(cJSON_GetObjectItem(j, "fragment")))
        flags |= IOTDATA_IMAGE_FLAG_FRAGMENT;
    if (cJSON_IsTrue(cJSON_GetObjectItem(j, "invert")))
        flags |= IOTDATA_IMAGE_FLAG_INVERT;
    /* Parse pixels (base64 → raw bytes) */
    static uint8_t _img_scratch[IOTDATA_IMAGE_DATA_MAX];
    uint8_t dlen = 0;
    cJSON *jp = cJSON_GetObjectItem(j, "pixels");
    if (jp && jp->valuestring)
        dlen = (uint8_t)_b64_decode(jp->valuestring, _img_scratch, IOTDATA_IMAGE_DATA_MAX);
    return iotdata_encode_image(enc, fmt, sz, comp, flags, _img_scratch, dlen);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_image(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    if (*bp + 16 > bb)
        return n;
    /* Length byte */
    size_t s = *bp;
    const uint8_t length = (uint8_t)bits_read(buf, bb, bp, 8);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u (%u total)", length, 1 + length);
    n = dump_add(dump, n, s, 8, length, dump->_dec_buf, "1..255", "image_length");
    /* Control byte */
    s = *bp;
    const uint8_t control = (uint8_t)bits_read(buf, bb, bp, 8);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%s %s %s%s%s", _image_fmt_names[(control >> 6) & 3], _image_size_names[(control >> 4) & 3], _image_comp_names[(control >> 2) & 3], (control & IOTDATA_IMAGE_FLAG_FRAGMENT) ? " frag" : "",
             (control & IOTDATA_IMAGE_FLAG_INVERT) ? " inv" : "");
    n = dump_add(dump, n, s, 8, control, dump->_dec_buf, "fmt|sz|comp|flg", "image_control");
    /* Pixel data (show as single span) */
    const uint8_t data_len = (length > 1) ? (uint8_t)(length - 1) : 0;
    if (data_len > 0) {
        s = *bp;
        const size_t data_bits = (size_t)data_len * 8;
        if (*bp + data_bits <= bb)
            *bp += data_bits;
        else
            *bp = bb;
        snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u bytes", data_len);
        n = dump_add(dump, n, s, data_bits, 0, dump->_dec_buf, "pixel data", "image_pixels");
    }
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_image(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s %s %s %s, %u bytes%s%s\n", l, _padd(l), _image_fmt_names[d->image_pixel_format & 3], _image_size_names[d->image_size_tier & 3], _image_comp_names[d->image_compression & 3], d->image_data_len,
            (d->image_flags & IOTDATA_IMAGE_FLAG_FRAGMENT) ? " [fragment]" : "", (d->image_flags & IOTDATA_IMAGE_FLAG_INVERT) ? " [inverted]" : "");
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_image = {
    _IOTDATA_OP_NAME("image")
    _IOTDATA_OP_BITS(0) /* variable length */
    _IOTDATA_OP_PACK(pack_image)
    _IOTDATA_OP_UNPACK(unpack_image)
    _IOTDATA_OP_DUMP(dump_image)
    _IOTDATA_OP_PRINT(print_image)
    _IOTDATA_OP_JSON_SET(json_set_image)
    _IOTDATA_OP_JSON_GET(json_get_image)
};
#define _IOTDATA_ENT_IMAGE [IOTDATA_FIELD_IMAGE] = &_iotdata_field_def_image,
#define _IOTDATA_ERR_IMAGE \
    case IOTDATA_ERR_IMAGE_FORMAT_HIGH: \
        return "Image pixel format above 2"; \
    case IOTDATA_ERR_IMAGE_SIZE_HIGH: \
        return "Image size tier above 3"; \
    case IOTDATA_ERR_IMAGE_COMPRESSION_HIGH: \
        return "Image compression above 2"; \
    case IOTDATA_ERR_IMAGE_DATA_NULL: \
        return "Image data pointer is NULL"; \
    case IOTDATA_ERR_IMAGE_DATA_HIGH: \
        return "Image data exceeds 254 bytes";
#else
#define _IOTDATA_ENT_IMAGE
#define _IOTDATA_ERR_IMAGE
#endif
// clang-format on

/* =========================================================================
 * Field FLAGS
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_FLAGS)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_flags(iotdata_encoder_t *enc, uint8_t flags) {
    CHECK_CTX_ACTIVE(enc);
    CHECK_NOT_DUPLICATE(enc, IOTDATA_FIELD_FLAGS);
    enc->flags = flags;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_FLAGS);
    return IOTDATA_OK;
}
#endif
// quantise
#if !defined(IOTDATA_NO_ENCODE)
static inline void pack_flags(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    bits_write(buf, bp, enc->flags, IOTDATA_FLAGS_BITS);
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_flags(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    out->flags = (uint8_t)bits_read(buf, bb, bp, IOTDATA_FLAGS_BITS);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_flags(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (!j)
        return IOTDATA_OK;
    return iotdata_encode_flags(enc, (uint8_t)j->valueint);
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_flags(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON_AddNumberToObject(root, label, d->flags);
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_flags(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    size_t s = *bp;
    uint32_t r = bits_read(buf, bb, bp, IOTDATA_FLAGS_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02x", r);
    n = dump_add(dump, n, s, IOTDATA_FLAGS_BITS, r, dump->_dec_buf, "8-bit bitmask", "flags");
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_flags(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s:%s 0x%02x\n", l, _padd(l), d->flags);
}
#endif
// clang-format off
static const iotdata_field_ops_t _iotdata_field_def_flags = {
    _IOTDATA_OP_NAME("flags")
    _IOTDATA_OP_BITS(IOTDATA_FLAGS_BITS)
    _IOTDATA_OP_PACK(pack_flags)
    _IOTDATA_OP_UNPACK(unpack_flags)
    _IOTDATA_OP_DUMP(dump_flags)
    _IOTDATA_OP_PRINT(print_flags)
    _IOTDATA_OP_JSON_SET(json_set_flags)
    _IOTDATA_OP_JSON_GET(json_get_flags)
};
#define _IOTDATA_ENT_FLAGS [IOTDATA_FIELD_FLAGS] = &_iotdata_field_def_flags,
#define _IOTDATA_ERR_FLAGS
#else
#define _IOTDATA_ENT_FLAGS
#define _IOTDATA_ERR_FLAGS
#endif
// clang-format on

/* =========================================================================
 * Field TLV
 * ========================================================================= */

#if defined(IOTDATA_ENABLE_TLV)
#if !defined(IOTDATA_NO_ENCODE)
iotdata_status_t iotdata_encode_tlv(iotdata_encoder_t *enc, uint8_t type, const uint8_t *data, uint8_t length) {
    CHECK_CTX_ACTIVE(enc);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (type > IOTDATA_TLV_TYPE_MAX)
        return IOTDATA_ERR_TLV_TYPE_HIGH;
    if (!data)
        return IOTDATA_ERR_TLV_DATA_NULL;
    /* length is uint8_t, max 255 == IOTDATA_TLV_DATA_MAX, always in range */
#endif
    if (enc->tlv_count >= IOTDATA_TLV_MAX)
        return IOTDATA_ERR_TLV_FULL;
    const int idx = enc->tlv_count++;
    enc->tlv[idx].format = IOTDATA_TLV_FMT_RAW;
    enc->tlv[idx].type = type;
    enc->tlv[idx].length = length;
    enc->tlv[idx].data = data;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_TLV);
    return IOTDATA_OK;
}
iotdata_status_t iotdata_encode_tlv_string(iotdata_encoder_t *enc, uint8_t type, const char *str) {
    CHECK_CTX_ACTIVE(enc);
#if !defined(IOTDATA_NO_CHECKS_TYPES)
    if (type > IOTDATA_TLV_TYPE_MAX)
        return IOTDATA_ERR_TLV_TYPE_HIGH;
    if (!str)
        return IOTDATA_ERR_TLV_STR_NULL;
    const size_t slen = strlen(str);
    if (slen > IOTDATA_TLV_STR_LEN_MAX)
        return IOTDATA_ERR_TLV_STR_LEN_HIGH;
    for (size_t i = 0; i < slen; i++)
        if (char_to_sixbit(str[i]) < 0)
            return IOTDATA_ERR_TLV_STR_CHAR_INVALID;
#else
    size_t slen = strlen(str);
    if (slen > IOTDATA_TLV_STR_LEN_MAX)
        slen = IOTDATA_TLV_STR_LEN_MAX;
#endif
    if (enc->tlv_count >= IOTDATA_TLV_MAX)
        return IOTDATA_ERR_TLV_FULL;
    const int idx = enc->tlv_count++;
    enc->tlv[idx].format = IOTDATA_TLV_FMT_STRING;
    enc->tlv[idx].type = type;
    enc->tlv[idx].length = (uint8_t)slen;
    enc->tlv[idx].str = str;
    IOTDATA_FIELD_SET(enc->fields, IOTDATA_FIELD_TLV);
    return IOTDATA_OK;
}
static inline void pack_tlv(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc) {
    for (int i = 0; i < enc->tlv_count; i++) {
        bits_write(buf, bp, enc->tlv[i].format, IOTDATA_TLV_FMT_BITS);
        bits_write(buf, bp, enc->tlv[i].type, IOTDATA_TLV_TYPE_BITS);
        bits_write(buf, bp, (i < enc->tlv_count - 1) ? 1 : 0, IOTDATA_TLV_MORE_BITS);
        bits_write(buf, bp, enc->tlv[i].length, IOTDATA_TLV_LENGTH_BITS);
        if (enc->tlv[i].format == IOTDATA_TLV_FMT_RAW)
            for (int j = 0; j < enc->tlv[i].length; j++)
                bits_write(buf, bp, enc->tlv[i].data[j], 8);
        else
            for (int j = 0; j < enc->tlv[i].length; j++)
                bits_write(buf, bp, (uint32_t)char_to_sixbit(enc->tlv[i].str[j]), IOTDATA_TLV_CHAR_BITS);
    }
}
#endif
#if !defined(IOTDATA_NO_DECODE)
static inline void unpack_tlv(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out) {
    bool more = true;
    while (more && out->tlv_count < IOTDATA_TLV_MAX && *bp + IOTDATA_TLV_HEADER_BITS <= bb) {
        const uint8_t format = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_FMT_BITS);
        const uint8_t type = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_TYPE_BITS);
        more = bits_read(buf, bb, bp, IOTDATA_TLV_MORE_BITS) != 0;
        const uint8_t length = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_LENGTH_BITS);
        const int idx = out->tlv_count++;
        out->tlv[idx].format = format;
        out->tlv[idx].type = type;
        out->tlv[idx].length = length;
        if (format == IOTDATA_TLV_FMT_STRING) {
            for (int j = 0; j < length && *bp + IOTDATA_TLV_CHAR_BITS <= bb; j++)
                out->tlv[idx].str[j] = sixbit_to_char((uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_CHAR_BITS));
            out->tlv[idx].str[length] = '\0';
        } else {
            for (int j = 0; j < length && *bp + 8 <= bb; j++)
                out->tlv[idx].raw[j] = (uint8_t)bits_read(buf, bb, bp, 8);
        }
    }
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE)
static inline void json_set_tlv(cJSON *root, const iotdata_decoded_t *d, const char *label) {
    cJSON *arr = cJSON_AddArrayToObject(root, label);
    for (int i = 0; i < d->tlv_count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "type", d->tlv[i].type);
        cJSON_AddStringToObject(obj, "format", d->tlv[i].format == IOTDATA_TLV_FMT_STRING ? "string" : "raw");
        if (d->tlv[i].format == IOTDATA_TLV_FMT_STRING) {
            cJSON_AddStringToObject(obj, "data", d->tlv[i].str);
        } else {
            char b64[(IOTDATA_TLV_DATA_MAX * 2) + 1];
            _b64_encode(d->tlv[i].raw, d->tlv[i].length, b64);
            cJSON_AddStringToObject(obj, "data", b64);
        }
        cJSON_AddItemToArray(arr, obj);
    }
}
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE)
static inline iotdata_status_t json_get_tlv(cJSON *root, iotdata_encoder_t *enc, const char *label) {
    iotdata_status_t rc;
    cJSON *j = cJSON_GetObjectItem(root, label);
    if (j && cJSON_IsArray(j)) {
        static uint8_t tlv_raw_scratch[IOTDATA_TLV_MAX][IOTDATA_TLV_DATA_MAX];
        static char tlv_str_scratch[IOTDATA_TLV_MAX][IOTDATA_TLV_STR_LEN_MAX + 1];
        cJSON *item;
        int tidx = 0;
        cJSON_ArrayForEach(item, j) {
            if (tidx >= IOTDATA_TLV_MAX)
                break;
            const uint8_t type = (uint8_t)cJSON_GetObjectItem(item, "type")->valueint;
            cJSON *format = cJSON_GetObjectItem(item, "format");
            const char *data = cJSON_GetObjectItem(item, "data")->valuestring;
            if (strcmp(format ? format->valuestring : "raw", "string") == 0) {
                snprintf(tlv_str_scratch[tidx], sizeof(tlv_str_scratch[tidx]), "%s", data);
                rc = iotdata_encode_tlv_string(enc, type, tlv_str_scratch[tidx]);
            } else {
                rc = iotdata_encode_tlv(enc, type, tlv_raw_scratch[tidx], (uint8_t)_b64_decode(data, tlv_raw_scratch[tidx], IOTDATA_TLV_DATA_MAX));
            }
            if (rc != IOTDATA_OK)
                return rc;
            tidx++;
        }
    }
    return IOTDATA_OK;
}
#endif
#if !defined(IOTDATA_NO_DUMP)
static inline int dump_tlv(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, const char *label) {
    (void)label;
    bool more = true;
    int tlv_idx = 0;
    while (more && *bp + IOTDATA_TLV_HEADER_BITS <= bb) {
        size_t s = *bp;
        const uint8_t format = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_FMT_BITS);
        const uint8_t type = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_TYPE_BITS);
        more = bits_read(buf, bb, bp, IOTDATA_TLV_MORE_BITS) != 0;
        snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%s type=%u more=%d", format == IOTDATA_TLV_FMT_STRING ? "str" : "raw", type, more ? 1 : 0);
        char name_buf[IOTDATA_DUMP_FIELD_NAME_MAX];
        snprintf(name_buf, sizeof(name_buf), "tlv[%d].hdr", tlv_idx);
        n = dump_add(dump, n, s, IOTDATA_TLV_FMT_BITS + IOTDATA_TLV_TYPE_BITS + IOTDATA_TLV_MORE_BITS, 0, dump->_dec_buf, "format+type+more", name_buf);
        s = *bp;
        const uint8_t length = (uint8_t)bits_read(buf, bb, bp, IOTDATA_TLV_LENGTH_BITS);
        snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u", length);
        snprintf(name_buf, sizeof(name_buf), "tlv[%d].len", tlv_idx);
        n = dump_add(dump, n, s, IOTDATA_TLV_LENGTH_BITS, length, dump->_dec_buf, "0..255", name_buf);
        if (length > 0) {
            s = *bp;
            const size_t data_bits = (format == IOTDATA_TLV_FMT_STRING) ? (size_t)length * IOTDATA_TLV_CHAR_BITS : (size_t)length * 8;
            snprintf(name_buf, sizeof(name_buf), "tlv[%d].data", tlv_idx);
            snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "(%zu bits)", data_bits);
            n = dump_add(dump, n, s, data_bits, 0, dump->_dec_buf, format == IOTDATA_TLV_FMT_STRING ? "6-bit chars" : "raw bytes", name_buf);
            bp += data_bits;
        }
        tlv_idx++;
    }
    return n;
}
#endif
#if !defined(IOTDATA_NO_PRINT) && !defined(IOTDATA_NO_DECODE)
static inline void print_tlv(const iotdata_decoded_t *d, FILE *fp, const char *l) {
    fprintf(fp, "  %s: %u TLV entries\n", l, d->tlv_count);
    for (int i = 0; i < d->tlv_count; i++) {
        if (d->tlv[i].format == IOTDATA_TLV_FMT_STRING) {
            fprintf(fp, "    [%d] type=%u str(%u)=\"%s\"\n", i, d->tlv[i].type, d->tlv[i].length, d->tlv[i].str);
        } else {
            fprintf(fp, "    [%d] type=%u raw(%u)=", i, d->tlv[i].type, d->tlv[i].length);
            for (int j = 0; j < d->tlv[i].length && j < 16; j++)
                fprintf(fp, "%02x", d->tlv[i].raw[j]);
            if (d->tlv[i].length > 16)
                fprintf(fp, "...");
            fprintf(fp, "\n");
        }
    }
}
#endif
#define _IOTDATA_ERR_TLV \
    case IOTDATA_ERR_TLV_TYPE_HIGH: \
        return "TLV type above maximum (63)"; \
    case IOTDATA_ERR_TLV_DATA_NULL: \
        return "TLV data pointer is NULL"; \
    case IOTDATA_ERR_TLV_LEN_HIGH: \
        return "TLV length above maximum (255)"; \
    case IOTDATA_ERR_TLV_FULL: \
        return "TLV fields exhausted (max 8)"; \
    case IOTDATA_ERR_TLV_STR_NULL: \
        return "TLV string pointer is NULL"; \
    case IOTDATA_ERR_TLV_STR_LEN_HIGH: \
        return "TLV string too long (max 255 chars)"; \
    case IOTDATA_ERR_TLV_STR_CHAR_INVALID: \
        return "TLV string contains unencodable character";
#else
#define _IOTDATA_ERR_TLV
#endif

/* =========================================================================
 * Internal field operations table
 * ========================================================================= */

// clang-format off

static const iotdata_field_ops_t *_iotdata_field_ops[IOTDATA_FIELD_COUNT] = {
    _IOTDATA_ENT_BATTERY
    _IOTDATA_ENT_LINK
    _IOTDATA_ENT_ENVIRONMENT
    _IOTDATA_ENT_TEMPERATURE
    _IOTDATA_ENT_PRESSURE
    _IOTDATA_ENT_HUMIDITY
    _IOTDATA_ENT_WIND
    _IOTDATA_ENT_WIND_SPEED
    _IOTDATA_ENT_WIND_DIRECTION
    _IOTDATA_ENT_WIND_GUST
    _IOTDATA_ENT_RAIN
    _IOTDATA_ENT_RAIN_RATE
    _IOTDATA_ENT_RAIN_SIZE
    _IOTDATA_ENT_SOLAR
    _IOTDATA_ENT_CLOUDS
    _IOTDATA_ENT_AIR_QUALITY
    _IOTDATA_ENT_AIR_QUALITY_INDEX
    _IOTDATA_ENT_AIR_QUALITY_PM
    _IOTDATA_ENT_AIR_QUALITY_GAS
    _IOTDATA_ENT_RADIATION
    _IOTDATA_ENT_RADIATION_CPM
    _IOTDATA_ENT_RADIATION_DOSE
    _IOTDATA_ENT_DEPTH
    _IOTDATA_ENT_POSITION
    _IOTDATA_ENT_DATETIME
    _IOTDATA_ENT_IMAGE
    _IOTDATA_ENT_FLAGS
};

// clang-format on

/* =========================================================================
 * Internal header
 * ========================================================================= */

static inline int _iotdata_field_count(int num_pres_bytes) {
    if (num_pres_bytes <= 0)
        return 0;
    return IOTDATA_PRES0_DATA_FIELDS + IOTDATA_PRESN_DATA_FIELDS * (num_pres_bytes - 1);
}

static inline int _iotdata_field_pres_byte(int field_idx) {
    if (field_idx < IOTDATA_PRES0_DATA_FIELDS)
        return 0;
    return 1 + (field_idx - IOTDATA_PRES0_DATA_FIELDS) / IOTDATA_PRESN_DATA_FIELDS;
}

static inline int _iotdata_field_pres_bit(int field_idx) {
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

static inline void _iotdata_encode_pack_field(uint8_t *buf, size_t *bp, const iotdata_encoder_t *enc, iotdata_field_type_t type) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->pack)
        ops->pack(buf, bp, enc);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
iotdata_status_t iotdata_encode_begin(iotdata_encoder_t *enc, uint8_t *buf, size_t buf_size, uint8_t variant, uint16_t station, uint16_t sequence) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!enc)
        return IOTDATA_ERR_CTX_NULL;
    if (!buf)
        return IOTDATA_ERR_BUF_NULL;
    if (buf_size < bits_to_bytes(IOTDATA_HEADER_BITS + 8))
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
    return IOTDATA_OK;
}
#pragma GCC diagnostic pop

iotdata_status_t iotdata_encode_end(iotdata_encoder_t *enc, size_t *out_bytes) {
    CHECK_CTX_ACTIVE(enc);

    const iotdata_variant_def_t *vdef = iotdata_get_variant(enc->variant);
    size_t bp = 0;

    /* Header */
    bits_write(enc->buf, &bp, enc->variant, IOTDATA_VARIANT_BITS);
    bits_write(enc->buf, &bp, enc->station, IOTDATA_STATION_BITS);
    bits_write(enc->buf, &bp, enc->sequence, IOTDATA_SEQUENCE_BITS);

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    int max_pres_needed = 1; /* always have pres0 */
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(enc->fields, vdef->fields[si].type)) {
            const int pb = _iotdata_field_pres_byte(si);
            pres[pb] |= (1U << _iotdata_field_pres_bit(si));
            if (pb + 1 > max_pres_needed)
                max_pres_needed = pb + 1;
        }
#if defined(IOTDATA_ENABLE_TLV)
    if (IOTDATA_FIELD_PRESENT(enc->fields, IOTDATA_FIELD_TLV))
        pres[0] |= IOTDATA_PRES_TLV;
#endif
    for (int i = 0; i < max_pres_needed; i++)
        bits_write(enc->buf, &bp, pres[i] | (i < (max_pres_needed - 1) ? IOTDATA_PRES_EXT : 0), 8);

    /* Fields */
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type)) {
            const int pb = _iotdata_field_pres_byte(si);
            if (pb < max_pres_needed && pres[pb] & (1U << _iotdata_field_pres_bit(si)))
                _iotdata_encode_pack_field(enc->buf, &bp, enc, vdef->fields[si].type);
        }

    /* TLV */
#if defined(IOTDATA_ENABLE_TLV)
    if (IOTDATA_FIELD_PRESENT(enc->fields, IOTDATA_FIELD_TLV))
        pack_tlv(enc->buf, &bp, enc);
#endif

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
        return "Encoding context pointer is NULL";
#else
#define _IOTDATA_ERR_ENCODE
#endif

/* =========================================================================
 * External DECODER
 * ========================================================================= */

#if !defined(IOTDATA_NO_DECODE)

static inline void _iotdata_decode_unpack_field(const uint8_t *buf, size_t bb, size_t *bp, iotdata_decoded_t *out, iotdata_field_type_t type) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->unpack)
        ops->unpack(buf, bb, bp, out);
}

iotdata_status_t iotdata_peek(const uint8_t *buf, size_t len, uint8_t *variant, uint16_t *station, uint16_t *sequence) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < bits_to_bytes(IOTDATA_HEADER_BITS + 8))
        return IOTDATA_ERR_DECODE_SHORT;

    size_t bb = len * 8, bp = 0;

    if (variant)
        *variant = (uint8_t)bits_read(buf, bb, &bp, IOTDATA_VARIANT_BITS);
    if (station)
        *station = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    if (sequence)
        *sequence = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);

    if (*variant == IOTDATA_VARIANT_RESERVED)
        return IOTDATA_ERR_DECODE_VARIANT;

    return IOTDATA_OK;
}

iotdata_status_t iotdata_decode(const uint8_t *buf, size_t len, iotdata_decoded_t *out) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf || !out)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < bits_to_bytes(IOTDATA_HEADER_BITS + 8))
        return IOTDATA_ERR_DECODE_SHORT;

    size_t bb = len * 8, bp = 0;

    /* Header */
    out->variant = (uint8_t)bits_read(buf, bb, &bp, IOTDATA_VARIANT_BITS);
    out->station = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    out->sequence = (uint16_t)bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);
    if (out->variant == IOTDATA_VARIANT_RESERVED)
        return IOTDATA_ERR_DECODE_VARIANT;

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    pres[0] = (uint8_t)bits_read(buf, bb, &bp, 8);
    int num_pres = 1;
    while (num_pres < IOTDATA_PRES_MAXIMUM && bp + 8 <= bb && (pres[num_pres - 1] & IOTDATA_PRES_EXT) != 0)
        pres[num_pres++] = (uint8_t)bits_read(buf, bb, &bp, 8);

    /* Fields */
    out->fields = IOTDATA_FIELD_EMPTY;
    const iotdata_variant_def_t *vdef = iotdata_get_variant(out->variant);
    for (int si = 0; si < _iotdata_field_count(num_pres) && si < IOTDATA_MAX_DATA_FIELDS; si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && _iotdata_field_pres_byte(si) < num_pres && pres[_iotdata_field_pres_byte(si)] & (1U << _iotdata_field_pres_bit(si))) {
            IOTDATA_FIELD_SET(out->fields, vdef->fields[si].type);
            _iotdata_decode_unpack_field(buf, bb, &bp, out, vdef->fields[si].type);
        }

    /* TLV */
#if defined(IOTDATA_ENABLE_TLV)
    if ((pres[0] & IOTDATA_PRES_TLV) != 0) {
        IOTDATA_FIELD_SET(out->fields, IOTDATA_FIELD_TLV);
        unpack_tlv(buf, bb, &bp, out);
    }
#endif

    out->packed_bits = bp;
    out->packed_bytes = bits_to_bytes(bp);
    return IOTDATA_OK;
}

#endif /* !IOTDATA_NO_DECODE */

#if !defined(IOTDATA_NO_DECODE)
#define _IOTDATA_ERR_DECODE \
    case IOTDATA_ERR_DECODE_SHORT: \
        return "Decoding buffer too short for header"; \
    case IOTDATA_ERR_DECODE_VARIANT: \
        return "Decoding variant unsupported"; \
    case IOTDATA_ERR_DECODE_TRUNCATED: \
        return "Decoding packet truncated";
#elif !defined(IOTDATA_NO_DUMP)
#define _IOTDATA_ERR_DECODE \
    case IOTDATA_ERR_DECODE_SHORT: \
        return "Decoding buffer too short for header";
#else
#define _IOTDATA_ERR_DECODE
#endif

/* =========================================================================
 * External JSON
 * ========================================================================= */

#if !defined(IOTDATA_NO_JSON)
#if !defined(IOTDATA_NO_DECODE)

static inline void _iotdata_decode_to_json_set_field(cJSON *root, const iotdata_decoded_t *d, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->json_set)
        ops->json_set((void *)root, (const void *)d, label);
}

iotdata_status_t iotdata_decode_to_json(const uint8_t *buf, size_t len, char **json_out) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!json_out)
        return IOTDATA_ERR_CTX_NULL;
#endif

    iotdata_decoded_t dec;
    iotdata_status_t rc;
    if ((rc = iotdata_decode(buf, len, &dec)) != IOTDATA_OK)
        return rc;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return IOTDATA_ERR_JSON_ALLOC;

    cJSON_AddNumberToObject(root, "variant", dec.variant);
    cJSON_AddNumberToObject(root, "station", dec.station);
    cJSON_AddNumberToObject(root, "sequence", dec.sequence);
    cJSON_AddNumberToObject(root, "packed_bits", (uint32_t)dec.packed_bits);
    cJSON_AddNumberToObject(root, "packed_bytes", (uint32_t)dec.packed_bytes);

    /* Fields */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(dec.variant);
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(dec.fields, vdef->fields[si].type))
            _iotdata_decode_to_json_set_field(root, &dec, vdef->fields[si].type, vdef->fields[si].label);

    /* TLV */
#if defined(IOTDATA_ENABLE_TLV)
    if (IOTDATA_FIELD_PRESENT(dec.fields, IOTDATA_FIELD_TLV))
        json_set_tlv(root, &dec, "data");
#endif

    *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!*json_out)
        return IOTDATA_ERR_JSON_ALLOC;
    return IOTDATA_OK;
}

#endif /* !IOTDATA_NO_DECODE */

#if !defined(IOTDATA_NO_ENCODE)

static inline iotdata_status_t _iotdata_encode_from_json_get_field(cJSON *root, iotdata_encoder_t *enc, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->json_get)
        return ops->json_get((void *)root, (void *)enc, label);
    return IOTDATA_OK;
}

iotdata_status_t iotdata_encode_from_json(const char *json, uint8_t *buf, size_t buf_size, size_t *out_bytes) {
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

    iotdata_encoder_t enc;
    iotdata_status_t rc;
    if ((rc = iotdata_encode_begin(&enc, buf, buf_size, (uint8_t)j_var->valueint, (uint16_t)j_sid->valueint, (uint16_t)j_seq->valueint)) != IOTDATA_OK) {
        cJSON_Delete(root);
        return rc;
    }

    /* Fields */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(enc.variant);
    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type))
            if ((rc = _iotdata_encode_from_json_get_field(root, &enc, vdef->fields[si].type, vdef->fields[si].label)) != IOTDATA_OK) {
                cJSON_Delete(root);
                return rc;
            }

    /* TLV */
#if defined(IOTDATA_ENABLE_TLV)
    if ((rc = json_get_tlv(root, &enc, "data")) != IOTDATA_OK) {
        cJSON_Delete(root);
        return rc;
    }
#endif

    rc = iotdata_encode_end(&enc, out_bytes);
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

#define IOTDATA_MAX_DUMP_ENTRIES 48
static int dump_add(iotdata_dump_t *dump, int n, size_t bit_offset, size_t bit_length, uint32_t raw_value, const char *decoded, const char *range, const char *name) {
    if (n >= IOTDATA_MAX_DUMP_ENTRIES)
        return n;
    iotdata_dump_entry_t *e = &dump->entries[n];
    e->bit_offset = bit_offset;
    e->bit_length = bit_length;
    e->raw_value = raw_value;
    snprintf(e->field_name, sizeof(e->field_name), "%s", name);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wrestrict"
    snprintf(e->decoded_str, sizeof(e->decoded_str), "%s", decoded);
    snprintf(e->range_str, sizeof(e->range_str), "%s", range);
#pragma GCC diagnostic pop
    return n + 1;
}

static inline int _iotdata_dump_build_field(const uint8_t *buf, size_t bb, size_t *bp, iotdata_dump_t *dump, int n, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->dump)
        return ops->dump(buf, bb, bp, dump, n, label);
    return n;
}

static iotdata_status_t _iotdata_dump_build(const uint8_t *buf, size_t len, iotdata_dump_t *dump) {
#if !defined(IOTDATA_NO_CHECKS_STATE)
    if (!buf || !dump)
        return IOTDATA_ERR_CTX_NULL;
#endif

    if (len < bits_to_bytes(IOTDATA_HEADER_BITS + 8))
        return IOTDATA_ERR_DECODE_SHORT;

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
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u", raw);
    n = dump_add(dump, n, s, IOTDATA_VARIANT_BITS, raw, dump->_dec_buf, "0-14 (15=rsvd)", "variant");
    const uint8_t variant = (uint8_t)raw;
    s = bp;
    raw = bits_read(buf, bb, &bp, IOTDATA_STATION_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u", raw);
    n = dump_add(dump, n, s, IOTDATA_STATION_BITS, raw, dump->_dec_buf, "0-4095", "station");
    s = bp;
    raw = bits_read(buf, bb, &bp, IOTDATA_SEQUENCE_BITS);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "%u", raw);
    n = dump_add(dump, n, s, IOTDATA_SEQUENCE_BITS, raw, dump->_dec_buf, "0-65535", "sequence");

    /* Presence */
    uint8_t pres[IOTDATA_PRES_MAXIMUM] = { 0 };
    s = bp;
    pres[0] = (uint8_t)bits_read(buf, bb, &bp, 8);
    snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02x", pres[0]);
    n = dump_add(dump, n, s, 8, pres[0], dump->_dec_buf, "ext|tlv|6 fields", "presence[0]");
    int num_pres = 1;
    while (num_pres < IOTDATA_PRES_MAXIMUM && bp + 8 <= bb && (pres[num_pres - 1] & IOTDATA_PRES_EXT) != 0) {
        s = bp;
        pres[num_pres] = (uint8_t)bits_read(buf, bb, &bp, 8);
        char pname[24];
        snprintf(pname, sizeof(pname), "presence[%d]", num_pres);
        snprintf(dump->_dec_buf, sizeof(dump->_dec_buf), "0x%02x", pres[num_pres]);
        n = dump_add(dump, n, s, 8, pres[num_pres], dump->_dec_buf, "ext|7 fields", pname);
        num_pres++;
    }

    /* Fields */
    const iotdata_variant_def_t *vdef = iotdata_get_variant(variant);
    for (int si = 0; si < _iotdata_field_count(num_pres) && si < IOTDATA_MAX_DATA_FIELDS; si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type))
            if (_iotdata_field_pres_byte(si) < num_pres && pres[_iotdata_field_pres_byte(si)] & (1U << _iotdata_field_pres_bit(si)))
                n = _iotdata_dump_build_field(buf, bb, &bp, dump, n, vdef->fields[si].type, vdef->fields[si].label);

    /* TLV */
#if defined(IOTDATA_ENABLE_TLV)
    if ((pres[0] & IOTDATA_PRES_TLV) != 0)
        n = dump_tlv(buf, bb, &bp, dump, n, "tlv");
#endif

    dump->count = (size_t)n;
    dump->packed_bits = bp;
    dump->packed_bytes = bits_to_bytes(bp);
    return IOTDATA_OK;
}

static iotdata_status_t _iotdata_dump_decoded_to_file(const iotdata_dump_t *dump, FILE *fp) {
    fprintf(fp, "%12s  %6s  %-24s  %10s  %-28s  %s\n", "Offset", "Len", "Field", "Raw", "Decoded", "Range");
    fprintf(fp, "%12s  %6s  %-24s  %10s  %-28s  %s\n", "------", "---", "-----", "---", "-------", "-----");
    for (size_t i = 0; i < dump->count; i++) {
        const iotdata_dump_entry_t *e = &dump->entries[i];
        fprintf(fp, "%12zu  %6zu  %-24s  %10u  %-28s  %s\n", e->bit_offset, e->bit_length, e->field_name, e->raw_value, e->decoded_str, e->range_str);
    }
    fprintf(fp, "\nTotal: %zu bits (%zu bytes)\n", dump->packed_bits, dump->packed_bytes);
    return IOTDATA_OK;
}

static iotdata_status_t _iotdata_dump_oneline_to_file(const iotdata_dump_t *dump, FILE *fp) {
    for (size_t i = 0; i < dump->count; i++) {
        const iotdata_dump_entry_t *e = &dump->entries[i];
        fprintf(fp, "%s%s=%s%s", (i > 0 ? ", " : ""), e->field_name, e->decoded_str, (i + 1 == dump->count ? "\n" : ""));
    }
    return IOTDATA_OK;
}

iotdata_status_t iotdata_dump_to_file(const uint8_t *buf, size_t len, FILE *fp, bool verbose) {
    iotdata_dump_t dump;
    iotdata_status_t rc;
    if ((rc = _iotdata_dump_build(buf, len, &dump)) != IOTDATA_OK)
        return rc;
    return verbose ? _iotdata_dump_decoded_to_file(&dump, fp) : _iotdata_dump_oneline_to_file(&dump, fp);
}

iotdata_status_t iotdata_dump_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size, bool verbose) {
    iotdata_dump_t dump;
    iotdata_status_t rc;
    if ((rc = _iotdata_dump_build(buf, len, &dump)) != IOTDATA_OK)
        return rc;
    FILE *fp = fmemopen(out, out_size, "w");
    if (!fp)
        return IOTDATA_ERR_DUMP_ALLOC;
    rc = verbose ? _iotdata_dump_decoded_to_file(&dump, fp) : _iotdata_dump_oneline_to_file(&dump, fp);
    fclose(fp);
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

static inline void _iotdata_print_field(const iotdata_decoded_t *d, FILE *fp, iotdata_field_type_t type, const char *label) {
    const iotdata_field_ops_t *ops = (type >= 0 && type < IOTDATA_FIELD_COUNT) ? _iotdata_field_ops[type] : NULL;
    if (ops && ops->print)
        ops->print(d, fp, label);
}

iotdata_status_t print_decoded_to_file(const iotdata_decoded_t *dec, FILE *fp) {
    const iotdata_variant_def_t *vdef = iotdata_get_variant(dec->variant);

    fprintf(fp, "Station %u seq=%u var=%u (%s) [%zu bits, %zu bytes]\n", dec->station, dec->sequence, dec->variant, vdef->name, dec->packed_bits, dec->packed_bytes);

    for (int si = 0; si < _iotdata_field_count(vdef->num_pres_bytes); si++)
        if (IOTDATA_FIELD_VALID(vdef->fields[si].type) && IOTDATA_FIELD_PRESENT(dec->fields, vdef->fields[si].type))
            _iotdata_print_field(dec, fp, vdef->fields[si].type, vdef->fields[si].label);

#if defined(IOTDATA_ENABLE_TLV)
    if (IOTDATA_FIELD_PRESENT(dec->fields, IOTDATA_FIELD_TLV))
        print_tlv(dec, fp, "Data");
#endif

    return IOTDATA_OK;
}

iotdata_status_t iotdata_print_decoded_to_string(const iotdata_decoded_t *dec, char *out, size_t out_size) {
    iotdata_status_t rc;
    FILE *fp = fmemopen(out, out_size, "w");
    if (!fp)
        return IOTDATA_ERR_PRINT_ALLOC;
    rc = print_decoded_to_file(dec, fp);
    fclose(fp);
    return rc;
}

iotdata_status_t iotdata_print_to_file(const uint8_t *buf, size_t len, FILE *fp) {
    iotdata_decoded_t dec;
    iotdata_status_t rc;
    if ((rc = iotdata_decode(buf, len, &dec)) != IOTDATA_OK)
        return rc;
    return print_decoded_to_file(&dec, fp);
}

iotdata_status_t iotdata_print_to_string(const uint8_t *buf, size_t len, char *out, size_t out_size) {
    iotdata_decoded_t dec;
    iotdata_status_t rc;
    if ((rc = iotdata_decode(buf, len, &dec)) != IOTDATA_OK)
        return rc;
    FILE *fp = fmemopen(out, out_size, "w");
    if (!fp)
        return IOTDATA_ERR_PRINT_ALLOC;
    rc = print_decoded_to_file(&dec, fp);
    fclose(fp);
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

        _IOTDATA_ERR_TLV

        _IOTDATA_ERR_BATTERY
        _IOTDATA_ERR_LINK
        _IOTDATA_ERR_TEMPERATURE
        _IOTDATA_ERR_PRESSURE
        _IOTDATA_ERR_HUMIDITY
        _IOTDATA_ERR_WIND_SPEED
        _IOTDATA_ERR_WIND_DIRECTION
        _IOTDATA_ERR_WIND_GUST
        _IOTDATA_ERR_RAIN_RATE
        _IOTDATA_ERR_RAIN_SIZE
        _IOTDATA_ERR_SOLAR
        _IOTDATA_ERR_CLOUDS
        _IOTDATA_ERR_AIR_QUALITY
        _IOTDATA_ERR_AIR_QUALITY_INDEX
        _IOTDATA_ERR_AIR_QUALITY_PM
        _IOTDATA_ERR_AIR_QUALITY_GAS
        _IOTDATA_ERR_RADIATION_CPM
        _IOTDATA_ERR_RADIATION_DOSE
        _IOTDATA_ERR_DEPTH
        _IOTDATA_ERR_POSITION
        _IOTDATA_ERR_DATETIME
        _IOTDATA_ERR_IMAGE
        _IOTDATA_ERR_FLAGS

    default:
        return "Unknown error";
    }
}

#endif /* !IOTDATA_NO_ERROR_STRINGS */

/* =========================================================================
 * End
 * ========================================================================= */
