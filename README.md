
# IoT Sensor Telemetry Protocol (iotdata)

## Specification

```
    Title:      IoT Sensor Telemetry Protocol
    Version:    1
    Status:     Running Code
    Created:    2026-02-07
    Authors:    Matthew Gream
    Licence:    [TBD]
    Repository: https://github.com/matthewgream/libiotdata

```

## Status of This Document

This document specifies a bit-packed telemetry protocol for
battery-constrained IoT sensor systems, with particular emphasis on
LoRa-based remote environmental monitoring.  The protocol has a
reference implementation in C (`libiotdata`) which is the normative
source for any ambiguity in this specification.  In the tradition of
RFC 1, the specification is informed by running code.

Discussion of this document and the reference implementation takes
place on the project's GitHub repository.

## Table of Contents

- [1. Introduction](#1-introduction)
- [2. Conventions and Terminology](#2-conventions-and-terminology)
- [3. Design Principles](#3-design-principles)
- [4. Packet Structure Overview](#4-packet-structure-overview)
- [5. Header](#5-header)
- [6. Presence Bytes](#6-presence-bytes)
- [7. Variant Definitions](#7-variant-definitions)
- [8. Field Encodings](#8-field-encodings)
  - [8.1. Battery](#81-battery)
  - [8.2. Link](#82-link)
  - [8.3. Environment](#83-environment)
  - [8.4. Solar](#84-solar)
  - [8.5. Depth](#85-depth)
  - [8.6. Flags](#86-flags)
  - [8.7. Position](#87-position)
  - [8.8. Datetime](#88-datetime)
  - [8.9. Temperature (standalone)](#89-temperature-standalone)
  - [8.10. Pressure (standalone)](#810-pressure-standalone)
  - [8.11. Humidity (standalone)](#811-humidity-standalone)
  - [8.12. Wind (bundle)](#812-wind-bundle)
  - [8.13. Wind Speed (standalone)](#813-wind-speed-standalone)
  - [8.14. Wind Direction (standalone)](#814-wind-direction-standalone)
  - [8.15. Wind Gust (standalone)](#815-wind-gust-standalone)
  - [8.16. Rain (bundle)](#816-rain-bundle)
  - [8.17. Rain Rate (standalone)](#817-rain-rate-standalone)
  - [8.18. Rain Size (standalone)](#818-rain-rate-standalone)
  - [8.19. Air Quality](#819-air-quality)
  - [8.20. Radiation (bundle)](#820-radiation-bundle)
  - [8.21. Radiation CPM (standalone)](#821-radiation-cpm-standalone)
  - [8.22. Radiation Dose (standalone)](#822-radiation-dose-standalone)
  - [8.23. Clouds](#823-clouds)
- [9. TLV Data](#9-tlv-data)
  - [9.1. TLV Header](#91-tlv-header)
  - [9.2. Raw Format](#92-raw-format)
  - [9.3. Packed String Format](#93-packed-string-format)
  - [9.4. Well-Known TLV Types](#94-well-known-tlv-types)
- [10. Canonical JSON Representation](#10-canonical-json-representation)
- [11. Receiver Considerations](#11-receiver-considerations)
  - [11.1. Datetime Year Resolution](#111-datetime-year-resolution)
  - [11.2. Position Source Ambiguity](#112-position-source-ambiguity)
  - [11.3. Sensor Metadata and Interoperability](#113-sensor-metadata-and-interoperability)
  - [11.4. Unknown Variants](#114-unknown-variants)
  - [11.5. Quantisation Error Budgets](#115-quantisation-error-budgets)
- [12. Packet Size Reference](#12-packet-size-reference)
- [13. Implementation Notes](#13-implementation-notes)
  - [13.1. Reference Implementation](#131-reference-implementation)
  - [13.2. Encoder Strategy](#132-encoder-strategy)
  - [13.3. Compile-Time Options](#133-compile-time-options)
  - [13.4. Build Size](#134-build-size)
  - [13.5. Variant Table Extension](#135-variant-table-extension)
- [14. Security Considerations](#14-security-considerations)
- [15. Future Work](#15-future-work)
- [Appendix A. 6-Bit Character Table](#appendix-a-6-bit-character-table)
- [Appendix B. Quantisation Worked Examples](#appendix-b-quantisation-worked-examples)
- [Appendix C. Complete Encoder Example](#appendix-c-complete-encoder-example)
- [Appendix D. Transmission Medium Considerations](#appendix-d-transmission-medium-considerations)
  - [D.1. Design Principle: One Frame, One Transmission](#d1-design-principle-one-frame-one-transmission)
  - [D.2. LoRa (Raw PHY)](#d2-lora-raw-phy)
  - [D.3. LoRaWAN](#d3-lorawan)
  - [D.4. Sigfox](#d4-sigfox)
  - [D.5. IEEE 802.11ah (Wi-Fi HaLow)](#d5-ieee-80211ah-wi-fi-halow)
  - [D.6. Cellular IoT (NB-IoT, LTE-M)](#d6-cellular-iot-nb-iot-lte-m)
  - [D.7. Medium Selection Summary](#d7-medium-selection-summary)
- [Appendix E. System Implementation Considerations](#appendix-e-system-implementation-considerations)
  - [E.1. Microcontoller Class Taxonomy](#e1-microcontroller-class-taxonomy)
  - [E.2. Memory Footprint](#e2-memory-footprint)
  - [E.3. Encoder Architecture: Store-Then-Pack](#e3-encoder-architecture-store-then-pack)
  - [E.4. Encoder Alternative: Pack-As-You-Go](#e4-alternative-pack-as-you-go)
  - [E.5. Compile-Time Field Stripping (#ifdef)](#e5-compile-time-field-stripping-ifdef)
  - [E.6. Floating Point Considerations](#e6-floating-point-considerations)
  - [E.7. Dependencies and Portability](#e7-dependencies-and-portability)
  - [E.8. Stack vs Heap Allocation](#e8-stack-vs-heap-allocation)
  - [E.9. Endianness](#e9-endianness)
  - [E.10. Real-Time Considerations](#e10-real-time-considerations)
  - [E.11. Platform-Specific Notes](#e11-platform-specific-notes)
  - [E.12. Class 1 Hand-Rolled Encoder Example](#e12-class-1-hand-rolled-encoder-example)
- [Appendix F. Example Weather Station Output](#appendix-f-example-weather-station-output)

---

## 1. Introduction

Remote environmental monitoring systems — weather stations, snow depth
sensors, ice thickness gauges — are frequently deployed in locations
without mains power or wired connectivity.  These devices are
constrained along three axes simultaneously:

  1. **Power** — battery and/or small solar panel, often in locations
     with limited winter daylight.

  2. **Transmission** — LoRa, 802.11ah, SigFox, cellular SMS,
     low-frequency RF, or similar low-power point-to-point, wide-area,
     or mesh networks with effective payload limits of tens of bytes
     per transmission.  Regulatory limits on transmission time
     (typically 1% duty cycle in EU ISM bands) mean that every byte
     transmitted has a direct cost in time-on-air and energy.

  3. **Processing** — small, inexpensive embedded microcontrollers
     running at tens of megahertz with tens or hundreds of kilobytes
     of RAM and program storage, where code size and complexity are
     real constraints.

Existing serialisation approaches — JSON, Protobuf, CBOR, even raw
C structs — waste bits on byte alignment, field delimiters, schema
metadata, or fixed-width fields for data that could be represented
in far fewer bits.

The IoT Sensor Telemetry Protocol (iotdata) addresses this by
defining a bit-packed wire format where each field is quantised to
the minimum number of bits required for its operational range and
resolution.  A typical weather station packet — battery, link
quality, temperature, pressure, humidity, wind speed, direction and
gust, rain rate and drop size, solar irradiance and UV index, plus
8 flag bits — fits in 16 bytes.  A full-featured packet adding air
quality, cloud cover, radiation, position, and timestamp fits in
32 bytes.

The protocol is designed for transmit-only devices.  There is no
negotiation, handshake, or acknowledgement at this layer.  A sensor
wakes, encodes its readings, transmits, and sleeps.  Transmissions
are typically infrequent (minutes to hours), bursty, and rely on
lower-layer integrity (checksums or CRC) without lower-layer
reliability (retransmission or acknowledgement).

## 2. Conventions and Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
"SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this
document are to be interpreted as described in RFC 2119.

**Bit numbering:**  All bit diagrams in this document use MSB-first
(big-endian) bit order.  Bit 7 of a byte is the most significant bit
and is transmitted first.  Multi-bit fields are packed MSB-first: the
most significant bit of a field occupies the earliest bit position in
the stream.

**Bit offset:**  Bit positions within a packet are numbered from 0,
starting at the MSB of the first byte.  Bit 0 is the MSB of byte 0;
bit 7 is the LSB of byte 0; bit 8 is the MSB of byte 1; and so on.

**Byte boundaries:**  Fields are NOT byte-aligned unless they happen to
fall on a byte boundary.  The packet is a continuous bit stream; byte
boundaries have no structural significance.  The final byte is
zero-padded in its least-significant bits if the total bit count is
not a multiple of 8.

**Quantisation:**  The process of mapping a continuous or large-range
value to a reduced set of discrete steps that fit in fewer bits.  All
quantisation in this protocol uses `round()` (round half away from
zero) and can be carried out as floating-point or integer-only.

## 3. Design Principles

The following principles guided the protocol design:

  1. **Bit efficiency over simplicity.**  Every field is quantised to
     the minimum bits that preserve operationally useful resolution.
     There is no byte-alignment padding between fields.

  2. **Presence flags over fixed structure.**  Optional fields are
     indicated by presence bits, so a battery-only packet is 6 bytes
     and a full-telemetry packet is 24 bytes — the same protocol
     serves both.

  3. **Variants over negotiation.**  Different sensor types (snow,
     ice, weather) can prioritise different fields in the compact
     first presence byte.  The 4-bit variant field in the header
     selects the field mapping.  No runtime negotiation is needed.

  4. **Source-agnostic fields.**  Position may come from GNSS, WiFi
     geolocation, cell tower triangulation, or static configuration.
     Datetime may come from GNSS, NTP, or a local RTC.  The wire
     encoding is the same regardless of source.

  5. **Extensibility via TLV.**  Diagnostic data, firmware metadata,
     and user-defined payloads use a trailing TLV (type-length-value)
     section that does not affect the fixed field layout.

  6. **Encode-only on the sensor.**  The encoder is small enough for
     resource-constrained MCUs.  JSON serialisation and other
     server-side features are optional and can be excluded from
     embedded builds.

  7. **Transport-delegated integrity.**  The protocol carries no
     checksum, CRC, length field, or encryption.  These functions
     are delegated to the underlying medium (LoRa CRC, LoRaWAN MIC,
     cellular security, etc.).  A redundant CRC would cost 16-32
     bits — significant when the entire payload may be 46 bits.
     Packet loss is tolerated: the sequence number (Section 5)
     enables detection without requiring retransmission.

## 4. Packet Structure Overview

An iotdata packet consists of the following sections, in order:

```
+--------+------------+-------------+------------+
| Header | Presence   | Data Fields | TLV Fields |
| 32 bits| 8 to 32 b. | variable    | optional   |
+--------+------------+-------------+------------+
```

All sections are packed as a continuous bit stream with no alignment
gaps between them.

  - **Header** (32 bits): Always present.  Identifies the variant,
    station, and sequence number.

  - **Presence** (8 to 32 bits): Always present.  One to four presence
    bytes chained via extension bits indicate which data fields follow.
    data fields and TLV data follow.

  - **Data fields** (variable): Zero or more sensor data fields,
    packed in the order defined by the variant's field table.

  - **TLV fields** (variable, optional): Zero or more type-length-value
    data entries.

The minimum valid packet is 5 bytes (header + one presence byte with
no fields set), though such a packet carries no sensor data and serves
only as a heartbeat.  In practice the minimum useful packet is 6 bytes
(header + presence + battery = 46 bits).

## 5. Header

The header is always the first 32 bits of a packet.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Var  |      Station ID       |           Sequence            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Variant** (4 bits, offset 0):
  Index into the variant field table (Section 7).  Values 0-14 are
  usable.  Value 15 is RESERVED for a future extended header format
  and MUST NOT be used.  An encoder MUST reject variant 15.  A
  decoder encountering variant 15 SHOULD reject the packet.

**Station ID** (12 bits, offset 4):
  Identifies the transmitting station.  Range 0-4095.  Station IDs
  are assigned by the deployment operator; this protocol does not
  define an allocation mechanism.

**Sequence** (16 bits, offset 16):
  Monotonically increasing packet counter, wrapping from 65535 to 0.
  The receiver MAY use this to detect lost packets.  The wrap-around
  is expected and MUST NOT be treated as an error.

## 6. Presence Bytes

Immediately following the header, one or more presence bytes indicate
which data fields are included in the packet.  Presence bytes form
an extension chain: each byte has an extension bit that, when set,
indicates another presence byte follows.

### Presence Byte 0 (always present)

```
 7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+
|Ext|TLV| S5| S4| S3| S2| S1| S0|
+---+---+---+---+---+---+---+---+
```

  - **Ext** (bit 7): Extension flag.  If set, Presence Byte 1 follows
    immediately.  If clear, no further presence bytes exist.

  - **TLV** (bit 6): TLV data flag.  If set, one or more TLV entries
    (Section 9) follow after all data fields.

  - **S0-S5** (bits 5-0): Data fields 0 through 5.  Each bit, when
    set, indicates that the corresponding field (as defined by the
    variant's field table) is present in the packet.  The field data
    appears in field order: S0 first, then S1, S2, and so on.

### Presence Byte N (N ≥ 1, conditional)

Present only when the Ext bit in the preceding presence byte is set.

```
 7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+
|Ext| S6| S5| S4| S3| S2| S1| S0|
+---+---+---+---+---+---+---+---+
```

  - **Ext** (bit 7): Extension flag.  If set, another presence byte
    follows.  This allows chaining of an arbitrary number of presence
    bytes.

  - **S0-S6** (bits 6-0): Data fields for this presence byte.  The
    first extension byte (pres1) carries fields 6-12, the second
    (pres2) carries fields 13-19, and so on.

### Field Capacity

The maximum number of data fields available depends on the number of
presence bytes:

| Presence Bytes | Total Data Fields | Formula          |
|----------------|-----------------|------------------|
| 1 (pres0)      | 6               | 6                |
| 2 (pres0+1)    | 13              | 6 + 7            |
| 3 (pres0+1+2)  | 20              | 6 + 7 + 7        |
| 4 (pres0+1+2+3)| 27              | 6 + 7 + 7 + 7    |

The reference implementation supports up to 4 presence bytes (27
data fields).  In practice, the default weather station variant uses 2
presence bytes for 12 data fields. It is unlikely an implementation
would pratically require more than 2-3 presence bytes.

### Extension Byte Optimisation

The encoder only emits the minimum number of presence bytes needed
for the fields actually present.  If all set fields fit in pres0
(fields 0-5), no extension bytes are emitted, even if the variant
defines fields in pres1.  This optimisation reduces packet size for
common transmissions that include only the most frequently updated
fields.

### Field Ordering

Data fields are packed in strict field order.  First, all set fields
from Presence Byte 0 are packed in order S0, S1, ..., S5.  Then, if
Presence Byte 1 is present, fields S6 through S12 are packed.  The TLV
section (if present) always comes last, after all data fields.

The meaning of each field position — which sensor field type it
represents — is determined entirely by the variant table (Section 7).

## 7. Variant Definitions

The variant field in the header selects a field mapping that determines
which field type occupies each presence bit position.  This mechanism
allows different sensor types to prioritise their most commonly
transmitted fields in Presence Byte 0, while less frequent fields
(such as position and datetime) occupy later presence bytes and only
trigger extension bytes when actually transmitted.

All field encodings (Section 8) are universal and independent of
variant.  The variant affects only which encoding type is associated
with which field position, and which label is used in human-readable
output and JSON serialisation.

### Variant Table Structure

In the reference implementation, each variant is defined as:

```c
typedef struct {
    iotdata_field_type_t  type;   /* encoding type for this field */
    const char           *label;  /* JSON key and display label  */
} iotdata_field_def_t;

typedef struct {
    const char          *name;
    uint8_t              num_pres_bytes;
    iotdata_field_def_t  fields[IOTDATA_MAX_DATA_FIELDS];
} iotdata_variant_def_t;
```

The `fields[]` array is flat: entries 0-5 map to Presence Byte 0,
entries 6-12 to Presence Byte 1, entries 13-19 to Presence Byte 2,
and so on.  Unused trailing fields should have type `IOTDATA_FIELD_NONE`.

### Default Variant: Weather Station

The built-in default variant (variant 0) is a general-purpose weather
station layout.  It is enabled by defining `IOTDATA_VARIANT_MAPS_DEFAULT`
at compile time.

| Pres Byte | Field | Type          | Label          | Bits |
|-----------|-------|---------------|----------------|------|
| 0         | S0    | BATTERY       | battery        | 6    |
| 0         | S1    | LINK          | link           | 6    |
| 0         | S2    | ENVIRONMENT   | environment    | 24   |
| 0         | S3    | WIND          | wind           | 22   |
| 0         | S4    | RAIN          | rain           | 12   |
| 0         | S5    | SOLAR         | solar          | 14   |
| 1         | S6    | CLOUDS        | clouds         | 4    |
| 1         | S7    | AIR_QUALITY   | air_quality    | 9    |
| 1         | S8    | RADIATION     | radiation      | 30   |
| 1         | S9    | POSITION      | position       | 48   |
| 1         | S10   | DATETIME      | datetime       | 24   |
| 1         | S11   | FLAGS         | flags          | 8    |

This layout prioritises the most commonly transmitted weather data
(battery, environment, wind, rain, solar, link quality) in Presence
Byte 0, minimising packet size for routine transmissions.  The less
frequently updated fields (position, datetime, radiation) are placed
in Presence Byte 1 and only add to the packet when present.

Note that the weather station variant uses the ENVIRONMENT and WIND
bundle types (see Section 8.2 and 8.12) rather than their individual
component types.  See Section 8 for a discussion of when to use
bundled vs individual field types.

### Custom Variant Maps

Applications can define their own variant tables at compile time
using the `IOTDATA_VARIANT_MAPS` and `IOTDATA_VARIANT_MAPS_COUNT`
defines.  This completely replaces the default variant table.

```c
/* Define custom variants */
const iotdata_variant_def_t my_variants[] = {
    [0] = {
        .name = "soil_sensor",
        .num_pres_bytes = 1,
        .fields = {
            { IOTDATA_FIELD_BATTERY,     "battery"    },
            { IOTDATA_FIELD_LINK,        "link"       },
            { IOTDATA_FIELD_TEMPERATURE, "soil_temp"  },
            { IOTDATA_FIELD_HUMIDITY,    "soil_moist" },
            { IOTDATA_FIELD_DEPTH,       "soil_depth" },
            { IOTDATA_FIELD_NONE,        NULL         },
        },
    },
};
```

Compile with:

```bash
cc -DIOTDATA_VARIANT_MAPS=my_variants -DIOTDATA_VARIANT_MAPS_COUNT=1 ...
```

Custom variants may use any combination of the 21 field types and
may place them in any field position.  Up to 15 variants can be
registered (variant IDs 0-14; variant 15 is reserved for future
extended header format).

### Registered Variants

| Variant | Name             | Pres Bytes | Fields | Notes                    |
|---------|------------------|------------|-------|--------------------------|
| 0       | weather_station  | 2          | 12    | Default (built-in)       |
| 1-14    | (application)    | —          | —     | User-defined via custom maps |
| 15      | RESERVED         | —          | —     | Extended header format   |

A receiver encountering an unknown variant SHOULD fall back to
variant 0's field mapping and flag the packet as using an unknown
variant (see Section 11.4).

## 8. Field Encodings

Each field type has a fixed bit layout that is independent of which
presence field it occupies.  Fields are always packed MSB-first.

The protocol provides 21 field types.  Some of these exist in both
bundled and individual forms, for cases where they are typically
originating from a sensor that generates all bundled items concurrently.

  - **Environment** (Section 8.3) is a convenience bundle that packs
    temperature, pressure, and humidity into a single 24-bit field.
    The same three measurements are also available as individual field
    types: Temperature (8.9), Pressure (8.10), and Humidity (8.11).
    The encodings and quantisation are identical.

  - **Wind** (Section 8.12) is a convenience bundle that packs wind
    speed, direction, and gust into a single 22-bit field.  The same
    three measurements are also available as individual field types:
    Wind Speed (8.13), Wind Direction (8.14), and Wind Gust (8.15).
    The encodings and quantisation are identical.

  - **Rain** (Section 8.16) is a convenience bundle that packs rain
    rate, and rain size into a single 12-bit field.  The same two
    measurements are also available as individual field types:
    Rate Rate (8.16), and Rain Size (8.17).  The encodings and
    quantisation are identical.

  - **Radiation** (Section 8.20) is a convenience bundle that packs
    radiation cpm, and radiation dose into a single 30-bit field.
    The same two measurements are also available as individual field
    types: Radiation CPM (8.21), and Radiation Dose (8.22).  The
    encodings and quantisation are identical.

A variant definition chooses which form to use.  The default weather
station variant uses the bundled forms (one field each for environment
and wind).  A custom variant might use the individual forms to include
only the specific measurements it needs, or to place them in different
priority positions.

Note that Solar Irradiance and Ultraviolet only exist in bundled form.

### 8.1. Battery

6 bits total.

```
 0   1   2   3   4   5
+---+---+---+---+---+---+
|   Level           |Chg|
|   (5 bits)        |(1)|
+---+---+---+---+---+---+
```

**Level** (5 bits):
  Battery charge level, quantised from 0-100% to 0-31.

  Encode: `q = round(level_pct / 100.0 * 31.0)`

  Decode: `level_pct = round(q / 31.0 * 100.0)`

  Resolution: ~3.2 percentage points.

**Charging** (1 bit):
  1 = charging, 0 = discharging/not charging.

### 8.2. Link

6 bits total.

```
 0   1   2   3   4   5
+---+---+---+---+---+---+
|   RSSI        | SNR   |
|   (4 bits)    | (2)   |
+---+---+---+---+---+---+
```

**RSSI** (4 bits):
  Range: -120 to -60 dBm.
  Resolution: 4 dBm (15 steps).

  Encode: `q = (rssi_dbm - (-120)) / 4`

  Decode: `rssi_dbm = -120 + q * 4`

**SNR** (2 bits):
  Range: -20 to +10 dB.
  Resolution: 10 dB (3 steps: -20, -10, 0, +10).

  Encode: `q = round((snr_db - (-20.0)) / 10.0)`

  Decode: `snr_db = -20.0 + q * 10.0`

This field is source-agnostic: while designed for LoRa link metrics,
the same encoding is suitable for 802.11ah or other low-power RF links
with comparable RSSI and SNR ranges.

### 8.3. Environment

24 bits total.

```
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Temperature  | Pressure      | Humidity      |
|  (9 bits)     | (8 bits)      | (7 bits)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Temperature** (9 bits):
  Range: -40.00°C to +80.00°C.
  Resolution: 0.25°C (480 steps, 9 bits = 512 values).

  Encode: `q = round((temp_c - (-40.0)) / 0.25)`

  Decode: `temp_c = -40.0 + q * 0.25`

**Pressure** (8 bits):
  Range: 850 to 1105 hPa.
  Resolution: 1 hPa (255 steps).

  Encode: `q = pressure_hpa - 850`

  Decode: `pressure_hpa = q + 850`

**Humidity** (7 bits):
  Range: 0 to 100%.
  Resolution: 1% (7 bits = 128 values, 0-100 used).

  Encode/Decode: direct (no quantisation needed).

### 8.4. Solar

14 bits total.

```
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Irradiance    | UV Idx  |
|   (10 bits)     | (4 bits)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Irradiance** (10 bits):
  Range: 0 to 1023 W/m².
  Resolution: 1 W/m².  Direct encoding.

**UV Index** (4 bits):
  Range: 0 to 15.  Direct encoding.

### 8.5. Depth

10 bits total.

Range: 0 to 1023 cm.  Resolution: 1 cm.  Direct encoding.

This is a generic depth field.  The variant label determines its
semantic meaning (snow depth, ice thickness, water level, etc.).
The wire encoding is identical regardless of label.

### 8.6. Flags

8 bits total.

```
 0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|       Flags (8 bits)          |
+---+---+---+---+---+---+---+---+
```

General-purpose bitmask.  Bit assignments are deployment-specific
and are not defined by this protocol.  Example uses include: low
battery warning, sensor fault indicators, tamper detection, or
configuration acknowledgement flags.

### 8.7. Position

48 bits total.

```
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Latitude (24 bits)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Longitude (24 bits)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Latitude** (24 bits):
  Range: -90.0° to +90.0°.

  Encode: `q = round((lat - (-90.0)) / 180.0 * 16777215.0)`

  Decode: `lat = q / 16777215.0 * 180.0 + (-90.0)`

  Resolution: 180.0 / 16777215 ≈ 0.00001073°
  ≈ 1.19 metres at the equator.

**Longitude** (24 bits):
  Range: -180.0° to +180.0°.

  Encode: `q = round((lon - (-180.0)) / 360.0 * 16777215.0)`

  Decode: `lon = q / 16777215.0 * 360.0 + (-180.0)`

  Resolution: 360.0 / 16777215 ≈ 0.00002146°
  ≈ 2.39 metres at the equator, reducing with cos(latitude).

This field is source-agnostic.  The position may originate from a GNSS
receiver, WiFi geolocation, cell tower triangulation, or static
configuration.  The protocol does not indicate the source or its
accuracy; see Section 11.2 and 11.3 for discussion.

### 8.8. Datetime

24 bits total.

```
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Ticks (24 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Ticks** (24 bits):
  Time offset from January 1 00:00:00 UTC of the current year,
  measured in 5-second ticks.

  Encode: `ticks = seconds_from_year_start / 5`

  Decode: `seconds = ticks * 5`

  Maximum value: 16,777,215 ticks = 83,886,075 seconds ≈ 970.9 days.

  Resolution: 5 seconds.

The year is NOT transmitted.  The receiver resolves the year using
its own clock; see Section 11.1 for the year resolution algorithm.

This field is source-agnostic.  The time may originate from a GNSS
receiver, NTP synchronisation, or a local RTC.  The protocol does not
indicate the source or its drift characteristics; see Section 11.3.

### 8.9. Temperature (standalone)

9 bits total.

Range: -40.00°C to +80.00°C.
Resolution: 0.25°C (480 steps, 9 bits = 512 values).

  Encode: `q = round((temp_c - (-40.0)) / 0.25)`

  Decode: `temp_c = -40.0 + q * 0.25`

This is the same encoding as the temperature component of the
Environment bundle (Section 8.2).  Use this standalone type in
variants that need temperature without pressure and humidity.

### 8.10. Pressure (standalone)

8 bits total.

Range: 850 to 1105 hPa.
Resolution: 1 hPa (255 steps).

  Encode: `q = pressure_hpa - 850`

  Decode: `pressure_hpa = q + 850`

This is the same encoding as the pressure component of the
Environment bundle (Section 8.2).

### 8.11. Humidity (standalone)

7 bits total.

Range: 0 to 100%.
Resolution: 1% (7 bits = 128 values, 0-100 used).

  Encode/Decode: direct (no quantisation needed).

This is the same encoding as the humidity component of the
Environment bundle (Section 8.2).

### 8.12. Wind (bundle)

22 bits total.

```
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Speed      | Direction     |  Gust       |
|  (7 bits)   | (8 bits)      | (7 bits)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs wind speed, direction, and
gust speed into a single field.  The component encodings are identical
to the standalone Wind Speed (8.13), Wind Direction (8.14), and
Wind Gust (8.15) types.

**Speed** (7 bits):
  Range: 0 to 63.5 m/s.  Resolution: 0.5 m/s.

  Encode: `q = round(speed_ms / 0.5)`

  Decode: `speed_ms = q * 0.5`

**Direction** (8 bits):
  Range: 0° to 355° (true bearing).  Resolution: ~1.41° (360/256).

  Encode: `q = round(direction_deg / 360.0 * 256.0) & 0xFF`

  Decode: `direction_deg = q / 256.0 * 360.0`

**Gust** (7 bits):
  Range: 0 to 63.5 m/s.  Resolution: 0.5 m/s.

  Encode/Decode: same as Speed.

### 8.13. Wind Speed (standalone)

7 bits total.

Range: 0 to 63.5 m/s.  Resolution: 0.5 m/s.

  Encode: `q = round(speed_ms / 0.5)`

  Decode: `speed_ms = q * 0.5`

Same encoding as the speed component of the Wind bundle (8.12).

### 8.14. Wind Direction (standalone)

8 bits total.

Range: 0° to 355° (true bearing).  Resolution: ~1.41° (360/256).

  Encode: `q = round(direction_deg / 360.0 * 256.0) & 0xFF`

  Decode: `direction_deg = q / 256.0 * 360.0`

Same encoding as the direction component of the Wind bundle (8.12).

### 8.15. Wind Gust (standalone)

7 bits total.

Range: 0 to 63.5 m/s.  Resolution: 0.5 m/s.

  Encode: `q = round(gust_ms / 0.5)`

  Decode: `gust_ms = q * 0.5`

Same encoding as the gust component of the Wind bundle (8.12).

### 8.16. Rain (bundle)

12 bits total.

```
 0                   1  
 0 1 2 3 4 5 6 7 8 9 0 1 
+-+-+-+-+-+-+-+-+-+-+-+-+
|  Rate         | Size  |
|  (8 bits)     | (4)   |
+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs rain rate, and size into a
single field.  The component encodings are identical to the
standalone Rain Rate (8.17), and Rain Size (8.18) types.

**Rate** (8 bits):

  Range: 0 to 255 mm/hr.
  Resolution: 1 mm/hr.  Direct encoding.

**Size** (4 bits):

  Range: 0 to 6.0mm.
  Resolution: 0.25 mm.

  Encode: `q = round(rain_size / 0.25)`

  Decode: `rain_size = q * 0.25`

### 8.17. Rain Rate (standalone)

8 bits total.

Range: 0 to 255 mm/hr.
Resolution: 1 mm/hr.  Direct encoding.

### 8.18. Rain Size (standalone)

4 bits total.

Range: 0 to 6.0mm.
Resolution: 0.25 mm.

Encode: `q = round(rain_size / 0.25)`

Decode: `rain_size = q * 0.25`

### 8.19. Air Quality

9 bits total.

Range: 0 to 500 AQI (Air Quality Index).
Resolution: 1 AQI.  Direct encoding (9 bits = 512 values, 0-500 used).

### 8.20. Radiation (bundle)

28 bits total.

```
 0                   1                   2                 
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  CPM                      | Dose                      |
|  (14 bits)                | (14 bits)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs radiation CPM, and dose
into a single field.  The component encodings are identical to the
standalone Radiation CPM (8.21), and Radiation Dose (8.22) types.

**CPM** (14 bits):

  Range: 0 to 16383 counts per minute (CPM).
  Resolution: 1 CPM.  Direct encoding.

  This field carries the raw count rate from a Geiger-Müller tube or
  similar radiation detector.

**Dose** (14 bits):

  Range: 0 to 163.83 µSv/h.
  Resolution: 0.01 µSv/h (16,383 steps).

    Encode: `q = round(dose_usvh / 0.01)`

    Decode: `dose_usvh = q * 0.01`

  This field carries the computed dose rate.  The relationship
  between CPM and dose rate is detector-specific and is not defined
  by this protocol.

### 8.21. Radiation CPM (standalone)

14 bits total.

Range: 0 to 16383 counts per minute (CPM).
Resolution: 1 CPM.  Direct encoding.

This field carries the raw count rate from a Geiger-Müller tube or
similar radiation detector.

### 8.22. Radiation Dose (standalone)

14 bits total.

Range: 0 to 163.83 µSv/h.
Resolution: 0.01 µSv/h (16,383 steps).

  Encode: `q = round(dose_usvh / 0.01)`

  Decode: `dose_usvh = q * 0.01`

This field carries the computed dose rate.  The relationship between
CPM and dose rate is detector-specific and is not defined by this
protocol.

### 8.23. Clouds

4 bits total.

Range: 0 to 8 okta.
Resolution: 1 okta.  Direct encoding (4 bits = 16 values, 0-8 used).

Clouds measures cloud cover in okta (eighths of sky covered), following
the standard meteorological convention where 0 = clear sky and
8 = fully overcast.

## 9. TLV Data

The TLV (Type-Length-Value) section provides an extensible mechanism
for diagnostic data, firmware metadata, user-defined payloads, and
future sensor metadata.  It is present only when the TLV bit (bit 6
of Presence Byte 0) is set.

The TLV section begins immediately after the last data field, at
whatever bit offset that field ended.  There is no alignment padding.

### 9.1. TLV Header

Each TLV entry begins with a 16-bit header:

```
 0                               1
 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
|Fmt|       Type (6)        |Mor|           Length (8)          |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

**Format** (1 bit):
  0 = raw bytes.  1 = packed 6-bit string.

**Type** (6 bits):
  Application-defined type identifier, range 0-63.
  See Section 9.4 for well-known types.

**More** (1 bit):
  1 = another TLV entry follows this one.
  0 = this is the last TLV entry.

**Length** (8 bits):
  For raw format: number of data bytes (0-255).
  For string format: number of characters (0-255).

### 9.2. Raw Format

When Format = 0, the data section is `Length` bytes (Length × 8 bits),
packed MSB-first with no alignment.

Total TLV entry size: 16 + (Length × 8) bits.

### 9.3. Packed String Format

When Format = 1, each character is encoded as 6 bits using the
character table in Appendix A.  This saves 25% compared to 8-bit
ASCII for the supported character set (alphanumeric plus space).

Total TLV entry size: 16 + (Length × 6) bits.

Characters outside the 6-bit table MUST NOT be transmitted.  An
encoder MUST reject strings containing unencodable characters.

### 9.4. Well-Known TLV Types

| Type      | Name        | Format | Description                                    |
|-----------|-------------|--------|------------------------------------------------|
| 0x01-0x0F | (reserved)  | —      | Reserved for globally designated TLVs          |
| 0x01      | UPTIME      | raw    | Device uptime in seconds (4 bytes, big-endian) |
| 0x02      | RESETS      | raw    | Reboot counter (2 bytes, big-endian)           |
| 0x03      | LOGGER      | string | Last logger error/warning message              |
| 0x04      | CONFIG      | raw    | Configuration snapshot                         |
| 0x05      | USERDATA    | string | User interaction event (e.g. button press)     |
| 0x10-0x1F | (reserved)  | —      | Reserved for future quality/metadata TLVs      |
| 0x20-     | (available) | —      | Available for propretiary TLVs                 |

Types 0x01-0x0F are reserved for globally designated types, as specified
in, and extended by, this document.  Types 0x10-0x1F are reserved for
sensor metadata (see Section 11.3 and Section 15).  Types 0x20 onwards are
available for application use.

## 10. Canonical JSON Representation

Gateways and servers typically convert binary packets to JSON for
storage, forwarding, and human inspection.  The reference
implementation provides bidirectional conversion (`iotdata_decode_to_json`
and `iotdata_encode_from_json`) with the following canonical mapping.

The JSON field names are derived from the variant's field labels, so
the same binary encoding may produce different JSON keys depending on
variant.  For example, a default weather station variant produces
`"wind"` as a bundled JSON object, while a custom variant using
individual wind fields produces separate `"wind_speed"`, `"wind_dir"`,
and `"wind_gust"` keys.  Similarly, the DEPTH field type may produce
`"snow_depth"`, `"soil_depth"`, or any other label depending on the
variant definition.

### Example JSON (Variant 0, weather_station)

```json
{
  "variant": 0,
  "station": 42,
  "sequence": 1234,
  "packed_bits": 120,
  "packed_bytes": 15,
  "battery": {
    "level": 84,
    "charging": false
  },
  "link": {
    "rssi": -96,
    "snr": 10.0
  }
  "environment": {
    "temperature": 21.5,
    "pressure": 1013,
    "humidity": 45
  },
  "wind": {
    "speed": 5.0,
    "direction": 180,
    "gust": 8.5
  },
  "rain": { 
    "rate": 3,
    "size": 2.5,
  },,
  "solar": {
    "irradiance": 850,
    "ultraviolet": 7
  },
}
```

### TLV in JSON

TLV entries are represented as an array under `"data"`:

```json
{
  "data": [
    { "type": 1, "format": "raw", "data": "00015180" },
    { "type": 5, "format": "string", "data": "Button A pressed" }
  ]
}
```

Raw TLV data is hex-encoded.  String TLV data is stored as a plain
JSON string.  The `"format"` field indicates which encoding was used
on the wire.

### Round-Trip Guarantee

The JSON representation MUST support lossless round-trip conversion:
encoding a packet to binary, decoding to JSON, re-encoding from JSON,
and comparing the resulting binary MUST produce an identical byte
sequence.  The reference implementation test suite verifies this
property.

## 11. Receiver Considerations

This section describes algorithms and considerations that apply to
the receiving side (gateway, server, or any device decoding packets).

### 11.1. Datetime Year Resolution

The datetime field encodes seconds from the start of the current year
but does not transmit the year.  The receiver MUST resolve the year
using the following algorithm:

  1. Let `T_rx` be the receiver's current UTC time.
  2. Let `Y` be the year component of `T_rx`.
  3. Decode the datetime field to obtain `S` seconds from year start.
  4. Compute `T_decoded = Y-01-01T00:00:00Z + S seconds`.
  5. If `T_decoded` is more than 6 months in the future relative to
     `T_rx`, subtract one year: `T_decoded = (Y-1)-01-01T00:00:00Z + S`.

This handles the year boundary: a packet timestamped December 31 and
received January 1 is correctly attributed to the previous year.

The 24-bit field at 5-second resolution supports approximately 971
days, so the encoding does not wrap within a single year.

The accuracy of the decoded timestamp depends on the accuracy of the
transmitter's time source.  For GNSS-synchronised devices this is
typically sub-second; for free-running RTC devices it may drift by
seconds per day.  See Section 11.3.

### 11.2. Position Source Ambiguity

The position field (Section 8.7) encodes latitude and longitude
without indicating the source.  The practical accuracy varies
significantly by source:

  - **GNSS (GPS/Galileo/GLONASS):** typically 2-5 metre accuracy,
    well within the ~1.2m quantisation of the 24-bit encoding.

  - **WiFi geolocation:** typically 15-50 metre accuracy.  The
    quantisation error is negligible relative to the source error.

  - **Cell tower:** typically 100-1000 metre accuracy.

  - **Static configuration:** the operator programmes the known
    coordinates at deployment time.  Accuracy depends on the method
    used (surveyed, map click, etc.).

In a closed system where the operator controls all devices and knows
their position sources, this ambiguity is acceptable.  For open or
interoperable systems, the source and accuracy SHOULD be communicated
via a sensor metadata TLV (Section 11.3).

Similarly, for fixed-position sensors, the position field is
typically transmitted once at startup or periodically at a low rate,
not on every packet.  The receiver SHOULD cache the last known
position for a station and associate it with subsequent packets that
omit position.

### 11.3. Sensor Metadata and Interoperability

The core protocol deliberately omits sensor metadata such as:

  - Sensor type (NTC thermistor, BME280, SHT40, etc.)
  - Measurement accuracy or precision class
  - Position source (GNSS, static config, etc.)
  - Time source (GNSS, NTP, free-running RTC, etc.)
  - Calibration date or coefficients

In a **closed system** — where one operator controls all devices and
the gateway software — this information is known out-of-band.  The
operator knows that station 42 uses a BME280 for environment readings
with ±1°C accuracy, has a static position programmed at deployment,
and synchronises time via NTP.  No wire overhead is needed.

For **interoperable systems** — where devices from different
manufacturers or deployments share a common receiver — sensor metadata
becomes important.  The protocol reserves TLV types 0x10-0x1F for
future standardised metadata TLVs that could convey:

  - Source type per field (e.g. "position source = GNSS" or
    "temperature sensor = BME280")
  - Accuracy class or error bounds
  - Calibration metadata

The design of these metadata TLVs is deferred to a future revision of
this specification.  Implementers requiring interoperability before
that revision MAY use application-defined TLV types (0x20-0x3F) for
this purpose, with the understanding that these are not standardised.

This approach follows a deliberate design philosophy: add wire
overhead only when it is needed.  A snow depth sensor transmitting to
its own gateway every 15 minutes on a coin cell battery should not pay
the cost of metadata bytes that the receiver already knows.

### 11.4. Unknown Variants

A receiver encountering a variant number that it does not have a table
entry for SHOULD:

  1. Fall back to variant 0's field mapping for decoding.
  2. Flag the packet as using an unknown variant in its output
     (e.g. a warning in the print output or a field in the JSON).
  3. NOT reject the packet, since the field encodings are universal
     and the data is likely still meaningful.

In the reference implementation, `iotdata_get_variant()` returns
variant 0's table as a fallback for any unknown variant number.

### 11.5. Quantisation Error Budgets

Receivers should be aware of the quantisation errors inherent in the
encoding.  These are systematic and deterministic — not noise — and
SHOULD be accounted for in any downstream processing.

| Field            | Bits | Range             | Resolution   | Max quant error |
|------------------|------|-------------------|--------------|-----------------|
| Battery level    | 5    | 0-100%            | ~3.23%       | ±1.6%           |
| Link RSSI        | 4    | -120 to -60 dBm   | 4 dBm        | ±2 dBm          |
| Link SNR         | 2    | -20 to +10 dB     | 10 dB        | ±5 dB           |
| Temperature      | 9    | -40 to +80°C      | 0.25°C       | ±0.125°C        |
| Pressure         | 8    | 850-1105 hPa      | 1 hPa        | ±0.5 hPa        |
| Humidity         | 7    | 0-100%            | 1%           | ±0.5%           |
| Wind speed       | 7    | 0-63.5 m/s        | 0.5 m/s      | ±0.25 m/s       |
| Wind direction   | 8    | 0-355°            | ~1.41°       | ±0.7°           |
| Wind gust        | 7    | 0-63.5 m/s        | 0.5 m/s      | ±0.25 m/s       |
| Rain rate        | 8    | 0-255 mm/hr       | 1 mm/hr      | ±0.5 mm/hr      |
| Rain size        | 4    | 0-6.0 mm/d        | 0.25 mm/d    | ±0.5 mm/d       |
| Solar Irradiance | 10   | 0-1023 W/m²       | 1 W/m²       | ±0.5 W/m²       |
| Solar UV Index   | 4    | 0-15              | 1            | ±0.5            |
| Clouds           | 4    | 0-8 okta          | 1 okta       | ±0.5 okta       |
| Air quality      | 9    | 0-500 AQI         | 1 AQI        | ±0.5 AQI        |
| Radiation CPM    | 16   | 0-65535 CPM       | 1 CPM        | ±0.5 CPM        |
| Radiation dose   | 14   | 0-163.83 µSv/h    | 0.01 µSv/h   | ±0.005 µSv/h    |
| Depth            | 10   | 0-1023 cm         | 1 cm         | ±0.5 cm         |
| Latitude         | 24   | -90° to +90°      | ~0.00001073° | ~0.6 m          |
| Longitude        | 24   | -180° to +180°    | ~0.00002146° | ~1.2 m (eq)     |
| Datetime         | 24   | 0-83.9M seconds   | 5 s          | ±2.5 s          |

These quantisation errors are generally smaller than the measurement
uncertainty of the sensors themselves.  For example, a typical BME280
temperature sensor has ±1°C accuracy, well above the 0.125°C
quantisation error.

## 12. Packet Size Reference

The following table shows exact bit and byte counts for common
packet configurations, using variant 0 (weather_station).

| Scenario                          | Fields                                        | Bits | Bytes |
|-----------------------------------|-----------------------------------------------|------|-------|
| Heartbeat (no data)               | header + pres0                                | 40   | 5     |
| Minimal (battery only)            | + battery                                     | 46   | 6     |
| Battery + environment             | + battery, environment                        | 70   | 9     |
| Typical pres0 (bat+env+wind+rain) | + battery, environment, wind, rain_rate       | 100  | 13    |
| Full pres0 (all 6 fields)         | + battery, env, wind, rain, solar, link       | 120  | 15    |
| Full station (all 12 fields)      | + all 12 field types (pres0 + pres1)          | 253  | 32    |

For comparison, the equivalent data in JSON would typically be
200-600 bytes, and in a packed C struct with byte alignment would be
40-60 bytes.

## 13. Implementation Notes

### 13.1. Reference Implementation

The reference implementation (`libiotdata`) is written in C11 and
targets both embedded systems (ESP32-C3, STM32, nRF52, Raspberry Pi)
and Linux gateways/servers.  It consists of:

  - `iotdata.h` — Public API, constants, and type definitions.
  - `iotdata.c` — Encoder, decoder, JSON, print, and dump.
  - `test_default.c` — Test suite for the default variant (31 tests).
  - `test_custom.c` — Test suite for custom variant maps (14 tests).
  - `test_version.c` — Test smoke evaluation for build versions.
  - `test_example.c` — Test example for a periodic weather station.
  - `Makefile` — Builds `libiotdata.a` static library and tests.

Build:

```bash
make                # Build library and both test suites
make test           # Build and run default and custom tests
make test-example   # Build and run example test
make test-versions  # Build and run versions tests
make lib            # Build static library only
make minimal        # Measure minimal encoder-only build
```

Dependencies: C11 compiler, `libm`, and `cJSON` (optional, only
required for JSON serialisation).

### 13.2. Encoder Strategy

The encoder uses a "store then pack" strategy:

  1. `iotdata_encode_begin()` initialises the context with header
     values and a buffer pointer.
  2. `iotdata_encode_battery()`, `iotdata_encode_environment()`, etc.
     validate inputs and store native-typed values in the context.
     Fields may be added in any order.
  3. `iotdata_encode_end()` performs all bit-packing in a single pass,
     consulting the variant field table to determine field order and
     presence byte layout.

This separation means that the encoder validates eagerly (at the
`encode_*()` call site where the developer can handle the error) and
packs lazily (in one pass, knowing the complete field set).

```c
/* Example: encode a weather station packet */
#define IOTDATA_VARIANT_MAPS_DEFAULT
#include "iotdata.h"

iotdata_encoder_t enc;
uint8_t buf[64];
size_t len;

iotdata_encode_begin(&enc, buf, sizeof(buf), 0, 42, seq++);
iotdata_encode_battery(&enc, 84, false);
iotdata_encode_link(&enc, -95, 8.5f);
iotdata_encode_environment(&enc, 21.5f, 1013, 45);
iotdata_encode_wind(&enc, 5.2f, 180.0f, 8.7f);
iotdata_encode_rain(&enc, 3, 15); // x10 units
iotdata_encode_solar(&enc, 850, 7);
iotdata_encode_end(&enc, &len);
/* buf[0..len-1] is now a 15-byte packet */
```

### 13.3. Compile-Time Options

The library supports extensive compile-time configuration to minimise
code size and memory usage on constrained targets.

**Variant selection:**

| Define                          | Effect |
|---------------------------------|--------|
| `IOTDATA_VARIANT_MAPS_DEFAULT`  | Enable built-in weather station variant |
| `IOTDATA_VARIANT_MAPS=<sym>`    | Use custom variant map array |
| `IOTDATA_VARIANT_MAPS_COUNT=<n>`| Number of entries in custom map |

**Field support compilation:**

| Define                          | Effect |
|---------------------------------|--------|
| `IOTDATA_ENABLE_SELECTIVE`      | Only compile elements explicitly enabled below |
| `IOTDATA_ENABLE_BATTERY`        | Compile battery field |
| `IOTDATA_ENABLE_LINK`           | Compile link field |
| `IOTDATA_ENABLE_ENVIRONMENT`    | Compile environment bundle |
| `IOTDATA_ENABLE_TEMPERATURE`    | Compile temperature field |
| `IOTDATA_ENABLE_PRESSURE`       | Compile pressure field |
| `IOTDATA_ENABLE_HUMIDITY`       | Compile humidity field |
| `IOTDATA_ENABLE_WIND`           | Compile wind bundle |
| `IOTDATA_ENABLE_WIND_SPEED`     | Compile wind speed field |
| `IOTDATA_ENABLE_WIND_DIR`       | Compile wind direction field |
| `IOTDATA_ENABLE_WIND_GUST`      | Compile wind gust field |
| `IOTDATA_ENABLE_RAIN`           | Compile rain bundle |
| `IOTDATA_ENABLE_RAIN_RATE`      | Compile rain rate field |
| `IOTDATA_ENABLE_RAIN_SIZE`      | Compile rain size field |
| `IOTDATA_ENABLE_SOLAR`          | Compile solar field |
| `IOTDATA_ENABLE_CLOUDS`         | Compile clouds field |
| `IOTDATA_ENABLE_AIR_QUALITY`    | Compile air quality field |
| `IOTDATA_ENABLE_RADIATION`      | Compile radiation bundle |
| `IOTDATA_ENABLE_RADIATION_CPM`  | Compile radiation CPM field |
| `IOTDATA_ENABLE_RADIATION_DOSE` | Compile radiation dose field |
| `IOTDATA_ENABLE_DEPTH`          | Compile depth field |
| `IOTDATA_ENABLE_POSITION`       | Compile position field |
| `IOTDATA_ENABLE_DATETIME`       | Compile datetime field |
| `IOTDATA_ENABLE_FLAGS`          | Compile flags field |
| `IOTDATA_ENABLE_TLV`            | Compile TLV fields |

When `IOTDATA_ENABLE_SELECTIVE` is defined, only the element types
with their corresponding `IOTDATA_ENABLE_xxx` defined will be
compiled.  When `IOTDATA_VARIANT_MAPS_DEFAULT` is defined (without
`IOTDATA_ENABLE_SELECTIVE`), all elements used by the default weather
station variant are automatically enabled.  When neither is defined,
all elements are compiled.

In particular, avoidance of the TLV element will save considerable footprint.

**Functional subsetting:**

| Define                          | Effect |
|---------------------------------|--------|
| `IOTDATA_NO_DECODE`             | Exclude decoder functions (also excludes print and JSON encoder) |
| `IOTDATA_NO_ENCODE`             | Exclude encoder functions (also excludes JSON decoder) |
| `IOTDATA_NO_PRINT`              | Exclude print functions |
| `IOTDATA_NO_DUMP`               | Exclude dump functions |
| `IOTDATA_NO_JSON`               | Exclude JSON functions |
| `IOTDATA_NO_CHECKS_STATE`       | Exclude state checking logic |
| `IOTDATA_NO_CHECKS_TYPES`       | Exclude type checking logic |
| `IOTDATA_NO_ERROR_STRINGS`      | Exclude error strings (and iotdata_strerror) |

These allow building an encoder-only image for a sensor node
(smallest possible footprint) or a decoder-only image for a gateway.

The JSON encoding functions have a dependency on the decoder (decode
from wire format and encode into JSON), and JSON decoding functions
equivalently are dependent on the encoder (decode from JSON and encode
into wire format). The print functions, for brevity, are also dependant
upon the decoder. The dump functions work directly upon the the wire
format buffer are are not dependent on either the encoder or decoder.

Be aware that `IOTDATA_NO_CHECKS_STATE` will cease verification of
non null `iotdata_encoder_t*` and the ordering of the encoding calls
(i.e.  that `begin` must be first, followed by individual `encode_`
functions before a final `end`. This is moderately safe, and
acceptable to turn on during development and off for production. 
It will also turn off null checks for buffers passed into the
`dump`, `print` and `json` function.s

`IOTDATA_NO_CHECKS_TYPES` will cease verification of type boundaries
on calls to `encode_` functions, for example that temperatures passed
are between quantisable minimum and maximum values. This is less safe,
but results only in bad data (and badly quantised data) passed over
the wire: this may fail to interped bad data obtained from sensors.
This option will turn off length checking in TLV encoded strings (and
worst case, truncate them) as we as TLV encoded string validity (and
worst case, transmit these as spaces).

Unless there are considerable space constraints, such as on Class 1
microcontrollers (Appendix E), it is not recommended to engage either
of the `NO_CHECKS` options.

**Floating-point control:**

| Define                          | Effect |
|---------------------------------|--------|
| (default)                       | `double` for position, `float` for other fields |
| `IOTDATA_NO_FLOATING_DOUBLES`   | Use `float` instead of `double` everywhere |
| `IOTDATA_NO_FLOATING`           | Integer-only mode: all values as scaled integers |

In integer-only mode (`IOTDATA_NO_FLOATING`), temperature is passed
as degrees×100 (e.g. 2250 for 22.50°C), wind speed as m/s×100,
radiation dose as µSv/h×100, position as degrees×10^7, and SNR as
dB×10.  This eliminates all floating-point dependencies.

**Test targets**

The `test-versions` target will build each of versions across the
Functional subsetting and Floating-point control, including a
combined `NO_JSON` and `NO_FLOATING` version. This is intended as
a build smoke test to verify compilation control paths. Note that
the combined version, will on x86 platforms, force the compiler
to reject floating-point operations, so as to ensure they are not
latent in the implementation.

### 13.4. Build Size

The following measurements are from GCC on x86-64, aarch64 and 
ESP32-C3 using the `minimal` build target. With space optimisation,
the minimal implementation is less than 1KB on the embedded target.

| Configuration                                       | x86-64 -O6 | x86-64 -Os | aarch64 -O6 | aarch64 -Os | esp32-c3 -O6 | esp32-c3 -Os |
|-----------------------------------------------------|------------|------------|-------------|-------------|--------------|--------------|
| Full library (all elements, encode + decode + JSON) | ~85 KB     | ~29 KB     | ~87 KB      | ~31 KB      | ~67 KB       | ~19 KB       |
| Encoder-only, battery + environment only            | ~5.5 KB    | ~1.1 KB    | ~5.4 KB     | ~1.1 KB     | ~5.0 KB      | ~0.7 KB      |

**x86-64 builds (-O6 and -Os)**

```
--- Full library ---
gcc -Wall -Wextra -Wpedantic -Werror -Wcast-align -Wcast-qual -Wstrict-prototypes -Wold-style-definition -Wcast-align -Wcast-qual -Wconversion -Wfloat-equal -Wformat=2 -Wformat-security -Winit-self -Wjump-misses-init -Wlogical-op -Wmissing-include-dirs -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-overflow=2 -Wswitch-default -Wundef -Wunreachable-code -Wunused -Wwrite-strings  -O6 -DIOTDATA_VARIANT_MAPS_DEFAULT -c iotdata.c -o iotdata_full.o
   text    data     bss     dec     hex filename
  85866    2112    4096   92074   167aa iotdata_full.o
--- Minimal encoder (battery + environment, integer-only) ---
gcc -Wall -Wextra -Wpedantic -Werror -Wcast-align -Wcast-qual -Wstrict-prototypes -Wold-style-definition -Wcast-align -Wcast-qual -Wconversion -Wfloat-equal -Wformat=2 -Wformat-security -Winit-self -Wjump-misses-init -Wlogical-op -Wmissing-include-dirs -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-overflow=2 -Wswitch-default -Wundef -Wunreachable-code -Wunused -Wwrite-strings  -O6 -mno-sse -mno-mmx -mno-80387 \
        -DIOTDATA_NO_DECODE \
        -DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
        -DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
        -DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
        -c iotdata.c -o iotdata_minimal.o
Minimal object size:
   text    data     bss     dec     hex filename
   5559      32       0    5591    15d7 iotdata_minimal.o
0000000000000000 l     O .data.rel.ro.local     0000000000000010 _iotdata_field_ops
0000000000000018 l     O .data.rel.ro.local     0000000000000008 _iotdata_field_def_battery
0000000000000010 l     O .data.rel.ro.local     0000000000000008 _iotdata_field_def_environment
0000000000000000 l    d  .data.rel.ro.local     0000000000000000 .data.rel.ro.local
```

```
--- Full library ---
gcc -Wall -Wextra -Wpedantic -Werror -Wcast-align -Wcast-qual -Wstrict-prototypes -Wold-style-definition -Wcast-align -Wcast-qual -Wconversion -Wfloat-equal -Wformat=2 -Wformat-security -Winit-self -Wjump-misses-init -Wlogical-op -Wmissing-include-dirs -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-overflow=2 -Wswitch-default -Wundef -Wunreachable-code -Wunused -Wwrite-strings  -Os -DIOTDATA_VARIANT_MAPS_DEFAULT -c iotdata.c -o iotdata_full.o
   text    data     bss     dec     hex filename
  29603    2112    4096   35811    8be3 iotdata_full.o
--- Minimal encoder (battery + environment, integer-only) ---
gcc -Wall -Wextra -Wpedantic -Werror -Wcast-align -Wcast-qual -Wstrict-prototypes -Wold-style-definition -Wcast-align -Wcast-qual -Wconversion -Wfloat-equal -Wformat=2 -Wformat-security -Winit-self -Wjump-misses-init -Wlogical-op -Wmissing-include-dirs -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-overflow=2 -Wswitch-default -Wundef -Wunreachable-code -Wunused -Wwrite-strings  -Os -mno-sse -mno-mmx -mno-80387 \
        -DIOTDATA_NO_DECODE \
        -DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
        -DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
        -DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
        -c iotdata.c -o iotdata_minimal.o
Minimal object size:
   text    data     bss     dec     hex filename
   1101      32       0    1133     46d iotdata_minimal.o
0000000000000000 l     O .data.rel.ro.local     0000000000000010 _iotdata_field_ops
0000000000000018 l     O .data.rel.ro.local     0000000000000008 _iotdata_field_def_battery
0000000000000010 l     O .data.rel.ro.local     0000000000000008 _iotdata_field_def_environment
0000000000000000 l    d  .data.rel.ro.local     0000000000000000 .data.rel.ro.local
```

**esp32-c3 builds (-Os)**

```
--- ESP32-C3 full library (no JSON) ---
riscv32-esp-elf-gcc -march=rv32imc -mabi=ilp32 -Os -DIOTDATA_NO_JSON -c iotdata.c -o iotdata_esp32c3_full.o
   text    data     bss     dec     hex filename
  19513       0       0   19513    4c39 iotdata_esp32c3_full.o
--- ESP32-C3 minimal encoder (battery + environment, integer-only) ---
riscv32-esp-elf-gcc -march=rv32imc -mabi=ilp32 -Os \
        -DIOTDATA_NO_DECODE \
        -DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
        -DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
        -DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
        -c iotdata.c -o iotdata_esp32c3_minimal.o
Minimal object size:
   text    data     bss     dec     hex filename
    768       0       0     768     300 iotdata_esp32c3_minimal.o
00000000 l    d  .data  00000000 .data
```


### 13.5. Variant Table Extension

The variant table in the reference implementation is a compile-time
array.  Adding a new variant requires defining a `variant_def_t`
entry with the desired field mapping and compiling with the
`IOTDATA_VARIANT_MAPS` and `IOTDATA_VARIANT_MAPS_COUNT` defines.
No changes to the encoder or decoder logic are needed — the dispatch
mechanism automatically handles any valid variant table entry.

If a new encoding type is needed (not just a relabelling of an
existing type), the implementer must:

  1. Add a new `IOTDATA_FIELD_*` enum value.
  2. Implement the six per-field functions (pack, unpack,
     json_add, json_read, dump, print).
  3. Add a case to each of the six dispatcher functions.
  4. Add the appropriate constants and quantisation helpers.

## 14. Security Considerations

This protocol provides no confidentiality, integrity, or
authentication mechanisms at the packet level.  It is designed for
environments where these properties are provided at other layers:

  - **LoRaWAN** provides AES-128 encryption and message integrity
    checks at the MAC layer.

  - **TLS/DTLS** may be used for IP-based transports.

  - **Physical security** may be sufficient for isolated deployments
    on private land.

Specific risks to consider:

  - **Replay attacks:** An attacker could retransmit captured packets.
    The sequence number provides detection (not prevention) of
    replayed packets, but only if the receiver tracks per-station
    sequence state.

  - **Spoofing:** Station IDs are not authenticated.  An attacker
    within radio range could transmit packets with a forged station
    ID.

  - **Eavesdropping:** The wire format is not encrypted.  Sensor
    readings (temperature, position, etc.) are transmitted in the
    clear.

Deployments with security requirements MUST use an appropriate
underlying transport that provides the needed properties.

## 15. Future Work

The following items are identified for future revisions:

  1. **Sensor metadata TLVs (types 0x10-0x1F):**  Standardised TLV
     formats for conveying sensor type, accuracy class, time source,
     position source, and calibration metadata.  This would enable
     interoperability between devices from different manufacturers
     or deployments without prior out-of-band knowledge.

  2. **Quality indicator fields:**  Per-field quality/confidence
     indicators (e.g. GNSS fix quality, HDOP, number of satellites).
     These would likely use the reserved TLV type range.

  3. **Extended header (variant 15):**  A future header format with
     more variant bits, larger station ID space, or additional
     structural fields.

  4. **Implementation singularity limitation:** The wire format
     supports multiple instances of the same field type in different
     slots (e.g. two independent temperature readings).  The current
     reference implementation uses fixed named storage in the
     encoder/decoder structs, limiting each field type to one
     instance.  A future implementation could decouple field type
     from field storage, allowing the variant map to bind each slot
     to an independent storage location.

---

## Appendix A. 6-Bit Character Table

The packed string format (TLV Format = 1) encodes each character as
6 bits using the following table:

| Value | Char  | Value | Char  | Value | Char  | Value | Char  |
|-------|-------|-------|-------|-------|-------|-------|-------|
|  0    | space |  16   | p     |  32   | 5     |  48   | L     |
|  1    | a     |  17   | q     |  33   | 6     |  49   | M     |
|  2    | b     |  18   | r     |  34   | 7     |  50   | N     |
|  3    | c     |  19   | s     |  35   | 8     |  51   | O     |
|  4    | d     |  20   | t     |  36   | 9     |  52   | P     |
|  5    | e     |  21   | u     |  37   | A     |  53   | Q     |
|  6    | f     |  22   | v     |  38   | B     |  54   | R     |
|  7    | g     |  23   | w     |  39   | C     |  55   | S     |
|  8    | h     |  24   | x     |  40   | D     |  56   | T     |
|  9    | i     |  25   | y     |  41   | E     |  57   | U     |
|  10   | j     |  26   | z     |  42   | F     |  58   | V     |
|  11   | k     |  27   | 0     |  43   | G     |  59   | W     |
|  12   | l     |  28   | 1     |  44   | H     |  60   | X     |
|  13   | m     |  29   | 2     |  45   | I     |  61   | Y     |
|  14   | n     |  30   | 3     |  46   | J     |  62   | Z     |
|  15   | o     |  31   | 4     |  47   | K     |  63   | (rsvd)|

Value 63 is reserved for a future escape mechanism to extend the
character set.

The corresponding encode/decode functions in the reference
implementation:

```c
static inline int char_to_6bit(char c) {
    if (c == ' ')              return 0;
    if (c >= 'a' && c <= 'z')  return 1 + (c - 'a');
    if (c >= '0' && c <= '9')  return 27 + (c - '0');
    if (c >= 'A' && c <= 'Z')  return 37 + (c - 'A');
    return -1;  /* unencodable */
}

static inline char sixbit_to_char(uint8_t val) {
    if (val == 0)              return ' ';
    if (val >= 1  && val <= 26) return 'a' + (val - 1);
    if (val >= 27 && val <= 36) return '0' + (val - 27);
    if (val >= 37 && val <= 62) return 'A' + (val - 37);
    return '?';
}
```

## Appendix B. Quantisation Worked Examples

### B.1. Battery Level

Input: 75%

```
q = round(75 / 100.0 * 31.0) = round(23.25) = 23
Decoded: round(23 / 31.0 * 100.0) = round(74.19) = 74%
Error: 1 percentage point
```

### B.2. Temperature

Input: -15.25°C

```
q = round((-15.25 - (-40.0)) / 0.25) = round(24.75 / 0.25) = round(99.0) = 99
Decoded: -40.0 + 99 * 0.25 = -40.0 + 24.75 = -15.25°C
Error: 0.00°C (exact)
```

### B.3. Position (59.334591°N, 18.063240°E)

Latitude:
```
q = round((59.334591 - (-90.0)) / 180.0 * 16777215)
  = round(149.334591 / 180.0 * 16777215)
  = round(0.829636617 * 16777215)
  = round(13918991.6) = 13918992

Decoded: 13918992 / 16777215.0 * 180.0 + (-90.0)
       = 0.829636653 * 180.0 - 90.0
       = 149.334597 - 90.0 = 59.334597°

Error: 0.000006° ≈ 0.67 m
```

Longitude:
```
q = round((18.063240 - (-180.0)) / 360.0 * 16777215)
  = round(198.063240 / 360.0 * 16777215)
  = round(0.550175667 * 16777215)
  = round(9230415.2) = 9230415

Decoded: 9230415 / 16777215.0 * 360.0 + (-180.0)
       = 0.550175631 * 360.0 - 180.0
       = 198.063227 - 180.0 = 18.063227°

Error: 0.000013° ≈ 0.72 m (at 59°N, cos correction)
```

### B.4. Datetime

Input: Day 5, 12:00:00 (432,000 + 43,200 = 475,200 seconds from year start)

```
ticks = 475200 / 5 = 95040
Decoded: 95040 * 5 = 475200 seconds
Error: 0 seconds (exact, since input is a multiple of 5)
```

Input: Day 5, 12:00:03 (475,203 seconds — not a multiple of 5)

```
ticks = 475203 / 5 = 95040 (integer division, truncated)
Decoded: 95040 * 5 = 475200 seconds
Error: 3 seconds (truncation towards zero)
```

Note: the encoder uses integer division (truncation), not rounding,
for the datetime field.  This means the decoded time is always ≤ the
actual time, with a maximum error of 4 seconds.

## Appendix C. Complete Encoder Example

The following example from the reference implementation test suite
demonstrates encoding a full weather station telemetry packet:

```c
#define IOTDATA_VARIANT_MAPS_DEFAULT
#include "iotdata.h"

/* Encode a full weather station packet (variant 0) */
void encode_full_packet(uint8_t *buf, size_t buf_size, size_t *out_len)
{
    iotdata_encoder_t enc;

    iotdata_encode_begin(&enc, buf, buf_size, 0, 42, 50000);

    /* Pres0 fields — most common, smallest packet when only these */
    iotdata_encode_battery(&enc, 95, true);
    iotdata_encode_link(&enc, -76, 10.0f);
    iotdata_encode_environment(&enc, -2.75f, 1005, 95);
    iotdata_encode_wind(&enc, 12.0f, 270, 18.5f);
    iotdata_encode_rain(&enc, 3, 15); // x10 units
    iotdata_encode_solar(&enc, 450, 7);

    /* Pres1 fields — trigger extension byte */
    iotdata_encode_cloud(&enc, 6);
    iotdata_encode_air_quality(&enc, 75);
    iotdata_encode_radiation(&enc, 100, 0.50f);
    iotdata_encode_position(&enc, 59.334591, 18.063240);
    iotdata_encode_datetime(&enc, 3251120);
    iotdata_encode_flags(&enc, 0x42);

    iotdata_encode_end(&enc, out_len);
    /* Result: 32 bytes for all 12 fields */
}
```

Decoding on the receiver side:

```c
/* Decode and inspect */
iotdata_decoded_t dec;
iotdata_decode(buf, len, &dec);

printf("Station %u: %.2f°C, %u hPa, wind %.1f m/s @ %u°\n",
       dec.station, dec.temperature, dec.pressure,
       dec.wind_speed, dec.wind_direction);

/* Or decode to JSON for forwarding */
char *json;
iotdata_decode_to_json(buf, len, &json);
/* ...forward json to MQTT, database, etc... */
free(json);
```

## Appendix D. Transmission Medium Considerations

### D.1. Design Principle: One Frame, One Transmission

The iotdata protocol is designed so that a useful telemetry packet
typically fits within a single link-layer frame on the target medium.
Fragmenting a packet across multiple frames defeats the core design
goals: each additional frame incurs a separate preamble, MAC header,
and — critically on duty-cycle-regulated media — a separate
transmission window.  On LoRa at SF12, a single 24-byte frame takes
approximately 1.5 seconds to transmit; two frames would take over
3 seconds, consume twice the energy, and halve the effective reporting
rate under duty cycle constraints.

Implementers SHOULD select fields per packet to remain within the
target medium's payload limit.  The presence flag mechanism makes this
straightforward: each packet is self-describing, so the receiver
correctly handles any combination of fields without prior negotiation.

### D.2. LoRa (Raw PHY)

LoRa is the primary target medium.  At 125 kHz bandwidth with coding
rate 4/5 and explicit header, the time-on-air for representative
iotdata packet sizes is:

| Packet            | Bytes | SF7    | SF8    | SF9    | SF10   | SF11   | SF12    |
|-------------------|-------|--------|--------|--------|--------|--------|---------|
| Minimal (battery) | 6     | 36 ms  | 62 ms  | 124 ms | 248 ms | 496 ms | 991 ms  |
| Typical (b+e+d)   | 10    | 41 ms  | 72 ms  | 144 ms | 289 ms | 578 ms | 991 ms  |
| With link+flags   | 12    | 41 ms  | 82 ms  | 144 ms | 289 ms | 578 ms | 1.16 s  |
| Full telemetry    | 24    | 62 ms  | 113 ms | 206 ms | 371 ms | 823 ms | 1.48 s  |

(Computed using the Semtech AN1200.13 formula with 8-symbol preamble
and low data rate optimisation enabled for SF11/SF12.)

All iotdata packets (6-24 bytes) fit well within the raw LoRa PHY
payload limit of 255 bytes at any spreading factor.  The binding
constraint is not payload size but **time-on-air and duty cycle**.

In the EU868 ISM band, the regulatory duty cycle limit is typically
1%.  This means a device must remain silent for 99× the transmission
duration after each packet.  The implications are significant:

| Packet         | Bytes | SF7         | SF12          |
|----------------|-------|-------------|---------------|
| Minimal        | 6     | 1 per 4 s   | 1 per 1.7 min |
| Typical        | 10    | 1 per 4 s   | 1 per 1.7 min |
| Full telemetry | 24    | 1 per 6 s   | 1 per 2.5 min |

The difference between a 10-byte typical packet and a 24-byte full
packet at SF12 is the difference between transmitting every 1.7
minutes and every 2.5 minutes — or equivalently, ~35 vs ~24
transmissions per hour.  This means that for bit-packing: the savings
are not merely aesthetic, they directly translate to reporting
frequency, battery life, or both.

**Spreading factor selection** is an implementation decision that
balances range against airtime.  SF7 provides the shortest airtime
but the least range; SF12 provides maximum range (approximately 10-15
km line of sight) at the cost of 32× the airtime.  The iotdata
protocol is agnostic to spreading factor — the same packet is valid
regardless of the underlying modulation parameters.

### D.3. LoRaWAN

LoRaWAN adds a MAC layer on top of LoRa, providing device
management, adaptive data rate (ADR), and AES-128 encryption with
message integrity.  The MAC overhead consumes approximately 13 bytes
of the LoRa PHY payload (MHDR, DevAddr, FCtrl, FCnt, FPort, MIC),
reducing the available application payload.

The maximum LoRaWAN application payload by data rate (EU868):

| Data Rate | Modulation  | Max payload | Full iotdata (24B) | Headroom |
|-----------|-------------|-------------|--------------------|---------:|
| DR0       | SF12/125    | 51 bytes    | fits               | 27 B     |
| DR1       | SF11/125    | 51 bytes    | fits               | 27 B     |
| DR2       | SF10/125    | 51 bytes    | fits               | 27 B     |
| DR3       | SF9/125     | 115 bytes   | fits               | 91 B     |
| DR4       | SF8/125     | 222 bytes   | fits               | 198 B    |
| DR5       | SF7/125     | 222 bytes   | fits               | 198 B    |

Full iotdata telemetry (24 bytes) fits comfortably at all LoRaWAN
data rates, with at least 27 bytes of headroom even at the lowest
data rate.

Note that the AWS LoRaWAN documentation identifies 11 bytes as the
safe universal application payload across all global frequency plans
and data rates.  The iotdata protocol's typical packet (battery +
environment + depth) is 10 bytes, falling within this universal limit.

LoRaWAN's AES-128 encryption and MIC address the security
considerations discussed in Section 14.  Deployments using LoRaWAN
inherit these protections without any additional work at the iotdata
protocol layer.

### D.4. Sigfox

Sigfox imposes the tightest constraints of any common LPWAN medium:
a maximum uplink payload of **12 bytes** and a limit of **140
messages per day** (approximately one every 10 minutes for uniform
distribution).

| Packet configuration      | Bytes | Fits Sigfox? |
|---------------------------|-------|:------------:|
| Minimal (battery)         | 6     | ✓            |
| Typical (bat+env+depth)   | 10    | ✓            |
| With link+flags           | 12    | ✓ (exact)    |
| With position             | 19    | ✗            |
| Full telemetry            | 24    | ✗            |

The protocol's core telemetry packets fit within the 12-byte Sigfox
limit.  Position and datetime, which require the extension byte and
add 6-9 bytes, do not fit alongside a full complement of sensor
fields.

For Sigfox deployments, implementers SHOULD use a **field rotation
strategy**: transmit core telemetry (battery, environment, depth) on
every message, and rotate in less-frequently-needed fields across
separate messages.  For example:

  - Every 10 minutes: battery + environment + depth (10 bytes)
  - Once per hour: battery + position (12 bytes)
  - Once per day: battery + datetime + flags (10 bytes)

The presence flag mechanism supports this natively — each packet is
self-describing, so the receiver assembles a complete picture from
multiple packets without any out-of-band configuration.

Sigfox provides its own authentication and anti-replay mechanisms at
the network level, but does not encrypt the payload.  Implementers
requiring payload confidentiality on Sigfox must implement
application-layer encryption within the 12-byte constraint.

### D.5. IEEE 802.11ah (Wi-Fi HaLow)

IEEE 802.11ah operates in the sub-GHz ISM bands (typically 868 MHz
in Europe, 902-928 MHz in the US) and targets IoT applications with
range up to 1 km.  Unlike LoRa and Sigfox, it is IP-based and
supports standard Ethernet-class MSDUs (up to 1500 bytes payload per
frame), with A-MPDU aggregation for larger transfers.

Packet size is not a meaningful constraint for iotdata on 802.11ah.
However, the efficiency argument still applies:

  - **Power consumption** scales with transmission duration.  802.11ah
    introduced a reduced MAC header (18 bytes vs 28 bytes in legacy
    802.11) specifically to reduce overhead for small IoT payloads.
    A 10-byte iotdata payload benefits from this optimisation more
    than a 200-byte JSON payload would.

  - **EU duty cycle** regulations apply to the sub-GHz bands used by
    802.11ah, though the specific constraints differ from LoRa
    (802.11ah typically uses listen-before-talk rather than pure duty
    cycle limits).

  - **Contention** in dense deployments is reduced by shorter frame
    durations, improving effective throughput for all stations.

The iotdata payload would typically be carried as a UDP datagram
within the 802.11ah frame.  The receiver-side JSON conversion is
well suited to 802.11ah gateways, which have IP connectivity and
typically run on more capable hardware.

### D.6. Cellular (NB-IoT, LTE-M, SMS)

Cellular technologies provide reliable, wide-area connectivity with
operator-managed infrastructure.  Three cellular transports are
relevant for iotdata:

**NB-IoT and LTE-M** are purpose-built cellular IoT standards.
NB-IoT supports payloads of approximately 1600 bytes per message;
LTE-M supports standard IP MTU sizes.  Payload size is not a
constraint.  Both provide encryption, integrity, and authentication
at the network layer, fully addressing the security considerations
of Section 14.

**SMS** is a widely overlooked but practical transport for
low-rate telemetry.  The GSM 03.40 specification defines an 8-bit
binary encoding mode (selected via the TP-DCS field) that provides
**140 bytes** of raw octet payload per message — more than enough
for any iotdata packet.  Binary SMS is sent via AT commands in PDU
mode, which every GSM/3G/4G modem supports (SIM800, u-blox SARA,
Quectel, etc.).

| Cellular transport | Max payload | Full iotdata | IP stack needed |
|--------------------|-------------|:------------:|:---------------:|
| NB-IoT             | ~1600 B     | ✓            | Yes             |
| LTE-M              | ~MTU        | ✓            | Yes             |
| SMS (8-bit binary) | 140 B       | ✓            | No              |

SMS has several properties that distinguish it from IP-based
cellular transports:

  - **Near-universal coverage.**  SMS operates on the GSM signalling
    channel and works in 2G-only areas where NB-IoT or LTE-M may
    not be deployed.

  - **Store-and-forward.**  The SMSC holds messages if the receiver
    is temporarily unreachable, providing inherent buffering that
    IP-based transports must implement at the application layer.

  - **No IP stack.**  The sensor MCU needs only a UART connection to
    a GSM modem and a handful of AT commands (`AT+CMGS` in PDU
    mode).  This significantly reduces firmware complexity compared
    to a full IP/CoAP/DTLS stack.

  - **No data plan.**  SMS-only SIM plans are available at low cost,
    avoiding the complexity of cellular data provisioning.

  - **Fallback resilience.**  SMS uses the control plane rather than
    the data plane, so it typically remains functional during network
    congestion that would affect data services.

The primary disadvantages of SMS are per-message cost (unlike LoRa
which is free, or bulk-metered data plans), latency (typically 1-30
seconds, occasionally longer during congestion), and receiving-side
complexity (the gateway requires either a GSM modem or an
SMS-to-HTTP gateway service).  SMS provides no payload encryption;
content is visible to the carrier network.

The bit-packing efficiency of iotdata remains beneficial across all
cellular transports for two reasons:

  1. **Energy per byte.**  Cellular radio transmission energy is
     roughly proportional to transmission time.  Shorter payloads
     mean shorter active radio periods and longer battery life.

  2. **Data and message cost.**  For IP-based transports, reducing
     payload from 200 bytes (JSON) to 10 bytes (iotdata) reduces
     per-message data consumption by 95%.  For SMS, keeping packets
     within a single 140-byte message avoids the overhead and cost
     of concatenated multi-part SMS.

SMS is particularly well suited as a **fallback transport**: a sensor
that normally transmits via LoRa could fall back to SMS when
connectivity is lost or when an alarm condition requires guaranteed
delivery via a different path.  The same encoded packet is valid on
both transports without modification.

### D.7. Medium Selection Summary

| Medium   | Max payload | Full iotdata | Primary constraint      | Encryption | Notes                         |
|----------|-------------|:------------:|-------------------------|:----------:|-------------------------------|
| LoRa     | 255 B       | ✓            | Duty cycle / airtime    | No         | Primary target medium         |
| LoRaWAN  | 51-222 B    | ✓            | Duty cycle / airtime    | AES-128    | Managed network               |
| Sigfox   | 12 B        | Partial      | Hard payload limit      | Auth only  | Field rotation needed         |
| 802.11ah | 1500 B      | ✓            | Duty cycle (EU) / power | WPA2/3     | IP-based, UDP transport       |
| NB-IoT   | ~1600 B     | ✓            | Energy / data cost      | Yes        | Operator infrastructure       |
| LTE-M    | ~MTU        | ✓            | Energy / data cost      | Yes        | Operator infrastructure       |
| SMS      | 140 B       | ✓            | Per-message cost        | No         | Fallback / universal coverage |

The protocol's presence flag mechanism makes medium-aware field
selection a runtime decision rather than a compile-time decision.
The same encoder can produce a 10-byte packet for Sigfox and a
24-byte packet for LoRa, with the receiver handling both identically.

---

## Appendix E. System Implementation Considerations

### E.1. Microcontroller Class Taxonomy

The iotdata protocol is designed to run on a wide range of
microcontrollers (MCU), but the appropriate implementation strategy
varies significantly by device class:

| Class | Examples                       | RAM       | Flash      | FPU | Typical role                   |
|-------|--------------------------------|-----------|------------|-----|--------------------------------|
| 1     | PIC16F, ATtiny, MSP430G        | 256B-2KB  | 4-16KB     | No  | Sensor (encode only)           |
| 2     | PIC18F, STM32L0, nRF52         | 2-64KB    | 32-256KB   | No* | Sensor (encode + basic decode) |
| 3     | ESP32-C3, STM32F4, RP2040      | 256-520KB | 384KB-16MB | Yes | Sensor + gateway               |
| 4     | Raspberry Pi, Linux SBC        | 256MB+    | SD/eMMC    | Yes | Gateway / server               |

*nRF52840 has an FPU; most Class 2 devices do not.

The reference implementation currently targets Class 3 and 4 devices.
Class 1 and 2 devices require implementation strategies discussed
below.  The design is specifically intended to be extended to them.

### E.2. Memory Footprint

The reference implementation's data structures have the following
sizes (measured on a 64-bit platform; 32-bit targets will be
smaller due to pointer size):

| Structure          | Size   | Purpose                                     |
|--------------------|--------|---------------------------------------------|
| `iotdata_encoder_t`| 328 B  | Encoder context (all fields + TLV pointers) |
| `iotdata_decoded_t`| 2176 B | Decoded packet (includes TLV data buffers)  |

The encoder context (328 bytes) is dominated by the TLV pointer
array (8 entries × 2 pointers × 8 bytes = 128 bytes on 64-bit).  The
core sensor fields occupy approximately 60 bytes.  On a 32-bit MCU:

  - Full encoder context: ~200 bytes
  - Encoder context without TLV: ~72 bytes
  - Core sensor fields alone: ~50 bytes

The decoded struct (2176 bytes) is dominated by the TLV data buffers
(8 entries × 256 bytes = 2048 bytes).  This structure is designed for
gateway/server use and is NOT appropriate for Class 1 or 2 devices.
A minimal decoder that ignores TLV data needs approximately 60 bytes.

TLV support can be excluded from the encoder, which would yield the
most considerable level of savings if resource constrained.

### E.3. Encoder Architecture: Store-Then-Pack

The current encoder uses a "store then pack" strategy:

```c
iotdata_encode_begin(&enc, buf, sizeof(buf), variant, station, seq);
iotdata_encode_battery(&enc, 84, false);    /* stores values */
iotdata_encode_environment(&enc, 21.5f, 1013, 45);
iotdata_encode_end(&enc, &out_len);         /* packs all at once */
```

**Advantages:**
  - Fields can be added in any order — the variant table determines
    wire order at pack time.
  - Validation happens eagerly, at the `encode_*()` call site.
  - The presence bytes are computed once the full field set is known,
    avoiding backfill.

**Disadvantage:**
  - The full encoder context must be held in RAM simultaneously:
    all field values plus the output buffer.  On Class 3+ devices
    this is negligible; on Class 1 devices (256 bytes RAM) it is
    prohibitive without stripping.

### E.4. Encoder Alternative: Pack-As-You-Go

For severely RAM-constrained devices (Class 1), a pack-as-you-go
encoder could eliminate the context struct entirely.  The encoder
would write bits directly to the output buffer as each field is
supplied.

The challenge is that the presence bytes (at bit offsets 32-39 and
optionally 40-47) must appear in the wire format before the data
fields they describe, but the encoder does not know which fields
will be present until all `encode_*()` calls have been made.

Two approaches can resolve this:

**Approach A: Presence byte backfill.**  Reserve the presence byte
positions in the output buffer (write zeros), then pack each field's
bits immediately.  After the last field, go back and write the
correct presence bytes.  This requires fields to be supplied in
strict field order (S0, S1, S2...) so that the bit cursor advances
correctly.

```
/* Pseudocode for pack-as-you-go with backfill */
write_header(buf, variant, station, seq);   /* bits 0-31 */
pres0_offset = 32;                          /* remember position */
skip_bits(8);                               /* reserve pres0 */
/* caller must add fields in field order */
add_battery(buf, &cursor, level, charging); /* pack immediately */
add_environment(buf, &cursor, t, p, h);
add_depth(buf, &cursor, cm);
/* after all fields: */
backfill_presence(buf, pres0_offset, fields_present);
```

**Approach B: Two-pass encode.**  First pass: call all `encode_*()`
functions which simply set bits in a `fields_present` bitmask
(1 byte of RAM).  Second pass: iterate the variant field table and
pack only the fields that are present.  This requires the field
values to be available again on the second pass, either from
global/static variables or by re-reading the sensors.

**Trade-offs:**

| Property              | Store-then-pack | Pack-as-you-go (A)  | Two-pass (B)    |
|-----------------------|:---------------:|:-------------------:|:---------------:|
| RAM (no TLV)          | ~72 B + buf     | ~4 B + buf          | ~4 B + buf*     |
| Field order           | Any             | Strict field order   | Any             |
| Code complexity       | Low             | Medium              | Medium          |
| Re-read sensors       | No              | No                  | Yes             |
| Suitable for Class 1  | Marginal        | Yes                 | Yes             |

*Two-pass requires field values to be available on the second pass,
either stored elsewhere or re-read from hardware.

The reference implementation uses store-then-pack because it is the
most developer-friendly and the target devices (ESP32-C3, Class 3)
have ample RAM.  Implementers targeting Class 1 devices SHOULD
consider pack-as-you-go with backfill.  Approach A maybe provided
in a future version of the reference implementation.

### E.5. Compile-Time Field Stripping (#ifdef)

The reference implementation factors all per-field operations into
static inline functions specifically to enable compile-time stripping:

```c
#ifdef IOTDATA_ENABLE_SOLAR
static inline void pack_solar(uint8_t *buf, size_t *bp,
                              const iotdata_encoder_t *enc) {
    bits_write(buf, bp, enc->solar_irradiance, 10);
    bits_write(buf, bp, enc->solar_ultraviolet, 4);
}
static inline void unpack_solar(...) { ... }
/* ...4 more functions... */
#endif
```

Each field type has 6 functions (pack, unpack, json_add, json_read,
dump, print).  On an embedded sensor that only transmits battery,
environment, and depth:

| Component                     | Functions | Approx. code size  |
|-------------------------------|:---------:|-------------------:|
| Core (header, presence, bits) | —         | ~1 KB              |
| Battery (6 functions)         | 6         | ~400 B             |
| Environment (6 functions)     | 6         | ~500 B             |
| Depth (6 functions)           | 6         | ~350 B             |
| **Included total**            | 18        | **~2.2 KB**        |
| Solar (excluded)              | 6         | -400 B             |
| Link quality (excluded)       | 6         | -350 B             |
| Flags (excluded)              | 6         | -300 B             |
| Position (excluded)           | 6         | -500 B             |
| Datetime (excluded)           | 6         | -400 B             |
| JSON functions (excluded)     | ~20       | -2 KB              |
| Print/dump (excluded)         | ~20       | -1.5 KB            |

A fully stripped sensor-only build (encode path only, 3 field types,
no JSON/print/dump) can fit in approximately 2-3 KB of flash.  This
is achievable on Class 1 devices.

### E.6. Floating Point Considerations

Several encode functions accept floating-point parameters:

  - `iotdata_encode_environment()` takes `float` temperature
  - `iotdata_encode_position()` takes `double` latitude/longitude
  - `iotdata_encode_link()` takes `float` SNR

On MCUs without a hardware FPU (most Class 1 and many Class 2
devices), floating-point arithmetic is emulated in software, which
is both slow (~50-100 cycles per operation) and adds code size
(~2-5 KB for soft-float library).

For Class 1 targets, implementers SHOULD consider:

  - **Integer-only temperature API:**  Accept temperature as a
    fixed-point integer in units of 0.25°C offset from -40°C.
    The caller performs `q = (temp_raw - (-40*4))` and passes `q`
    directly.  No floating point needed.

  - **Integer-only position API:**  Accept pre-quantised 24-bit
    latitude and longitude values.  The caller uses the GNSS
    receiver's native integer output and scales appropriately.

  - **Integer-only SNR:**  Accept SNR as an integer in dB.

The reference implementation uses float/double for developer
convenience on Class 3+ targets.  However, the compile-time
configurations of `IOTDATA_NO_FLOATING_DOUBLES` or 
`IOTDATA_NO_FLOATING` can be used to remove floating point
operations and provide replacement integer-only entry points
such as temperature encoders that take values multiplied by
100, e.g. 152 to represent a temperature of 15.2°C.

### E.7. Dependencies and Portability

The reference implementation has the following dependency profile:

| Component       | Dependencies                                          | Required for            |
|-----------------|-------------------------------------------------------|-------------------------|
| Encoder         | `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<math.h>` | All builds              |
| Decoder         | Same as encoder                                       | Gateway / bidirectional |
| JSON conversion | `libcjson`                                            | Gateway / server        |
| Print / dump    | `<stdio.h>`                                           | Debug / gateway         |

The core encoder has no external library dependencies.  The `<math.h>`
dependency is for `round()` and `floor()` in the quantisation
functions; on platforms where `<math.h>` is unavailable or
expensive, these can be replaced with integer arithmetic equivalents:

```c
/* Integer-only round for non-negative values */
static inline uint16_t int_round(uint32_t num, uint32_t denom) {
    return (uint16_t)((num + denom / 2) / denom);
}
```

The `libcjson` dependency exists only for the JSON serialisation
functions and SHOULD be excluded from embedded builds via `#ifdef
IOTDATA_NO_JSON`.

### E.8. Stack vs Heap Allocation

The encoder and decoder are designed for **stack allocation only**.
No `malloc()` or `free()` calls are made in any encode or decode
path.  This is critical for:

  - **Bare-metal systems** without a heap allocator.
  - **RTOS environments** (FreeRTOS, Zephyr) where heap
    fragmentation must be avoided and stack sizes are fixed.
  - **Safety-critical systems** where dynamic allocation is
    prohibited by coding standards (MISRA C, etc.).

The JSON conversion functions (`iotdata_decode_to_json`) do allocate
heap memory via `cJSON_CreateObject()` and return a `malloc`'d
string.  These functions are gateway/server-only and are not intended
for embedded use.

### E.9. Endianness

The bit-packing functions operate on individual bytes via bit
manipulation and are **endian-agnostic**.  The `bits_write()` and
`bits_read()` functions address the output buffer byte-by-byte,
computing bit offsets explicitly.  No multi-byte loads or stores are
used in the packing/unpacking path.

This means the same code runs correctly on:
  - Little-endian ARM (ESP32, STM32, RP2040)
  - Big-endian PIC (in standard configuration)
  - Any other byte-addressable architecture

No byte-swap operations are needed when moving between platforms.

### E.10. Real-Time Considerations

The encoder's `encode_end()` function performs a single linear pass
over the variant field table and packs all present fields.  The
execution time is bounded and predictable:

  - **Minimum** (battery only): ~50 bit operations
  - **Maximum** (all fields + TLV): ~300 bit operations

Each bit operation is a constant-time shift-and-mask.  There are no
loops with data-dependent iteration counts (except TLV string
encoding, which iterates over the string length).  The encoder is
suitable for use in interrupt service routines or time-critical
sections, though calling it from a main-loop context is more typical.

### E.11. Platform-Specific Notes

**Raspberry Pi / Linux (Class 4):**  The full implementation runs
unmodified, including JSON conversion, print, dump, and all 8 field
types.  Typically used as a gateway, receiving packets via LoRa HAT
or USB-connected radio module, decoding to JSON, and forwarding via
MQTT, HTTP, or database insertion.

**ESP32-C3 (Class 3, primary target):**  The reference implementation
runs unmodified.  The ESP32-C3 has 400 KB SRAM and 4 MB flash but no
hardware FPU — both single- and double-precision arithmetic is
software-emulated.  Use `IOTDATA_NO_FLOATING` for best performance.
Both the encoder and decoder, including JSON functions, fit
comfortably.  The ESP-IDF build system supports `#ifdef` stripping
via `menuconfig` or `sdkconfig` defines.

**STM32L0 (Class 2):**  With 20 KB RAM and 64-192 KB flash, both the
encoder context (328 bytes) and decoded struct (2176 bytes) fit on the
stack comfortably.  Exclude JSON, print, and dump functions.  Use
`-ffunction-sections -fdata-sections -Wl,--gc-sections` to strip
unused code.

**PIC18F (Class 2):**  Similar constraints to STM32L0 but with
typically less RAM (2-4 KB) and a more limited C compiler.  The
reference implementation's use of `static inline` functions may need
to be adjusted (some PIC compilers do not inline effectively).
Consider the pack-as-you-go approach (Section E.4) for minimal RAM
usage.

**PIC16F (Class 1):**  With as little as 256 bytes of RAM, the
full encoder context does not fit.  Use pack-as-you-go with backfill
(Section E.4, Approach A), integer-only APIs (Section E.6), and
aggressive `#ifdef` stripping (Section E.5).  Target flash budget:
2-3 KB for a battery + environment + depth encoder.

### E.12. Class 1 Hand-Rolled Encoder Example

On devices with as little as 256 bytes of RAM (PIC16F, ATtiny), the
library's table-driven architecture is unnecessary overhead.  The
following self-contained function encodes a weather station packet
with battery, environment, and two TLV entries directly into a
caller-provided buffer.  No structs, no function pointers, no
library linkage — just bit-packing arithmetic.  The full weather
station packet requires a buffer of no more than 32 bytes (without
TLV).  If really necessary, the implementation can avoid buffers
and use ws_bits to write directly to an output (e.g. serial port).

```c
#define WS_VARIANT       0    /* Built-in weather station */
#define WS_STATION       1    /* 0 to 4095 */
#define WS_PRES0_FIELDS  6    /* battery, link, environment, wind, rain, solar */

#define WS_PRES_EXT      0x80
#define WS_PRES_TLV      0x40

static void ws_bits(uint8_t *buf, uint16_t *bp, uint32_t val, uint8_t n) {
    for (int8_t i = n - 1; i >= 0; i--, (*bp)++)
        if (val & (1UL << i))
            buf[*bp >> 3] |= (1U << (7 - (*bp & 7)));
}

/*
 * Encodes: battery, environment, one raw TLV, one string TLV.
 * All integer arithmetic.  No floating point.  No malloc.
 *
 * Parameters:
 *   buf        — output buffer, must be zeroed by caller, >= 32 bytes
 *   sequence   — 16-bit sequence number
 *   batt_pct   — battery level 0-100
 *   charging   — 0 or 1
 *   temp100    — temperature in centidegrees (-4000 = -40.00 C)
 *   press_hpa  — pressure in hPa (850-1105)
 *   humid_pct  — humidity 0-100
 *   tlv0_type  — first TLV type (0-63)
 *   tlv0_data  — first TLV raw data pointer
 *   tlv0_len   — first TLV data length
 *   tlv1_type  — second TLV type (0-63)
 *   tlv1_str   — second TLV string (6-bit charset: a-z 0-9 A-Z space)
 *   tlv1_len   — second TLV string length
 *
 * Returns: packet size in bytes
 */
static uint8_t ws_encode(
    uint8_t *buf, uint16_t sequence,
    uint8_t batt_pct, uint8_t charging,
    int16_t temp100, uint16_t press_hpa, uint8_t humid_pct,
    uint8_t tlv0_type, const uint8_t *tlv0_data, uint8_t tlv0_len,
    uint8_t tlv1_type, const char *tlv1_str, uint8_t tlv1_len)
{
    uint16_t bp = 0;

    /* --- Header: 4-bit variant + 12-bit station + 16-bit sequence --- */
    ws_bits(buf, &bp, WS_VARIANT, 4);
    ws_bits(buf, &bp, WS_STATION, 12);
    ws_bits(buf, &bp, sequence, 16);

    /* --- Presence byte 0: ext=0, tlv=1, battery=1, link=0,
           environment=1, wind=0, rain=0, solar=0 --- */
    /*   bit 7: ext         = 0  (no pres1)
     *   bit 6: tlv         = 1  (TLV present)
     *   bit 5: battery     = 1
     *   bit 4: link        = 0
     *   bit 3: environment = 1
     *   bit 2: wind        = 0
     *   bit 1: rain        = 0
     *   bit 0: solar       = 0
     */
    ws_bits(buf, &bp, 0x68, 8);   /* 0b01101000 */

    /* --- Battery: 5-bit level + 1-bit charging --- */
    ws_bits(buf, &bp, ((uint32_t)batt_pct * 31 + 50) / 100, 5);
    ws_bits(buf, &bp, charging ? 1 : 0, 1);

    /* --- Environment: 9-bit temp + 8-bit pressure + 7-bit humidity --- */
    ws_bits(buf, &bp, (uint32_t)((temp100 - (-4000)) + 12) / 25, 9);
    ws_bits(buf, &bp, (uint32_t)(press_hpa - 850), 8);
    ws_bits(buf, &bp, (uint32_t)humid_pct, 7);

    /* --- TLV 0: raw --- */
    ws_bits(buf, &bp, 0, 1);           /* format: raw */
    ws_bits(buf, &bp, tlv0_type, 6);   /* type */
    ws_bits(buf, &bp, 1, 1);           /* more: yes */
    ws_bits(buf, &bp, tlv0_len, 8);    /* length */
    for (uint8_t i = 0; i < tlv0_len; i++)
        ws_bits(buf, &bp, tlv0_data[i], 8);

    /* --- TLV 1: 6-bit string --- */
    ws_bits(buf, &bp, 1, 1);           /* format: string */
    ws_bits(buf, &bp, tlv1_type, 6);   /* type */
    ws_bits(buf, &bp, 0, 1);           /* more: no */
    ws_bits(buf, &bp, tlv1_len, 8);    /* length */
    for (uint8_t i = 0; i < tlv1_len; i++) {
        char c = tlv1_str[i];
        uint8_t v = (c == ' ') ? 0 :
                    (c >= 'a' && c <= 'z') ? 1 + (c - 'a') :
                    (c >= '0' && c <= '9') ? 27 + (c - '0') :
                    (c >= 'A' && c <= 'Z') ? 37 + (c - 'A') : 0;
        ws_bits(buf, &bp, v, 6);
    }

    return (uint8_t)((bp + 7) >> 3);
}
```

**Resource usage:** This function requires approximately 20 bytes of
stack (loop counters, bit pointer, temporaries) plus the output
buffer (or not, if directly writing to serial output).  The code
compiles to under 400 bytes on PIC18F or AVR.  The caller can reuse
the output buffer between transmissions.

**Adapting for other variants:** Copy the function, change the
presence byte constant, and add or remove the field sections.  Each
field is a self-contained block of `ws_bits` calls — the protocol
document (Sections 4-6) gives the bit widths and quantisation
formulae for every field type.

## Appendix F. Example Weather Station Output

```
╔══════════════════════════════════════════════════╗
║  iotdata weather station simulator               ║
║  Station 42 — variant 0 (weather_station)        ║
║  30s reports / 5min full reports with position   ║
║  Press Ctrl-C to stop                            ║
╚══════════════════════════════════════════════════╝

────────────────────────────────────────────────────────────────────────────────
** Packet #1  [17:29:08]  *** 5-minute report (with position/datetime) ***
────────────────────────────────────────────────────────────────────────────────

** Sensor values:

    battery:      85.2%  
    link:          -85 dBm   SNR 4.8 dB
    temperature: +14.75 °C
    pressure:     1013 hPa
    humidity:       55 %
    wind:          4.1 m/s @ 172°  (gust 8.7 m/s)
    rain:            3 mm/hr, 0.5 mm/d
    solar:         393 W/m²  UV 3
    clouds:          4 okta
    air quality:    41 AQI
    radation:       22 CPM,     0.10 µSv/h
    position:    59.334588, 18.063240
    datetime:    3518948 s from year start
    flags:       0x01

** Binary (32 bytes):

    00 2A 00 01 BF 7E D2 26 DD 1B 71 0F 44 40 C5 89 
    34 14 80 2C 00 56 A3 18 84 66 C2 78 55 E9 68 08 

** Diagnostic dump:

      Offset     Len  Field                            Raw  Decoded                       Range
      ------     ---  -----                            ---  -------                       -----
           0       4  variant                            0  0                             0-14 (15=rsvd)
           4      12  station                           42  42                            0-4095
          16      16  sequence                           1  1                             0-65535
          32       8  presence[0]                      191  0xbf                          ext|tlv|6 fields
          40       8  presence[1]                      126  0x7e                          ext|7 fields
          48       5  battery_level                     26  84%                           0..100%%, 5b quant
          53       1  battery_charging                   0  discharging                   0/1
          54       4  link_rssi                          8  -88 dBm                       -120..-60, 4dBm
          58       2  link_snr                           2  0 dB                          -20..+10, 10dB
          60       9  temperature                      219  14.75 C                       -40..+80C, 0.25C
          69       8  pressure                         163  1013 hPa                      850..1105 hPa
          77       7  humidity                          55  55%                           0..100%%
          84       7  wind_speed                         8  4.0 m/s                       0..63.5, 0.5m/s
          91       8  wind_direction                   122  172 deg                       0..355, ~1.4deg
          99       7  wind_gust                         17  8.5 m/s                       0..63.5, 0.5m/s
         106       8  rain_rate                          3  3 mm/hr                       0..255 mm/hr
         114       4  rain_size                          1  0.4 mm/d                      0..6.3 mm/d
         118      10  solar_irradiance                 393  393 W/m2                      0..1023 W/m2
         128       4  solar_ultraviolet                  3  3                             0..15
         132       4  clouds                             4  4 okta                        0..8 okta
         136       9  air_quality                       41  41 AQI                        0..500 AQI
         145      14  radiation_cpm                     22  22 CPM                        0..65535 CPM
         159      14  radiation_dose                    10  0.10 uSv/h                    0..163.83, 0.01
         173      24  latitude                    13918992  59.334592                     -90..+90
         197      24  longitude                    9230415  18.063230                     -180..+180
         221      24  datetime                      703789  day 40 17:29:05 (3518945s)    5s res
         245       8  flags                              1  0x01                          8-bit bitmask

Total: 253 bits (32 bytes)

** Decoded:

Station 42 seq=1 var=0 (weather_station) [253 bits, 32 bytes]
  battery:             84% (discharging)
  link:                -88 dBm RSSI, 0 dB SNR
  environment:         14.75 C, 1013 hPa, 55%
  wind:                4.0 m/s, 172 deg, gust 8.5 m/s
  rain:                3 mm/hr, 0.4 mm/d
  solar:               393 W/m2, UV 3
  clouds:              4 okta
  air_quality:         41 AQI
  radiation:           22 CPM, 0.10 uSv/h
  position:            59.334592, 18.063230
  datetime:            day 40 17:29:05 (3518945s)
  flags:               0x01

** JSON:

{"variant":0,"station":42,"sequence":1,"packed_bits":253,"packed_bytes":32,"battery":{"level":84,"charging":false},"link":{"rssi":-88,"snr":0},"environment":{"temperature":14.75,"pressure":1013,"humidity":55},"wind":{"speed":4,"direction":172,"gust":8.5},"rain":{"rate":3,"size":4},"solar":{"irradiance":393,"ultraviolet":3},"clouds":4,"air_quality":41,"radiation":{"cpm":22,"dose":0.099999994039535522},"position":{"latitude":59.334592183506032,"longitude":18.06323039908591},"datetime":3518945,"flags":1}

────────────────────────────────────────────────────────────────────────────────
** Packet #2  [17:29:38]  30-second report
────────────────────────────────────────────────────────────────────────────────

** Sensor values:

    battery:      84.9%  
    link:          -85 dBm   SNR 5.5 dB
    temperature: +14.48 °C
    pressure:     1013 hPa
    humidity:       55 %
    wind:          3.6 m/s @ 171°  (gust 7.2 m/s)
    rain:            5 mm/hr, 0.0 mm/d
    solar:         390 W/m²  UV 3

** Binary (16 bytes):

    00 2A 00 02 3F D2 36 D5 1B 70 EF 43 81 41 86 30 

** Diagnostic dump:

      Offset     Len  Field                            Raw  Decoded                       Range
      ------     ---  -----                            ---  -------                       -----
           0       4  variant                            0  0                             0-14 (15=rsvd)
           4      12  station                           42  42                            0-4095
          16      16  sequence                           2  2                             0-65535
          32       8  presence[0]                       63  0x3f                          ext|tlv|6 fields
          40       5  battery_level                     26  84%                           0..100%%, 5b quant
          45       1  battery_charging                   0  discharging                   0/1
          46       4  link_rssi                          8  -88 dBm                       -120..-60, 4dBm
          50       2  link_snr                           3  10 dB                         -20..+10, 10dB
          52       9  temperature                      218  14.50 C                       -40..+80C, 0.25C
          61       8  pressure                         163  1013 hPa                      850..1105 hPa
          69       7  humidity                          55  55%                           0..100%%
          76       7  wind_speed                         7  3.5 m/s                       0..63.5, 0.5m/s
          83       8  wind_direction                   122  172 deg                       0..355, ~1.4deg
          91       7  wind_gust                         14  7.0 m/s                       0..63.5, 0.5m/s
          98       8  rain_rate                          5  5 mm/hr                       0..255 mm/hr
         106       4  rain_size                          0  0.0 mm/d                      0..6.3 mm/d
         110      10  solar_irradiance                 390  390 W/m2                      0..1023 W/m2
         120       4  solar_ultraviolet                  3  3                             0..15

Total: 124 bits (16 bytes)

** Decoded:

Station 42 seq=2 var=0 (weather_station) [124 bits, 16 bytes]
  battery:             84% (discharging)
  link:                -88 dBm RSSI, 10 dB SNR
  environment:         14.50 C, 1013 hPa, 55%
  wind:                3.5 m/s, 172 deg, gust 7.0 m/s
  rain:                5 mm/hr, 0.0 mm/d
  solar:               390 W/m2, UV 3

** JSON:
{"variant":0,"station":42,"sequence":2,"packed_bits":124,"packed_bytes":16,"battery":{"level":84,"charging":false},"link":{"rssi":-88,"snr":10},"environment":{"temperature":14.5,"pressure":1013,"humidity":55},"wind":{"speed":3.5,"direction":172,"gust":7},"rain":{"rate":5,"size":0},"solar":{"irradiance":390,"ultraviolet":3}}
```

---

*This document and the reference implementation are maintained at [https://github.com/matthewgream/libiotdata].*
