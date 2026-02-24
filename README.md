# IoT Sensor Telemetry Protocol (iotdata)

## Specification

```text
    Title:     IoT Sensor Telemetry Protocol
    Version:   1
    Status:    Running Code
    Created:   2026-02-07
    Authors:   Matthew Gream
    Licence:   Attribute-ShareAlike 4.0 International
               https://creativecommons.org/licenses/by-sa/4.0
    Location:  https://libiotdata.org
               https://github.com/matthewgream/libiotdata

```

## Status of This Document

This document specifies a bit-packed telemetry protocol for battery- and
transmission- constrained IoT sensor systems, with particular emphasis on
LoRa-based remote environmental monitoring, seamlessly operable in
point-to-point and mesh-relay deployments. The protocol has a reference
implementation in C (`libiotdata`) which is the normative source for any
ambiguity in this specification. In the tradition of RFC 1, the specification is
informed by running code.

Discussion of this document and the reference implementation takes place on the
project's GitHub repository.

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
  - [8.18. Rain Size (standalone)](#818-rain-size-standalone)
  - [8.19. Air Quality (bundle)](#819-air-quality-bundle)
  - [8.20. Air Quality Index (standalone)](#820-air-quality-index-standalone)
  - [8.21. Air Quality PM (standalone)](#821-air-quality-pm-standalone)
  - [8.22. Air Quality Gas (standalone)](#822-air-quality-gas-standalone)
  - [8.23. Radiation (bundle)](#823-radiation-bundle)
  - [8.24. Radiation CPM (standalone)](#824-radiation-cpm-standalone)
  - [8.25. Radiation Dose (standalone)](#825-radiation-dose-standalone)
  - [8.26. Clouds](#826-clouds)
  - [8.27. Image](#827-image)
- [9. TLV Data](#9-tlv-data)
  - [9.1. TLV Header](#91-tlv-header)
  - [9.2. Raw Format](#92-raw-format)
  - [9.3. Packed String Format](#93-packed-string-format)
  - [9.4. TLV Types](#94-tlv-types)
  - [9.5. Global TLV Types](#95-global-tlv-types)
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
  - [13.4. Build Size and Stack Usage](#134-build-size-and-stack-usage)
  - [13.5. Variant Table Extension](#135-variant-table-extension)
- [14. Security Considerations](#14-security-considerations)
- [15. Future Work](#15-future-work)
- [Appendix A. 6-Bit Character Table](#appendix-a-6-bit-character-table)
- [Appendix B. Quantisation Worked Examples](#appendix-b-quantisation-worked-examples)
- [Appendix C. Complete Encoder Example](#appendix-c-complete-encoder-example)
- [Appendix D. Transmission Medium Considerations](#appendix-d-transmission-medium-considerations)
- [Appendix E. System Implementation Considerations](#appendix-e-system-implementation-considerations)
- [Appendix F. Example Weather Station Output](#appendix-f-example-weather-station-output)
- [Appendix G. Mesh Relay Protocol](#appendix-g-mesh-relay-protocol)

---

## 1. Introduction

Remote environmental monitoring systems — weather stations, snow depth sensors,
ice thickness gauges — are frequently deployed in locations without mains power
or wired connectivity. These devices are constrained along three axes
simultaneously:

1. **Power** — battery and/or small solar panel, particularly in locations with
   limited winter daylight.

2. **Transmission** — LoRa, 802.11ah, SigFox, cellular SMS, low-frequency RF, or
   similar low-power point-to-point, wide-area, or mesh networks with effective
   payload limits of tens of bytes per transmission. Regulatory limits on
   transmission time (typically 1% duty cycle in EU ISM bands) mean that every
   byte transmitted has a direct cost in time-on-air and energy.

3. **Processing** — small, inexpensive embedded microcontrollers running at tens
   of megahertz with tens or hundreds of kilobytes of RAM and program storage,
   where code size and complexity are real constraints, and where there are no,
   or limited, operating system or protocol support.

Existing serialisation approaches — JSON, Protobuf, CBOR, even raw C structs —
waste bits on byte alignment, field delimiters, schema metadata, or fixed-width
fields for data that could be represented in far fewer bits.

The IoT Sensor Telemetry Protocol (iotdata) addresses this by defining a
bit-packed wire format where each field is quantised to the minimum number of
bits required for its operational range and resolution. A typical weather
station packet — battery, link quality, temperature, pressure, humidity, wind
speed, direction and gust, rain rate and drop size, solar irradiance and UV
index, plus 8 flag bits — fits in 16 bytes. A full-featured packet adding air
quality, cloud cover, radiation CPM and dose, position latitude and longitude,
and timestamp fits in 32 bytes.

The protocol is designed for transmit-only devices. There is no negotiation,
handshake, or acknowledgement at this layer. A sensor wakes, encodes its
readings, transmits, and sleeps. Transmissions are typically infrequent (minutes
to hours), bursty, and rely on lower-layer integrity (checksums or CRC) without
lower-layer reliability (retransmission or acknowledgement).

## 2. Conventions and Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD",
"SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be
interpreted as described in RFC 2119.

**Bit numbering:** All bit diagrams in this document use MSB-first (big-endian)
bit order. Bit 7 of a byte is the most significant bit and is transmitted first.
Multi-bit fields are packed MSB-first: the most significant bit of a field
occupies the earliest bit position in the stream.

**Bit offset:** Bit positions within a packet are numbered from 0, starting at
the MSB of the first byte. Bit 0 is the MSB of byte 0; bit 7 is the LSB of byte
0; bit 8 is the MSB of byte 1; and so on.

**Byte boundaries:** Fields are NOT byte-aligned unless they happen to fall on a
byte boundary. The packet is a continuous bit stream; byte boundaries have no
structural significance. The final byte is zero-padded in its least-significant
bits if the total bit count is not a multiple of 8.

**Quantisation:** The process of mapping a continuous or large-range value to a
reduced set of discrete steps that fit in fewer bits. All quantisation in this
protocol uses `round()` (round half away from zero) and can be carried out as
floating-point or integer-only.

## 3. Design Principles

The following principles guided the protocol design:

1. **Bit efficiency over simplicity.** Every field is quantised to the minimum
   bits that preserve operationally useful resolution. There is no
   byte-alignment padding between fields.

2. **Presence flags over fixed structure.** Optional fields are indicated by
   presence bits, so a battery-only packet is 6 bytes and a full-telemetry
   packet is 24 bytes — the same protocol serves both.

3. **Variants over negotiation.** Different sensor types (snow, ice, weather)
   can prioritise different fields in the compact first presence byte. The 4-bit
   variant field in the header selects the field mapping. No runtime negotiation
   is needed.

4. **Source-agnostic fields.** Position may come from GNSS, WiFi geolocation,
   cell tower triangulation, or static configuration. Datetime may come from
   GNSS, NTP, or a local RTC. The wire encoding is the same regardless of
   source.

5. **Extensibility via TLV.** Diagnostic data, firmware metadata, and
   user-defined payloads use a trailing TLV (type-length-value) section that
   does not affect the fixed field layout. These are typically designed to be
   system data, rather than sensor data.

6. **Encode-only on the sensor.** The encoder is small enough for
   resource-constrained MCUs. JSON serialisation and other server-side features
   are optional and can be excluded from embedded builds. The reference
   implementation can build to 1 KB and non-refernece implementations to less
   than 512 bytes.

7. **Transport-delegated integrity.** The protocol carries no checksum, CRC,
   length field, or encryption. These functions are delegated to the underlying
   medium (LoRa CRC, LoRaWAN MIC, cellular security, etc.). A redundant CRC
   would cost 16-32 bits — significant when the entire payload may be 46 bits.
   Packet loss is tolerated: the sequence number (Section 5) enables detection
   without requiring retransmission.

8. **No global interoperability.** It is expressly not a goal to support
   interoperability between implementations, e.g. between vendors. Rather, the
   design intends to provide an optimal framework and reference for a specific
   vendor across a suite of devices. Interoperability may be a goal for future
   versions.

## 4. Packet Structure Overview

An iotdata packet consists of the following sections, in order:

```text
+--------+------------+-------------+------------+
| Header | Presence   | Data Fields | TLV Fields |
| 32 bits| 8 to 32 b. | variable    | optional   |
+--------+------------+-------------+------------+
```

All sections are packed as a continuous bit stream with no alignment gaps
between them.

- **Header** (32 bits): Always present. Identifies the variant, station, and
  sequence number.

- **Presence** (8 to 32 bits): Always present. One to four presence bytes
  chained via extension bits indicate which data fields follow. data fields and
  TLV data follow.

- **Data fields** (variable): Zero or more sensor data fields, packed in the
  order defined by the variant's field table.

- **TLV fields** (variable, optional): Zero or more type-length-value data
  entries.

The minimum valid packet is 5 bytes (header + one presence byte with no fields
set), though such a packet carries no sensor data and serves only as a
heartbeat. In practice the minimum useful packet is 6 bytes (header + presence +
battery = 46 bits).

## 5. Header

The header is always the first 32 bits of a packet.

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Var  |      Station ID       |           Sequence            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Variant** (4 bits, offset 0): Index into the variant field table (Section 7).
Values 0-14 are usable. Value 15 is RESERVED for a future extended header format
and MUST NOT be used. An encoder MUST reject variant 15. A decoder encountering
variant 15 SHOULD reject the packet.

**Station ID** (12 bits, offset 4): Identifies the transmitting station. Range
0-4095. Station IDs are assigned by the deployment operator; this protocol does
not define an allocation mechanism.

**Sequence** (16 bits, offset 16): Monotonically increasing packet counter,
wrapping from 65535 to 0. The receiver MAY use this to detect lost packets. The
wrap-around is expected and MUST NOT be treated as an error.

The header could be reduced to 24 bits by a reduction in the Station ID (from 12
to 8 bits, saving 4 bits) and the Sequence (from 16 to 12 bits, saving 4 bits).
This would retain station diversity (at 256, rather than 4096) and loss
detection (at 4096 packet window, rather than 65536). Such a modification is not
contemplated in this version of the protocol.

## 6. Presence Bytes

Immediately following the header, one or more presence bytes indicate which data
fields are included in the packet. Presence bytes form an extension chain: each
byte has an extension bit that, when set, indicates another presence byte
follows.

### Presence Byte 0 (always present)

```text
 7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+
|Ext|TLV| S5| S4| S3| S2| S1| S0|
+---+---+---+---+---+---+---+---+
```

- **Ext** (bit 7): Extension flag. If set, Presence Byte 1 follows immediately.
  If clear, no further presence bytes exist.

- **TLV** (bit 6): TLV data flag. If set, one or more TLV entries (Section 9)
  follow after all data fields. Builds excluding TLV might use this as a Data
  field, but this is not contemplated in this version of the protocol.

- **S0-S5** (bits 5-0): Data fields 0 through 5. Each bit, when set, indicates
  that the corresponding field (as defined by the variant's field table) is
  present in the packet. The field data appears in field order: S0 first, then
  S1, S2, and so on.

### Presence Byte N (N ≥ 1, conditional)

Present only when the Ext bit in the preceding presence byte is set.

```text
 7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+
|Ext| S6| S5| S4| S3| S2| S1| S0|
+---+---+---+---+---+---+---+---+
```

- **Ext** (bit 7): Extension flag. If set, another presence byte follows. This
  allows chaining of an arbitrary number of presence bytes.

- **S0-S6** (bits 6-0): Data fields for this presence byte. The first extension
  byte (pres1) carries fields 6-12, the second (pres2) carries fields 13-19, and
  so on.

### Field Capacity

The maximum number of data fields available depends on the number of presence
bytes:

| Presence Bytes  | Total Data Fields | Formula       |
| --------------- | ----------------- | ------------- |
| 1 (pres0)       | 6                 | 6             |
| 2 (pres0+1)     | 13                | 6 + 7         |
| 3 (pres0+1+2)   | 20                | 6 + 7 + 7     |
| 4 (pres0+1+2+3) | 27                | 6 + 7 + 7 + 7 |

The reference implementation supports up to 4 presence bytes (27 data fields).
In practice, the default weather station variant uses 2 presence bytes for 12
data fields. It is unlikely an implementation would pratically require more than
2-3 presence bytes.

### Extension Byte Optimisation

The encoder only emits the minimum number of presence bytes needed for the
fields actually present. If all set fields fit in pres0 (fields 0-5), no
extension bytes are emitted, even if the variant defines fields in pres1. This
optimisation reduces packet size for common transmissions that include only the
most frequently updated fields.

### Field Ordering

Data fields are packed in strict field order. First, all set fields from
Presence Byte 0 are packed in order S0, S1, ..., S5. Then, if Presence Byte 1 is
present, fields S6 through S12 are packed. The TLV section (if present) always
comes last, after all data fields.

The meaning of each field position — which sensor field type it represents — is
determined entirely by the variant table (Section 7).

## 7. Variant Definitions

The variant field in the header selects a field mapping that determines which
field type occupies each presence bit position. This mechanism allows different
sensor types to prioritise their most commonly transmitted fields in Presence
Byte 0, while less frequent fields (such as position and datetime) occupy later
presence bytes and only trigger extension bytes when actually transmitted.

All field encodings (Section 8) are universal and independent of variant. The
variant affects only which encoding type is associated with which field
position, and which label is used in human-readable output and JSON
serialisation.

Fields may be repeated, such as to specify multiple temperature entries which
have different meanings (for example, the temperature of the microcontroller vs.
the temperature of the environment). This is supported by the protocol, but not
the current reference implementation (which will be modified at some future date
to do so).

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

The `fields[]` array is flat: entries 0-5 map to Presence Byte 0, entries 6-12
to Presence Byte 1, entries 13-19 to Presence Byte 2, and so on. Unused trailing
fields should have type `IOTDATA_FIELD_NONE`.

### Default Variant: Weather Station

The built-in default variant (variant 0) is a general-purpose weather station
layout. It is enabled by defining `IOTDATA_VARIANT_MAPS_DEFAULT` at compile
time. It is illustrative and not mandated for this use case: there are no
standardised variants, as global interoperability is not a goal.

| Pres Byte | Field | Type              | Label       | Bits |
| --------- | ----- | ----------------- | ----------- | ---- |
| 0         | S0    | BATTERY           | battery     | 6    |
| 0         | S1    | LINK              | link        | 6    |
| 0         | S2    | ENVIRONMENT       | environment | 24   |
| 0         | S3    | WIND              | wind        | 22   |
| 0         | S4    | RAIN              | rain        | 12   |
| 0         | S5    | SOLAR             | solar       | 14   |
| 1         | S6    | CLOUDS            | clouds      | 4    |
| 1         | S7    | AIR_QUALITY_INDEX | air_quality | 9    |
| 1         | S8    | RADIATION         | radiation   | 30   |
| 1         | S9    | POSITION          | position    | 48   |
| 1         | S10   | DATETIME          | datetime    | 24   |
| 1         | S11   | FLAGS             | flags       | 8    |

This layout prioritises the most commonly transmitted weather data (battery,
environment, wind, rain, solar, link quality) in Presence Byte 0, minimising
packet size for routine transmissions. The less frequently updated fields
(position, datetime, radiation) are placed in Presence Byte 1 and only add to
the packet when present.

Note that the weather station variant uses the ENVIRONMENT, WIND, RAIN, and
RADIATION bundle types (see Sections 8.3, 8.12, 8.16, 8.23) rather than their
individual component types. See Section 8 for a discussion of when to use
bundled vs individual field types.

### Custom Variant Maps

Applications can define their own variant tables at compile time using the
`IOTDATA_VARIANT_MAPS` and `IOTDATA_VARIANT_MAPS_COUNT` defines. This completely
replaces the default variant table.

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

Custom variants may use any combination of the available field types and may
place them in any field position. Up to 15 variants can be registered as variant
IDs 0-14; with variant 15 reserved for the mesh protocol (see Appendix G).

### Registered Variants

| Variant | Name            | Pres Bytes | Fields | Notes                        |
| ------- | --------------- | ---------- | ------ | ---------------------------- |
| 0       | weather_station | 2          | 12     | Default (built-in)           |
| 1-14    | (application)   | —          | —      | User-defined via custom maps |
| 15      | RESERVED        | —          | —      | Mesh protocol (Appendix G)   |

A receiver encountering an unknown variant SHOULD fall back to variant 0's field
mapping and flag the packet as using an unknown variant (see Section 11.4).

## 8. Field Encodings

Each field type has a specified bit layout that is independent of which presence
field it occupies. Fields are always packed MSB-first.

The protocol provides over 20 built-in field types. Some of these exist in both
individual and bundled forms, to aid efficiency for cases where like data (e.g.
temperature, pressure and humidity) are always concurrently measured and
transmitted.

- **Environment** (Section 8.3) is a convenience bundle that packs temperature,
  pressure, and humidity into a single 24-bit field. The same three measurements
  are also available as individual field types: Temperature (8.9), Pressure
  (8.10), and Humidity (8.11). The encodings and quantisation are identical.

- **Wind** (Section 8.12) is a convenience bundle that packs wind speed,
  direction, and gust into a single 22-bit field. The same three measurements
  are also available as individual field types: Wind Speed (8.13), Wind
  Direction (8.14), and Wind Gust (8.15). The encodings and quantisation are
  identical.

- **Rain** (Section 8.16) is a convenience bundle that packs rain rate, and rain
  size into a single 12-bit field. The same two measurements are also available
  as individual field types: Rate Rate (8.16), and Rain Size (8.17). The
  encodings and quantisation are identical.

- **Air Quality** (Section 8.19) is a convenience bundle that packs air quality
  index, air quality pm, and air quality gas into a single multi-bit field. The
  same three measurements are also available as individual field types: Air
  Quality Index (8.20), Air Quality PM (8.21), and Air Quality Gas (8.22). The
  encodings and quantisation are identical.

- **Radiation** (Section 8.23) is a convenience bundle that packs radiation cpm,
  and radiation dose into a single 30-bit field. The same two measurements are
  also available as individual field types: Radiation CPM (8.24), and Radiation
  Dose (8.25). The encodings and quantisation are identical.

A variant definition chooses which form to use. The default weather station
variant uses many of the bundled forms as the sensors generate the entire bundle
of values concurrently. A custom variant might use the individual forms to
include only the specific measurements it needs, or to place them in different
priority positions, or where they are sourced from different sensors at
different times. For example, the commonly used BME280/680 sensor can generate
temperature, pressure and humidity readings concurrently.

Note that at this point, some bundles have no standalone forms, such as the
Solar bundle with Irradiance and Ultraviolet measurements. This may be addressed
in future versions of this protocol.

### 8.1. Battery

6 bits total.

```text
 0   1   2   3   4   5
+---+---+---+---+---+---+
|   Level           |Chg|
|   (5 bits)        |(1)|
+---+---+---+---+---+---+
```

**Level** (5 bits): Battery charge level, quantised from 0-100% to 0-31.

Encode: `q = round(level_pct / 100.0 * 31.0)`

Decode: `level_pct = round(q / 31.0 * 100.0)`

Resolution: ~3.2 percentage points.

**Charging** (1 bit): 1 = charging, 0 = discharging/not charging.

### 8.2. Link

6 bits total.

```text
 0   1   2   3   4   5
+---+---+---+---+---+---+
|   RSSI        | SNR   |
|   (4 bits)    | (2)   |
+---+---+---+---+---+---+
```

**RSSI** (4 bits): Range: -120 to -60 dBm. Resolution: 4 dBm (15 steps).

Encode: `q = (rssi_dbm - (-120)) / 4`

Decode: `rssi_dbm = -120 + q * 4`

**SNR** (2 bits): Range: -20 to +10 dB. Resolution: 10 dB (3 steps: -20, -10, 0,
+10).

Encode: `q = round((snr_db - (-20.0)) / 10.0)`

Decode: `snr_db = -20.0 + q * 10.0`

This field is source-agnostic: while designed for LoRa link metrics, the same
encoding is suitable for 802.11ah or other low-power RF links with comparable
RSSI and SNR ranges.

### 8.3. Environment

24 bits total.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Temperature  | Pressure      | Humidity      |
|  (9 bits)     | (8 bits)      | (7 bits)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Temperature** (9 bits): Range: -40.00°C to +80.00°C. Resolution: 0.25°C (480
steps, 9 bits = 512 values).

Encode: `q = round((temp_c - (-40.0)) / 0.25)`

Decode: `temp_c = -40.0 + q * 0.25`

**Pressure** (8 bits): Range: 850 to 1105 hPa. Resolution: 1 hPa (255 steps).

Encode: `q = pressure_hpa - 850`

Decode: `pressure_hpa = q + 850`

**Humidity** (7 bits): Range: 0 to 100%. Resolution: 1% (7 bits = 128 values,
0-100 used).

Encode/Decode: direct (no quantisation needed).

### 8.4. Solar

14 bits total.

```text
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Irradiance    | UV Idx  |
|   (10 bits)     | (4 bits)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Irradiance** (10 bits): Range: 0 to 1023 W/m². Resolution: 1 W/m². Direct
encoding.

**UV Index** (4 bits): Range: 0 to 15. Direct encoding.

### 8.5. Depth

10 bits total.

Range: 0 to 1023 cm. Resolution: 1 cm. Direct encoding.

This is a generic depth field. The variant label determines its semantic meaning
(snow depth, ice thickness, water level, etc.). The wire encoding is identical
regardless of label.

### 8.6. Flags

8 bits total.

```text
 0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|       Flags (8 bits)          |
+---+---+---+---+---+---+---+---+
```

General-purpose bitmask. Bit assignments are deployment-specific and are not
defined by this protocol. Example uses include: low battery warning, sensor
fault indicators, tamper detection, or configuration acknowledgement flags.

### 8.7. Position

48 bits total.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Latitude (24 bits)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Longitude (24 bits)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Latitude** (24 bits): Range: -90.0° to +90.0°.

Encode: `q = round((lat - (-90.0)) / 180.0 * 16777215.0)`

Decode: `lat = q / 16777215.0 * 180.0 + (-90.0)`

Resolution: 180.0 / 16777215 ≈ 0.00001073° ≈ 1.19 metres at the equator.

**Longitude** (24 bits): Range: -180.0° to +180.0°.

Encode: `q = round((lon - (-180.0)) / 360.0 * 16777215.0)`

Decode: `lon = q / 16777215.0 * 360.0 + (-180.0)`

Resolution: 360.0 / 16777215 ≈ 0.00002146° ≈ 2.39 metres at the equator,
reducing with cos(latitude).

This field is source-agnostic. The position may originate from a GNSS receiver,
WiFi geolocation, cell tower triangulation, or static configuration. The
protocol does not indicate the source or its accuracy; see Section 11.2 and 11.3
for discussion.

### 8.8. Datetime

24 bits total.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Ticks (24 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Ticks** (24 bits): Time offset from January 1 00:00:00 UTC of the current
year, measured in 5-second ticks.

Encode: `ticks = seconds_from_year_start / 5`

Decode: `seconds = ticks * 5`

Maximum value: 16,777,215 ticks = 83,886,075 seconds ≈ 970.9 days.

Resolution: 5 seconds.

The year is NOT transmitted. The receiver resolves the year using its own clock;
see Section 11.1 for the year resolution algorithm.

This field is source-agnostic. The time may originate from a GNSS receiver, NTP
synchronisation, or a local RTC. The protocol does not indicate the source or
its drift characteristics; see Section 11.3.

### 8.9. Temperature (standalone)

9 bits total.

Range: -40.00°C to +80.00°C. Resolution: 0.25°C (480 steps, 9 bits = 512
values).

Encode: `q = round((temp_c - (-40.0)) / 0.25)`

Decode: `temp_c = -40.0 + q * 0.25`

This is the same encoding as the temperature component of the Environment bundle
(Section 8.2). Use this standalone type in variants that need temperature
without pressure and humidity.

### 8.10. Pressure (standalone)

8 bits total.

Range: 850 to 1105 hPa. Resolution: 1 hPa (255 steps).

Encode: `q = pressure_hpa - 850`

Decode: `pressure_hpa = q + 850`

This is the same encoding as the pressure component of the Environment bundle
(Section 8.2).

### 8.11. Humidity (standalone)

7 bits total.

Range: 0 to 100%. Resolution: 1% (7 bits = 128 values, 0-100 used).

Encode/Decode: direct (no quantisation needed).

This is the same encoding as the humidity component of the Environment bundle
(Section 8.2).

### 8.12. Wind (bundle)

22 bits total.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Speed      | Direction     |  Gust       |
|  (7 bits)   | (8 bits)      | (7 bits)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs wind speed, direction, and gust speed
into a single field. The component encodings are identical to the standalone
Wind Speed (8.13), Wind Direction (8.14), and Wind Gust (8.15) types.

**Speed** (7 bits): Range: 0 to 63.5 m/s. Resolution: 0.5 m/s.

Encode: `q = round(speed_ms / 0.5)`

Decode: `speed_ms = q * 0.5`

**Direction** (8 bits): Range: 0° to 355° (true bearing). Resolution: ~1.41°
(360/256).

Encode: `q = round(direction_deg / 360.0 * 256.0) & 0xFF`

Decode: `direction_deg = q / 256.0 * 360.0`

**Gust** (7 bits): Range: 0 to 63.5 m/s. Resolution: 0.5 m/s.

Encode/Decode: same as Speed.

### 8.13. Wind Speed (standalone)

7 bits total.

Range: 0 to 63.5 m/s. Resolution: 0.5 m/s.

Encode: `q = round(speed_ms / 0.5)`

Decode: `speed_ms = q * 0.5`

Same encoding as the speed component of the Wind bundle (8.12).

### 8.14. Wind Direction (standalone)

8 bits total.

Range: 0° to 355° (true bearing). Resolution: ~1.41° (360/256).

Encode: `q = round(direction_deg / 360.0 * 256.0) & 0xFF`

Decode: `direction_deg = q / 256.0 * 360.0`

Same encoding as the direction component of the Wind bundle (8.12).

### 8.15. Wind Gust (standalone)

7 bits total.

Range: 0 to 63.5 m/s. Resolution: 0.5 m/s.

Encode: `q = round(gust_ms / 0.5)`

Decode: `gust_ms = q * 0.5`

Same encoding as the gust component of the Wind bundle (8.12).

### 8.16. Rain (bundle)

12 bits total.

```text
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+
|  Rate         | Size  |
|  (8 bits)     | (4)   |
+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs rain rate, and size into a single field.
The component encodings are identical to the standalone Rain Rate (8.17), and
Rain Size (8.18) types.

**Rate** (8 bits):

Range: 0 to 255 mm/hr. Resolution: 1 mm/hr. Direct encoding.

**Size** (4 bits):

Range: 0 to 6.0mm. Resolution: 0.25 mm.

Encode: `q = round(rain_size / 0.25)`

Decode: `rain_size = q * 0.25`

### 8.17. Rain Rate (standalone)

8 bits total.

Range: 0 to 255 mm/hr. Resolution: 1 mm/hr. Direct encoding.

### 8.18. Rain Size (standalone)

4 bits total.

Range: 0 to 6.0mm. Resolution: 0.25 mm.

Encode: `q = round(rain_size / 0.25)`

Decode: `rain_size = q * 0.25`

### 8.19. Air Quality (bundle)

Variable length (minimum 21 bits).

This is a convenience bundle that packs air quality index, particulate matter,
and gas readings into a single field. The component encodings are identical to
the standalone Air Quality Index (8.19), Air Quality PM (8.20), and Air Quality
Gas (8.21) types.

```text
+-----------+-----------+-----------+
| AQ Index  | AQ PM     | AQ Gas    |
| (9 bits)  | (4+ bits) | (8+ bits) |
+-----------+-----------+-----------+
```

The three sub-fields are packed in order: index, PM, gas. Each sub-field
includes its own presence mask, so absent PM channels and gas slots consume no
bits beyond the mask itself.

Minimum: 9 (index) + 4 (PM mask, no channels) + 8 (gas mask, no slots) = 21
bits. Typical SEN55 full reading: 9 + 36 + 24 = 69 bits.

### 8.20. Air Quality Index (standalone)

9 bits total.

Range: 0 to 500 AQI (Air Quality Index). Resolution: 1 AQI. Direct encoding (9
bits = 512 values, 0-500 used).

### 8.21. Air Quality PM (standalone)

4 to 36 bits total (variable).

```text
 0
 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+- - - - -+
|P|P|P|P| ch0   | ch1 ...  (8 bits per present channel)
|1|25|4|10|       |
+-+-+-+-+-+-+-+-+- - - - -+
```

4-bit presence mask followed by 8 bits for each present PM channel. Resolution:
5 µg/m³.

**Presence mask** (4 bits):

- Bit 0: PM1 present
- Bit 1: PM2.5 present
- Bit 2: PM4 present
- Bit 3: PM10 present

**Each channel** (8 bits):

Range: 0 to 1275 µg/m³. Resolution: 5 µg/m³ (255 steps).

Encode: `q = value_ugm3 / 5`

Decode: `value_ugm3 = q * 5`

The 5 µg/m³ resolution matches the ±5 µg/m³ precision of typical
laser-scattering PM sensors (e.g. Sensirion SEN55, Plantower PMS5003).

Typical sensors output all four channels simultaneously; a presence mask of 0xF
(all present) with 4 × 8 = 32 data bits is the common case, giving 36 bits
total.

### 8.22. Air Quality Gas (standalone)

8 to 84 bits total (variable).

```text
 0
 0 1 2 3 4 5 6 7 8 9 ...
+-+-+-+-+-+-+-+-+- - - - - - -+
|V|N|C|C|H|O|R|R| slot0 | slot1 ...
|O|O|O|O|C|3|6|7|       |
|C|X|2| |H| | | |       |
+-+-+-+-+-+-+-+-+- - - - - - -+
```

8-bit presence mask followed by data for each present gas slot. Each slot has a
fixed bit width and resolution determined by its position in the mask.

**Presence mask** (8 bits):

- Bit 0: VOC Index
- Bit 1: NOx Index
- Bit 2: CO₂
- Bit 3: CO
- Bit 4: HCHO (formaldehyde)
- Bit 5: O₃ (ozone)
- Bit 6: Reserved
- Bit 7: Reserved

**Slot encodings**:

| Slot | Gas  | Bits | Resolution  | Range    | Unit |
| ---- | ---- | ---- | ----------- | -------- | ---- |
| 0    | VOC  | 8    | 2 index pts | 0-510    | idx  |
| 1    | NOx  | 8    | 2 index pts | 0-510    | idx  |
| 2    | CO₂  | 10   | 50 ppm      | 0-51,150 | ppm  |
| 3    | CO   | 10   | 1 ppm       | 0-1,023  | ppm  |
| 4    | HCHO | 10   | 5 ppb       | 0-5,115  | ppb  |
| 5    | O₃   | 10   | 1 ppb       | 0-1,023  | ppb  |
| 6    | Rsvd | 10   | —           | —        | —    |
| 7    | Rsvd | 10   | —           | —        | —    |

Encode: `q = value / resolution`

Decode: `value = q * resolution`

VOC and NOx index slots carry Sensirion SGP4x-style algorithm indices (1-500
typical). The 2-point resolution is well within the ±15/±50 index point
device-to-device variation.

CO₂ at 50 ppm resolution covers the full SCD4x range (0-40,000 ppm) and exceeds
its ±40 ppm + 5% accuracy.

HCHO at 5 ppb resolution matches the ~10 ppb accuracy of typical electrochemical
formaldehyde sensors (e.g. Sensirion SEN69C, Dart WZ-S).

A typical Sensirion SEN55 station (VOC + NOx) sends 8 + 8 + 8 = 24 bits. A SEN66
station (VOC + NOx + CO₂) sends 8 + 8 + 8 + 10 = 34 bits.

### 8.23. Radiation (bundle)

28 bits total.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  CPM                      | Dose                      |
|  (14 bits)                | (14 bits)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

This is a convenience bundle that packs radiation CPM, and dose into a single
field. The component encodings are identical to the standalone Radiation CPM
(8.21), and Radiation Dose (8.22) types.

**CPM** (14 bits):

Range: 0 to 16383 counts per minute (CPM). Resolution: 1 CPM. Direct encoding.

This field carries the raw count rate from a Geiger-Müller tube or similar
radiation detector.

**Dose** (14 bits):

Range: 0 to 163.83 µSv/h. Resolution: 0.01 µSv/h (16,383 steps).

Encode: `q = round(dose_usvh / 0.01)`

Decode: `dose_usvh = q * 0.01`

This field carries the computed dose rate. The relationship between CPM and dose
rate is detector-specific and is not defined by this protocol.

### 8.24. Radiation CPM (standalone)

14 bits total.

Range: 0 to 16383 counts per minute (CPM). Resolution: 1 CPM. Direct encoding.

This field carries the raw count rate from a Geiger-Müller tube or similar
radiation detector.

### 8.25. Radiation Dose (standalone)

14 bits total.

Range: 0 to 163.83 µSv/h. Resolution: 0.01 µSv/h (16,383 steps).

Encode: `q = round(dose_usvh / 0.01)`

Decode: `dose_usvh = q * 0.01`

This field carries the computed dose rate. The relationship between CPM and dose
rate is detector-specific and is not defined by this protocol.

### 8.26. Clouds

4 bits total.

Range: 0 to 8 okta. Resolution: 1 okta. Direct encoding (4 bits = 16 values, 0-8
used).

Clouds measures cloud cover in okta (eighths of sky covered), following the
standard meteorological convention where 0 = clear sky and 8 = fully overcast.

### 8.27. Image

Variable length. Minimum 2 bytes (length + control), maximum 256 bytes (length +
control + 254 bytes of pixel data).

```text
 0               1
 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-  ...  -+
|  Length (8)    |  Control (8)  | Pixel Data   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-  ...  -+
```

This is the only variable-length data field in the protocol. The length byte at
the start tells the decoder how many additional bytes follow, allowing the field
to be skipped without understanding its contents.

**Length** (8 bits): Number of bytes that follow the length byte, including the
control byte and all pixel data. Range: 1-255.

A length of 1 indicates a control byte only with no pixel data. This is not
useful in practice but is legal.

The total field size in bytes is `1 + Length`. The total field size in bits is
`(1 + Length) × 8`.

**Control** (8 bits): Describes the pixel format, image dimensions, compression
method, and flags. The decoder reads this byte to determine how to interpret all
subsequent bytes.

```text
 0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
| Format| Size  | Comp  | Flags |
| (2)   | (2)   | (2)   | (2)   |
+---+---+---+---+---+---+---+---+
```

**Format** (bits 7-6): Pixel depth.

| Value | Name    | Bits/pixel | Description                       |
| ----- | ------- | ---------- | --------------------------------- |
| 0     | BILEVEL | 1          | Black and white (1-bit per pixel) |
| 1     | GREY4   | 2          | 4-level greyscale                 |
| 2     | GREY16  | 4          | 16-level greyscale                |
| 3     |         | —          | Reserved                          |

For BILEVEL, each pixel is a single bit: 0 = black, 1 = white. Pixels are packed
MSB-first within each byte, left-to-right across each row, rows top-to-bottom.

For GREY4, each pixel is 2 bits: 0 = black, 1 = dark grey, 2 = light grey, 3 =
white. Pixels are packed MSB-first, four pixels per byte.

For GREY16, each pixel is 4 bits (one nibble): 0 = black, 15 = white. Pixels are
packed high-nibble-first, two pixels per byte.

**Size** (bits 5-4): Image dimensions (width × height).

| Value | Dimensions | Pixels | Raw bytes (1bpp) | Raw bytes (4bpp) |
| ----- | ---------- | ------ | ---------------- | ---------------- |
| 0     | 24 × 18    | 432    | 54               | 216              |
| 1     | 32 × 24    | 768    | 96               | 384              |
| 2     | 48 × 36    | 1,728  | 216              | 864              |
| 3     | 64 × 48    | 3,072  | 384              | 1,536            |

All sizes use a 4:3 aspect ratio. The size tier determines both width and
height; non-standard dimensions are not supported.

**Compression** (bits 3-2): Compression method applied to pixel data.

| Value | Name       | Description                          |
| ----- | ---------- | ------------------------------------ |
| 0     | RAW        | Uncompressed pixel data              |
| 1     | RLE        | Run-length encoding (Section 8.27.1) |
| 2     | HEATSHRINK | Heatshrink LZSS (Section 8.27.2)     |
| 3     |            | Reserved                             |

**Flags** (bits 1-0):

| Bit | Name     | Description                                       |
| --- | -------- | ------------------------------------------------- |
| 1   | FRAGMENT | This image is a fragment; more fragments follow   |
| 0   | INVERT   | Display with inverted polarity (0=white, 1=black) |

The FRAGMENT flag enables multi-packet image transmission for cases where the
pixel data exceeds the available payload. Fragments share the same control byte;
the receiver reassembles using the packet sequence number and station_id. For
v1, single-frame images (FRAGMENT = 0) are the expected case.

The INVERT flag indicates that the pixel sense is reversed. This is useful for
difference-frame images where motion pixels are naturally encoded as 1 (white on
black background). The flag allows the display layer to render with the correct
visual polarity without the encoder needing to invert the pixel data.

#### Design Philosophy

The Image field defines a container for a rectangular pixel grid. It does not
specify what the pixels represent. The sensor implementation decides what is
most informative — a full-frame downscale, a cropped region-of-interest around
detected motion, a background-subtracted difference mask, a depth map, or any
other rectangular image. The field carries the result; the semantics are a
property of the sensor and variant, not the encoding.

#### Variable-Length Decoding

Unlike all other data fields in the protocol, Image has a variable bit width.
The decoder handles this as follows:

1. The presence bit for the Image slot is set.
2. The decoder reads the first byte (Length).
3. The decoder consumes `Length` additional bytes.
4. Decoding continues at the next field's bit offset.

Implementations that do not support Image can skip the field by reading the
length byte and advancing by `Length` bytes, without interpreting the control
byte or pixel data. This preserves forward compatibility: a decoder compiled
without Image support can still decode all other fields in the packet.

#### 8.27.1. RLE Compression

When Compression = RLE, the pixel data is encoded as a sequence of run-length
pairs. Each pair is a single byte:

```text
 0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|Val|       Run Length (7)       |
+---+---+---+---+---+---+---+---+
```

For BILEVEL format, **Val** (bit 7) is the pixel value (0 or 1) and **Run
Length** (bits 6-0) is the number of consecutive pixels with that value, minus 1
(range 1-128 pixels per run).

For GREY4 and GREY16 formats, the encoding switches to a byte-pair scheme: the
first byte is a raw pixel value (2 or 4 bits, zero-padded to 8 bits) and the
second byte is the run count minus 1. This produces 2 bytes per run but handles
the wider pixel values cleanly.

Runs that exceed 128 pixels (BILEVEL) or 256 pixels (greyscale) are split into
consecutive run entries with the same value.

The decoder reconstructs the pixel grid left-to-right, top-to-bottom, consuming
runs until width × height pixels have been produced.

RLE is particularly effective for BILEVEL images with large uniform regions,
such as background-subtracted motion frames, where compression ratios of 2:1 to
6:1 are typical.

#### 8.27.2. Heatshrink Compression

When Compression = HEATSHRINK, the pixel data (in its raw packed form) has been
compressed using the heatshrink LZSS algorithm.

The heatshrink parameters are fixed by this protocol and MUST NOT be varied
per-packet:

- Window size: 8 (256-byte window)
- Lookahead size: 4 (16-byte lookahead)

These parameters are chosen for minimal RAM usage at the decoder (approximately
256 bytes for decompression state) while still providing useful compression. The
decoder does not need to be told the parameters; they are implicit in the field
type.

Heatshrink is most useful for GREY4 and GREY16 formats where pixel data has more
entropy than BILEVEL and simple RLE is less effective.

#### 8.27.3. Payload Budget

The length byte (8 bits) limits the field value to 255 bytes after the length
byte itself: 1 control byte plus up to 254 bytes of pixel data.

The following table shows which format/size combinations fit within 254 bytes
without compression:

| Size    | BILEVEL (1bpp) | GREY4 (2bpp) | GREY16 (4bpp) |
| ------- | :------------: | :----------: | :-----------: |
| 24 × 18 |     54 B ✓     |   108 B ✓    |    216 B ✓    |
| 32 × 24 |     96 B ✓     |   192 B ✓    |    384 B ✗    |
| 48 × 36 |    216 B ✓     |   432 B ✗    |    864 B ✗    |
| 64 × 48 |    384 B ✗     |   768 B ✗    |   1,536 B ✗   |

Combinations marked ✗ require compression to fit. In practice, BILEVEL at 32 ×
24 (96 bytes raw, typically 40-60 bytes with RLE) is the recommended default for
single-frame LoRa transmission. It provides sufficient resolution to distinguish
human silhouettes, vehicles, and animals while leaving substantial room for
other iotdata fields in the same packet.

The LoRa payload limit (222 bytes at SF7/125kHz, 115 bytes at SF9, 51 bytes at
SF10) further constrains the practical combinations. For higher spreading
factors, 24 × 18 BILEVEL with RLE is the safest choice.

#### 8.27.4. Recommended Practices

- **Default choice:** BILEVEL format, 32 × 24 size, RLE compression. This
  produces 40-60 byte thumbnails for typical motion frames, fits comfortably in
  a single LoRa packet at any spreading factor, and requires trivial
  encode/decode logic.

- **ROI cropping:** If the sensor detects motion in a small region of the camera
  frame, cropping to that region before downscaling preserves more detail than
  downscaling the entire frame. The Image field does not carry crop coordinates;
  these are a property of the sensor's processing pipeline, not the transport
  encoding.

- **Difference frames:** For background-subtracted motion images, set the INVERT
  flag if the natural encoding is white-on-black (motion pixels = 1). The
  resulting BILEVEL image compresses exceptionally well with RLE due to large
  background regions.

- **Greyscale use:** GREY16 at 24 × 18 with heatshrink (216 bytes raw, typically
  100-150 bytes compressed) provides a richer visual at the cost of decode
  complexity. Use when the MCU has sufficient resources and the additional
  visual detail is valuable.

- **Multi-frame spanning:** The FRAGMENT flag enables splitting a large
  thumbnail across multiple packets. The gateway reassembles fragments using
  {station_id, sequence} ordering. This adds complexity and fragility (any lost
  fragment invalidates the image) and is not recommended for v1 deployments.

#### 8.27.5. JSON Representation

In the canonical JSON output, the Image field is represented as a structured
object under its variant label (e.g. `"image"`, `"thumbnail"`, `"motion_image"`,
depending on the variant map definition):

```json
{
  "image": {
    "format": "bilevel",
    "size": "32x24",
    "compression": "rle",
    "fragment": false,
    "invert": false,
    "pixels": "base64-encoded-pixel-data"
  }
}
```

The gateway performs decompression before base64-encoding the `pixels` field, so
downstream consumers receive uniform raw pixel data regardless of the
compression method used on the wire.

- `format`: One of `"bilevel"`, `"grey4"`, `"grey16"`.
- `size`: One of `"24x18"`, `"32x24"`, `"48x36"`, `"64x48"`.
- `compression`: One of `"raw"`, `"rle"`, `"heatshrink"`.
- `fragment`: Boolean.
- `invert`: Boolean.
- `pixels`: Base64-encoded decompressed pixel data.

The `compression` field records the wire method for diagnostics but is not
needed for rendering.

## 9. TLV Data

The TLV (Type-Length-Value) section provides an extensible mechanism for
diagnostic data, firmware metadata, user-defined payloads, and future sensor
metadata. It is present only when the TLV bit (bit 6 of Presence Byte 0) is set.
By preference, it should not be used for sensor data per se: such data should
have a designated field type.

The TLV section begins immediately after the last data field, at whatever bit
offset that field ended. There is no alignment padding.

### 9.1. TLV Header

Each TLV entry begins with a 16-bit header:

```text
 0                               1
 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
|Fmt|       Type (6)        |Mor|           Length (8)          |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

**Format** (1 bit): 0 = raw bytes. 1 = packed 6-bit string.

**Type** (6 bits): Application-defined type identifier, range 0-63. See Section
9.4 for types.

**More** (1 bit): 1 = another TLV entry follows this one. 0 = this is the last
TLV entry.

**Length** (8 bits): For raw format: number of data bytes (0-255). For string
format: number of characters (0-255).

### 9.2. Raw Format

When Format = 0, the data section is `Length` bytes (Length × 8 bits), packed
MSB-first with no alignment.

Total TLV entry size: 16 + (Length × 8) bits.

### 9.3. Packed String Format

When Format = 1, each character is encoded as 6 bits using the character table
in Appendix A. This saves 25% compared to 8-bit ASCII for the supported
character set (alphanumeric plus space).

Total TLV entry size: 16 + (Length × 6) bits.

Characters outside the 6-bit table MUST NOT be transmitted. An encoder MUST
reject strings containing unencodable characters.

### 9.4. TLV Types

| Type      | Name        | Format | Description                                        |
| --------- | ----------- | ------ | -------------------------------------------------- |
| 0x01-0x0F | (reserved)  | —      | Reserved for globally designated TLVs              |
| 0x01      | VERSION     | string | Firmware and hardware version identification       |
| 0x02      | STATUS      | raw    | Uptime, lifetime uptime, restart count and reason  |
| 0x03      | HEALTH      | raw    | CPU temperature, supply voltage, heap, active time |
| 0x04      | CONFIG      | string | Configuration key-value pairs                      |
| 0x05      | DIAGNOSTIC  | string | Free-form diagnostic message                       |
| 0x06      | USERDATA    | string | User interaction event                             |
| 0x08-0x0F | (reserved)  | —      | Reserved for future globally designated TLVs       |
| 0x10-0x1F | (reserved)  | —      | Reserved for future quality/metadata TLVs          |
| 0x20-     | (available) | —      | Available for proprietary TLVs                     |

Types 0x01-0x0F are reserved for globally designated types, as specified in, and
extended by, this document. They have encoding functions provided in the
reference implementation. Types 0x10-0x1F are reserved for sensor metadata (see
Section 11.3 and Section 15) and may have future reference implementation
support. Types 0x20 onwards are available for application use.

### 9.5. Global TLV Types

The following TLV types are globally designated and have fixed semantics across
all variants and deployments. Implementations SHOULD use these types for their
intended purpose to aid interoperability between sensors, gateways, and
downstream consumers.

All global TLV types are optional. A sensor includes them when the information
is available and the payload budget permits. The recommended transmission
strategy varies by type:

- **VERSION**: Once at boot (first packet after restart).
- **STATUS**: Every Nth packet (e.g. every 10th), or periodically.
- **HEALTH**: Less frequently (e.g. every 50th), or when signficantly changed.
- **CONFIG**: Once at boot, or after configuration changes.
- **DIAGNOSTIC**: When a notable condition occurs.
- **USERDATA**: When a user interaction event occurs.

#### 9.5.1. Version (0x01)

Variable length, string format.

Identifies the firmware and hardware versions running on the device. This is
essential for fleet management: knowing which devices are running which firmware
version after an OTA campaign, or identifying hardware revisions with known
issues.

The content uses the same space-delimited key-value convention as Config
(Section 9.5.4), encoded with the 6-bit packed character set (Appendix A):

```text
KEY1 VALUE1 KEY2 VALUE2 ...
```

Recommended keys:

| Key | Description                                        |
| --- | -------------------------------------------------- |
| FW  | Firmware version (build number or encoded version) |
| HW  | Hardware revision                                  |
| BL  | Bootloader version                                 |
| ID  | Device model or type identifier                    |
| SN  | Serial number or unique identifier                 |

Examples:

- `FW 142 HW 3` — firmware build 142, hardware revision 3
- `FW 20401 HW 2 BL 5` — firmware 2.4.1 (encoded as 20401), hardware rev 2,
  bootloader 5
- `ID SNOWV2 FW 38 HW 1` — device model SNOWV2, firmware 38
- `FW 12 HW 1 SN A04F` — with serial number

The key namespace is the same as Config: application-defined, short uppercase
identifiers. The keys listed above are recommendations, not requirements. A
minimal implementation may send only `FW` and `HW`.

Since version information is static within a boot cycle, this TLV is typically
sent only in the first packet after a restart. The gateway or upstream system
can cache it per station_id.

Since the 6-bit character set does not include dots or hyphens, semantic version
strings such as `2.4.1` cannot be encoded directly. Recommended alternatives:

- Concatenated digits: `20401` for 2.4.1 (convention: MMPPP where
  MM=major×100+minor, PPP=patch).
- Plain build number: `142` (monotonically increasing).
- Separate keys: `FWMAJ 2 FWMIN 4 FWPAT 1` (verbose but explicit).

The build number approach is simplest and sufficient for most deployments.

**JSON representation:**

```json
{
  "type": 1,
  "format": "version",
  "data": {
    "FW": "142",
    "HW": "3"
  }
}
```

The gateway parses the space-delimited tokens into key-value pairs, identical to
the Config JSON representation.

#### 9.5.2. Status (0x02)

9 bytes, raw format.

Reports device boot lifecycle: how long since last restart, how long the device
has been alive in total across all boots, how many times it has restarted, and
why the most recent restart occurred.

```text
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Session Uptime (24 bits)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Lifetime Uptime (24 bits)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Restarts (16 bits)    | Reason (8)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Session Uptime** (3 bytes, uint24, big-endian): Time since the most recent
boot, measured in 5-second ticks. This matches the resolution and encoding of
the Datetime field (Section 8.8).

Encode: `ticks = uptime_seconds / 5`

Decode: `seconds = ticks * 5`

Maximum: 16,777,215 ticks = 83,886,075 seconds ≈ 970.9 days.

**Lifetime Uptime** (3 bytes, uint24, big-endian): Total accumulated uptime
across all boots since first commissioning, measured in 5-second ticks. Same
encoding as session uptime.

This value requires non-volatile storage (NVS, EEPROM, or flash). The device
persists the accumulated total periodically (e.g. every hour or at shutdown) and
adds the current session uptime when encoding the TLV.

Devices that do not track lifetime uptime MUST transmit 0x000000. The receiver
interprets this as "not tracked" rather than "zero uptime".

**Restarts** (2 bytes, uint16, big-endian): Total number of device starts since
first commissioning, including the current boot. Wraps at 65535. A value of 1
indicates the device has never restarted since first power-on.

**Reason** (1 byte, uint8): Reason for the most recent restart. Bit 7 determines
the interpretation:

- **Bit 7 clear (0x00-0x7F):** Globally defined reason codes, specified by this
  protocol. All implementations MUST use these values for the corresponding
  conditions.

- **Bit 7 set (0x80-0xFF):** Vendor-specific or device-specific reason codes.
  The interpretation depends on the device type and firmware. Receivers that do
  not recognise a vendor-specific code SHOULD display it as a numeric value.

Globally defined reason codes:

| Value     | Name       | Description                                  |
| --------- | ---------- | -------------------------------------------- |
| 0x00      | UNKNOWN    | Reason not available or not determined       |
| 0x01      | POWER_ON   | Cold boot (initial power application)        |
| 0x02      | SOFTWARE   | Intentional software-initiated reset         |
| 0x03      | WATCHDOG   | Watchdog timer expiry                        |
| 0x04      | BROWNOUT   | Supply voltage dropped below threshold       |
| 0x05      | PANIC      | Unrecoverable software fault or exception    |
| 0x06      | DEEPSLEEP  | Wake from deep sleep (normal operation)      |
| 0x07      | EXTERNAL   | External reset pin or button                 |
| 0x08      | OTA        | Reset following over-the-air firmware update |
| 0x09-0x7F | (reserved) | Reserved for future globally defined reasons |

Most microcontrollers expose the reset reason register at boot. For example,
ESP32 provides `esp_reset_reason()` and STM32 provides `__HAL_RCC_GET_FLAG()`.
The encoder maps the platform-specific value to the nearest globally defined
code where possible, or to a vendor-specific code (0x80+) for platform-specific
conditions that have no global equivalent.

The DEEPSLEEP reason (0x06) is expected in normal operation for battery-powered
sensors that sleep between transmission cycles. A high restart count with
DEEPSLEEP reason is healthy; a high restart count with WATCHDOG or PANIC reason
indicates a fault.

**JSON representation:**

```json
{
  "type": 2,
  "format": "status",
  "data": {
    "session_uptime": 86400,
    "lifetime_uptime": 1209600,
    "restarts": 12,
    "reason": "watchdog"
  }
}
```

The gateway destructures the 9-byte raw data into named fields. Uptime values
are converted to seconds (ticks × 5) for the JSON output. A lifetime_uptime of 0
is omitted from the JSON or represented as `null` to indicate "not tracked". The
`reason` field is a lowercase string using the name column from the reason table
for globally defined codes (0x00-0x7F), or the numeric value for vendor-specific
codes (e.g. `"reason": 131`).

#### 9.5.3. Health (0x03)

7 bytes, raw format.

Reports runtime hardware state: thermal, electrical, memory, and duty cycle
metrics. These change during operation and are useful for detecting overheating,
power supply issues, memory leaks, and validating power budgets.

```text
 0               1               2
 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| CPU Temp (8)  |      Supply Voltage (16)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Free Heap (16)        | Active (16)   :
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
:  Active cont. |
+-+-+-+-+-+-+-+-+
```

**CPU Temperature** (1 byte, int8, signed): Internal die temperature in degrees
Celsius. Range: -40 to +85°C. Resolution: 1°C.

Most MCUs have an internal temperature sensor: ESP32 provides
`temperatureRead()`, STM32 provides an internal ADC channel. The reading
reflects die temperature, which is typically 5-15°C above ambient depending on
workload and packaging.

Devices without an internal temperature sensor MUST transmit 0x7F (127). This
value is outside the normal operating range and the receiver interprets it as
"not available".

**Supply Voltage** (2 bytes, uint16, big-endian): Raw supply rail voltage in
millivolts. Range: 0-65535 mV.

This is distinct from the Battery field (Section 8.1) which reports a percentage
level. Supply voltage provides absolute electrical data: solar panel output
voltage, regulator headroom, voltage sag under transmit load, or direct battery
voltage before any regulation.

For devices powered via a regulated 3.3V rail, this may be a fixed value and is
less informative. For solar-powered devices with a wide input range, this is a
key diagnostic.

**Free Heap** (2 bytes, uint16, big-endian): Remaining free heap memory in
bytes. Range: 0-65535.

ESP32 provides `esp_get_free_heap_size()`. For devices with more than 65535
bytes free, report 65535 (capped). A steadily decreasing free heap over time
indicates a memory leak.

Devices without dynamic memory allocation or without a mechanism to query free
heap MUST transmit 0xFFFF (65535). Since this is also the cap value, the
receiver treats it as "healthy or not tracked".

**Session Active** (2 bytes, uint16, big-endian): Accumulated time spent in
active state (not in deep sleep) since the most recent boot, measured in
5-second ticks.

Maximum: 65535 ticks = 327675 seconds ≈ 91.0 hours.

The firmware increments this counter each time it wakes from sleep, accumulating
the duration of each active period. Comparing session active to session uptime
(from Status, Section 9.5.2) yields the duty cycle:

`duty_cycle = session_active / session_uptime`

A sensor with 86400s session uptime but 200 active ticks (1000s) has a duty
cycle of ~1.2%, confirming that power budgets are being met.

For devices that do not sleep (always-on gateways, relay nodes), session active
equals session uptime and this field provides no additional information. Such
devices may omit the Health TLV or set session active to 0x0000 to indicate "not
tracked".

**JSON representation:**

```json
{
  "type": 3,
  "format": "health",
  "data": {
    "cpu_temp": 34,
    "supply_mv": 3842,
    "free_heap": 42816,
    "session_active": 1050
  }
}
```

The gateway destructures the 7-byte raw data into named fields. The `cpu_temp`
is signed degrees Celsius. The `supply_mv` is millivolts. The `free_heap` is
bytes. The `session_active` is converted to seconds (ticks × 5).

A `cpu_temp` of 127 is omitted from the JSON or represented as `null` to
indicate "not available".

#### 9.5.4. Config (0x04)

Variable length, string format.

Reports current device configuration as space-delimited key-value pairs, encoded
using the 6-bit packed character set (Appendix A).

The content is a sequence of alternating tokens separated by single spaces:

```text
KEY1 VALUE1 KEY2 VALUE2 ...
```

Odd-position tokens (1st, 3rd, 5th, ...) are keys. Even-position tokens (2nd,
4th, 6th, ...) are values. The total token count MUST be even (every key has a
corresponding value).

Keys and values MUST NOT contain spaces. Keys and values may use any length and
any mix of characters available in the 6-bit character set. Short uppercase
identifiers are recommended for keys to minimise wire size, but this is a
convention, not a requirement.

Examples:

- `TX 30 SF 7 PW 14 CH 23` — radio configuration
- `INT 10 BAT LOW` — 10-second interval, battery threshold LOW
- `MODE NORMAL THRESH 50` — operating mode and threshold
- `FW 142 HW 3` — firmware version 142, hardware revision 3

The key namespace is application-defined and not standardised by this protocol.
Different sensor types may use different keys. The receiver presents the pairs
as-is; it does not need to understand the key semantics.

In the rare case where configuration values contain characters outside the 6-bit
set, raw format (Format = 0) MAY be used with 8-bit ASCII bytes following the
same space-delimited convention. This should be avoided where possible.

**JSON representation:**

```json
{
  "type": 4,
  "format": "config",
  "data": {
    "TX": "30",
    "SF": "7",
    "PW": "14",
    "CH": "23"
  }
}
```

The gateway parses the space-delimited tokens into alternating key-value pairs
and presents them as a JSON object. Both keys and values are strings.

#### 9.5.5. Diagnostic (0x05)

Variable length, string format.

A free-form diagnostic message from the device. This is the device's mechanism
for reporting conditions that do not map to any structured field: error
messages, warning strings, state transitions, or any other human-readable
diagnostic information.

The message is encoded using the 6-bit packed character set (Appendix A). This
covers uppercase alphanumeric characters, digits, and space — sufficient for
diagnostic messages.

Examples:

- `SENSOR FAULT I2C`
- `LOW SIGNAL`
- `SD FULL`
- `LORA TX FAIL 3`
- `BME280 CRC ERR`

There is no structure imposed on the message content. The protocol does not
define severity levels, error codes, or categories. Conventions such as
prefixing with a subsystem name (`I2C`, `LORA`, `SD`) are recommended but not
required.

In the rare case where a diagnostic message contains characters outside the
6-bit set, raw format (Format = 0) MAY be used with 8-bit ASCII bytes. This
should be avoided where possible as it increases the wire size by 33%.

Multiple diagnostic messages may be sent by chaining TLV entries using the More
bit. Each entry carries one message.

**JSON representation:**

```json
{ "type": 5, "format": "string", "data": "SENSOR FAULT I2C" }
```

The `format` field reflects the wire encoding (`"string"` or `"raw"` in the
exceptional case). The `data` field is always a decoded text string regardless
of wire format.

#### 9.5.6. Userdata (0x06)

Variable length, string format.

Reports a user-initiated event or interaction, encoded using the 6-bit packed
character set (Appendix A).

This covers any event that originates from physical user interaction with the
device rather than from automated sensor readings: button presses, switch
changes, mode selections, tamper detection, or manual triggers.

The content is a free-form string describing the event. Examples:

- `BTN A` — button A pressed
- `BTN B LONG` — button B long-press
- `MODE 2` — user selected operating mode 2
- `TAMPER` — enclosure tamper switch triggered
- `ARM` — user armed the device
- `CAL START` — user initiated calibration
- `DOOR OPEN` — door sensor triggered

No structure is imposed on the message content. The sensor firmware defines the
event vocabulary appropriate to its hardware and application.

In the rare case where an event description contains characters outside the
6-bit set, raw format (Format = 0) MAY be used with 8-bit ASCII bytes. This
should be avoided where possible.

**JSON representation:**

```json
{ "type": 6, "format": "string", "data": "BTN A" }
```

The `format` field reflects the wire encoding. The `data` field is a decoded
text string.

## 10. Canonical JSON Representation

Gateways and servers typically convert binary packets to JSON for storage,
forwarding, and human inspection. The reference implementation provides
bidirectional conversion (`iotdata_decode_to_json` and
`iotdata_encode_from_json`) with the following canonical mapping.

The JSON field names are derived from the variant's field labels, so the same
binary encoding may produce different JSON keys depending on variant. For
example, a default weather station variant produces `"wind"` as a bundled JSON
object, while a custom variant using individual wind fields produces separate
`"wind_speed"`, `"wind_direction"`, and `"wind_gust"` keys. Similarly, the
`"depth"` field type may produce `"snow_depth"`, `"soil_depth"`, or any other
label depending on the variant definition.

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

TLV entries are represented as an array under `"data"`. Each entry contains
`"type"`, `"format"`, and `"data"` fields.

The `"type"` field is the numeric TLV type identifier. The `"format"` field
indicates how the `"data"` field should be interpreted.

#### Globally Defined Types

TLV types that have a defined JSON representation (Section 9.5) are destructured
by the gateway into structured objects or decoded strings. The `"format"` field
reflects the structured type rather than the wire encoding:

| Type | Format      | `data` contains                                 |
| ---- | ----------- | ----------------------------------------------- |
| 0x01 | `"version"` | Structured object: firmware/hardware key-values |
| 0x02 | `"status"`  | Structured object: uptimes, restarts, reason    |
| 0x03 | `"health"`  | Structured object: temp, voltage, heap, active  |
| 0x04 | `"config"`  | Structured object: key-value pairs              |
| 0x05 | `"string"`  | Decoded diagnostic text string                  |
| 0x06 | `"string"`  | Decoded userdata text string                    |

Example with all global types:

```json
{
  "data": [
    {
      "type": 1,
      "format": "version",
      "data": {
        "FW": "142",
        "HW": "3"
      }
    },
    {
      "type": 2,
      "format": "status",
      "data": {
        "session_uptime": 86400,
        "lifetime_uptime": 1209600,
        "restarts": 12,
        "reason": "watchdog"
      }
    },
    {
      "type": 3,
      "format": "health",
      "data": {
        "cpu_temp": 34,
        "supply_mv": 3842,
        "free_heap": 42816,
        "session_active": 1050
      }
    },
    {
      "type": 4,
      "format": "config",
      "data": {
        "TX": "30",
        "SF": "7",
        "PW": "14"
      }
    },
    { "type": 5, "format": "string", "data": "LOW SIGNAL" },
    { "type": 6, "format": "string", "data": "BTN A" }
  ]
}
```

Note that a single packet would not typically contain all of these. A normal
transmission might include only sensor data fields with no TLV entries at all,
or one or two TLV entries such as Status and a Diagnostic message. Packets may
also contain repeated entries, for example, multiple Diagnostic or Userdata
TLVs.

#### Unrecognised and Proprietary Types

TLV types that do not have a defined JSON representation — including proprietary
types (0x20+), reserved types, and any type the gateway does not recognise —
fall back to a generic encoding based on the wire format bit:

| Wire format | `"format"` | `"data"` contains          |
| ----------- | ---------- | -------------------------- |
| raw (0)     | `"raw"`    | Base64-encoded byte string |
| string (1)  | `"string"` | Decoded text string        |

Examples:

```json
{
  "data": [
    { "type": 32, "format": "raw", "data": "A0b4901=" },
    { "type": 33, "format": "string", "data": "HELLO WORLD" }
  ]
}
```

This ensures that all TLV entries are representable in JSON even if the gateway
has no knowledge of the type's semantics. The raw Base64 or decoded string is
passed through for downstream consumers to interpret.

#### Format Field Summary

The `"format"` field serves as a discriminator for how to parse the `"data"`
field. The complete set of values:

| Value       | `data` type     | Source                                  |
| ----------- | --------------- | --------------------------------------- |
| `"raw"`     | string (Base64) | Fallback for unrecognised raw TLV types |
| `"string"`  | string (text)   | String-format TLVs (wire or defined)    |
| `"version"` | object          | Version TLV (0x01)                      |
| `"status"`  | object          | Status TLV (0x02)                       |
| `"health"`  | object          | Health TLV (0x03)                       |
| `"config"`  | object          | Config TLV (0x04)                       |

Note that `"string"` appears both as the defined format for Diagnostic (0x05)
and Userdata (0x06), and as the fallback for unrecognised string-format TLVs.
This is intentional — the representation is identical in both cases (a plain
text string), so no distinction is needed.

### Round-Trip Guarantee

The JSON representation MUST support lossless round-trip conversion: encoding a
packet to binary, decoding to JSON, re-encoding from JSON, and comparing the
resulting binary MUST produce an identical byte sequence. The reference
implementation test suite verifies this property.

## 11. Receiver Considerations

This section describes algorithms and considerations that apply to the receiving
side (gateway, server, or any device decoding packets).

### 11.1. Datetime Year Resolution

The datetime field encodes seconds from the start of the current year but does
not transmit the year. The receiver MUST resolve the year using the following
algorithm:

1. Let `T_rx` be the receiver's current UTC time.
2. Let `Y` be the year component of `T_rx`.
3. Decode the datetime field to obtain `S` seconds from year start.
4. Compute `T_decoded = Y-01-01T00:00:00Z + S seconds`.
5. If `T_decoded` is more than 6 months in the future relative to `T_rx`,
   subtract one year: `T_decoded = (Y-1)-01-01T00:00:00Z + S`.

This handles the year boundary: a packet timestamped December 31 and received
January 1 is correctly attributed to the previous year.

The 24-bit field at 5-second resolution supports approximately 971 days, so the
encoding does not wrap within a single year.

The accuracy of the decoded timestamp depends on the accuracy of the
transmitter's time source. For GNSS-synchronised devices this is typically
sub-second; for free-running RTC devices it may drift by seconds per day. See
Section 11.3.

### 11.2. Position Source Ambiguity

The position field (Section 8.7) encodes latitude and longitude without
indicating the source. The practical accuracy varies significantly by source:

- **GNSS (GPS/Galileo/GLONASS):** typically 2-5 metre accuracy, well within the
  ~1.2m quantisation of the 24-bit encoding.

- **WiFi geolocation:** typically 15-50 metre accuracy. The quantisation error
  is negligible relative to the source error.

- **Cell tower:** typically 100-1000 metre accuracy.

- **Static configuration:** the operator programmes the known coordinates at
  deployment time. Accuracy depends on the method used (surveyed, map click,
  etc.).

In a closed system where the operator controls all devices and knows their
position sources, this ambiguity is acceptable. For open or interoperable
systems, the source and accuracy SHOULD be communicated via a sensor metadata
TLV (Section 11.3).

Similarly, for fixed-position sensors, the position field is typically
transmitted once at startup or periodically at a low rate, not on every packet.
The receiver SHOULD cache the last known position for a station and associate it
with subsequent packets that omit position.

### 11.3. Sensor Metadata and Interoperability

The core protocol deliberately omits sensor metadata such as:

- Sensor type (NTC thermistor, BME280, SHT40, etc.)
- Measurement accuracy or precision class
- Position source (GNSS, static config, etc.)
- Time source (GNSS, NTP, free-running RTC, etc.)
- Calibration date or coefficients

In a **closed system** — where one operator controls all devices and the gateway
software — this information is known out-of-band. The operator knows that
station 42 uses a BME280 for environment readings with ±1°C accuracy, has a
static position programmed at deployment, and synchronises time via NTP. No wire
overhead is needed.

For **interoperable systems** — where devices from different manufacturers or
deployments share a common receiver — sensor metadata becomes important. The
protocol reserves TLV types 0x10-0x1F for future standardised metadata TLVs that
could convey:

- Source type per field (e.g. "position source = GNSS" or "temperature sensor =
  BME280")
- Accuracy class or error bounds
- Calibration metadata

The design of these metadata TLVs is deferred to a future revision of this
specification. Implementers requiring interoperability before that revision MAY
use application-defined TLV types (0x20-) for this purpose, with the
understanding that these are not standardised.

This approach follows a deliberate design philosophy: add wire overhead only
when it is needed. A snow depth sensor transmitting to its own gateway every 15
minutes on a coin cell battery should not pay the cost of metadata bytes that
the receiver already knows.

### 11.4. Unknown Variants

A receiver encountering a variant number that it does not have a table entry for
SHOULD:

1. Fall back to variant 0's field mapping for decoding.
2. Flag the packet as using an unknown variant in its output (e.g. a warning in
   the print output or a field in the JSON).
3. NOT reject the packet, since the field encodings are universal and the data
   is likely still meaningful.

In the reference implementation, `iotdata_get_variant()` returns variant 0's
table as a fallback for any unknown variant number.

### 11.5. Quantisation Error Budgets

Receivers should be aware of the quantisation errors inherent in the encoding.
These are systematic and deterministic — not noise — and SHOULD be accounted for
in any downstream processing.

| Field            | Bits | Range           | Resolution   | Max quant error |
| ---------------- | ---- | --------------- | ------------ | --------------- |
| Battery level    | 5    | 0-100%          | ~3.23%       | ±1.6%           |
| Link RSSI        | 4    | -120 to -60 dBm | 4 dBm        | ±2 dBm          |
| Link SNR         | 2    | -20 to +10 dB   | 10 dB        | ±5 dB           |
| Temperature      | 9    | -40 to +80°C    | 0.25°C       | ±0.125°C        |
| Pressure         | 8    | 850-1105 hPa    | 1 hPa        | ±0.5 hPa        |
| Humidity         | 7    | 0-100%          | 1%           | ±0.5%           |
| Wind speed       | 7    | 0-63.5 m/s      | 0.5 m/s      | ±0.25 m/s       |
| Wind direction   | 8    | 0-355°          | ~1.41°       | ±0.7°           |
| Wind gust        | 7    | 0-63.5 m/s      | 0.5 m/s      | ±0.25 m/s       |
| Rain rate        | 8    | 0-255 mm/hr     | 1 mm/hr      | ±0.5 mm/hr      |
| Rain size        | 4    | 0-6.0 mm/d      | 0.25 mm/d    | ±0.5 mm/d       |
| Solar Irradiance | 10   | 0-1023 W/m²     | 1 W/m²       | ±0.5 W/m²       |
| Solar UV Index   | 4    | 0-15            | 1            | ±0.5            |
| Clouds           | 4    | 0-8 okta        | 1 okta       | ±0.5 okta       |
| AQ Index         | 9    | 0-500 AQI       | 1 AQI        | ±0.5 AQI        |
| AQ PM channels   | 8    | 0-1275 µg/m³    | 5 µg/m³      | ±2.5 µg/m³      |
| AQ Gas VOC idx   | 8    | 0-510           | 2 idx pts    | ±1 idx pt       |
| AQ Gas NOx idx   | 8    | 0-510           | 2 idx pts    | ±1 idx pt       |
| AQ Gas CO₂       | 10   | 0-51,150 ppm    | 50 ppm       | ±25 ppm         |
| AQ Gas CO        | 10   | 0-1,023 ppm     | 1 ppm        | ±0.5 ppm        |
| AQ Gas HCHO      | 10   | 0-5,115 ppb     | 5 ppb        | ±2.5 ppb        |
| AQ Gas O₃        | 10   | 0-1,023 ppb     | 1 ppb        | ±0.5 ppb        |
| Radiation CPM    | 16   | 0-65535 CPM     | 1 CPM        | ±0.5 CPM        |
| Radiation dose   | 14   | 0-163.83 µSv/h  | 0.01 µSv/h   | ±0.005 µSv/h    |
| Depth            | 10   | 0-1023 cm       | 1 cm         | ±0.5 cm         |
| Latitude         | 24   | -90° to +90°    | ~0.00001073° | ~0.6 m          |
| Longitude        | 24   | -180° to +180°  | ~0.00002146° | ~1.2 m (eq)     |
| Datetime         | 24   | 0-83.9M seconds | 5 s          | ±2.5 s          |

These quantisation errors are generally smaller than the measurement uncertainty
of the sensors themselves. For example, a typical BME280 temperature sensor has
±1°C accuracy, well above the 0.125°C quantisation error.

## 12. Packet Size Reference

The following table shows exact bit and byte counts for common packet
configurations, using variant 0 (weather_station).

| Scenario                          | Fields                                  | Bits | Bytes |
| --------------------------------- | --------------------------------------- | ---- | ----- |
| Heartbeat (no data)               | header + pres0                          | 40   | 5     |
| Minimal (battery only)            | + battery                               | 46   | 6     |
| Battery + environment             | + battery, environment                  | 70   | 9     |
| Typical pres0 (bat+env+wind+rain) | + battery, environment, wind, rain_rate | 104  | 13    |
| Full pres0 (all 6 fields)         | + battery, env, wind, rain, solar, link | 124  | 16    |
| Full station (all 12 fields)      | + all 12 field types (pres0 + pres1)    | 253  | 32    |

For comparison, the equivalent data in JSON would typically be 200-600 bytes,
and in a packed C struct with byte alignment would be 40-60 bytes.

## 13. Implementation Notes

### 13.1. Reference Implementation

The reference implementation (`libiotdata`) is written in C11 and targets both
embedded systems (ESP32-C3, STM32, nRF52, Raspberry Pi) and Linux
gateways/servers. It consists of:

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

Dependencies: C11 compiler, `libm`, and `cJSON` (optional, only required for
JSON serialisation).

### 13.2. Encoder Strategy

The encoder uses a "store then pack" strategy:

1. `iotdata_encode_begin()` initialises the context with header values and a
   buffer pointer.
2. `iotdata_encode_battery()`, `iotdata_encode_environment()`, etc. validate
   inputs and store native-typed values in the context. Fields may be added in
   any order.
3. `iotdata_encode_end()` performs all bit-packing in a single pass, consulting
   the variant field table to determine field order and presence byte layout.

This separation means that the encoder validates eagerly (at the `encode_*()`
call site where the developer can handle the error) and packs lazily (in one
pass, knowing the complete field set).

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

The library supports extensive compile-time configuration to minimise code size
and memory usage on constrained targets.

**Variant selection:**

| Define                           | Effect                                  |
| -------------------------------- | --------------------------------------- |
| `IOTDATA_VARIANT_MAPS_DEFAULT`   | Enable built-in weather station variant |
| `IOTDATA_VARIANT_MAPS=<sym>`     | Use custom variant map array            |
| `IOTDATA_VARIANT_MAPS_COUNT=<n>` | Number of entries in custom map         |

**Field support compilation:**

| Define                          | Effect                                         |
| ------------------------------- | ---------------------------------------------- |
| `IOTDATA_ENABLE_SELECTIVE`      | Only compile elements explicitly enabled below |
| `IOTDATA_ENABLE_BATTERY`        | Compile battery field                          |
| `IOTDATA_ENABLE_LINK`           | Compile link field                             |
| `IOTDATA_ENABLE_ENVIRONMENT`    | Compile environment bundle                     |
| `IOTDATA_ENABLE_TEMPERATURE`    | Compile temperature field                      |
| `IOTDATA_ENABLE_PRESSURE`       | Compile pressure field                         |
| `IOTDATA_ENABLE_HUMIDITY`       | Compile humidity field                         |
| `IOTDATA_ENABLE_WIND`           | Compile wind bundle                            |
| `IOTDATA_ENABLE_WIND_SPEED`     | Compile wind speed field                       |
| `IOTDATA_ENABLE_WIND_DIR`       | Compile wind direction field                   |
| `IOTDATA_ENABLE_WIND_GUST`      | Compile wind gust field                        |
| `IOTDATA_ENABLE_RAIN`           | Compile rain bundle                            |
| `IOTDATA_ENABLE_RAIN_RATE`      | Compile rain rate field                        |
| `IOTDATA_ENABLE_RAIN_SIZE`      | Compile rain size field                        |
| `IOTDATA_ENABLE_SOLAR`          | Compile solar field                            |
| `IOTDATA_ENABLE_CLOUDS`         | Compile clouds field                           |
| `IOTDATA_ENABLE_AIR_QUALITY`    | Compile air quality field                      |
| `IOTDATA_ENABLE_RADIATION`      | Compile radiation bundle                       |
| `IOTDATA_ENABLE_RADIATION_CPM`  | Compile radiation CPM field                    |
| `IOTDATA_ENABLE_RADIATION_DOSE` | Compile radiation dose field                   |
| `IOTDATA_ENABLE_DEPTH`          | Compile depth field                            |
| `IOTDATA_ENABLE_POSITION`       | Compile position field                         |
| `IOTDATA_ENABLE_DATETIME`       | Compile datetime field                         |
| `IOTDATA_ENABLE_FLAGS`          | Compile flags field                            |
| `IOTDATA_ENABLE_TLV`            | Compile TLV fields                             |

When `IOTDATA_ENABLE_SELECTIVE` is defined, only the element types with their
corresponding `IOTDATA_ENABLE_xxx` defined will be compiled. When
`IOTDATA_VARIANT_MAPS_DEFAULT` is defined (without `IOTDATA_ENABLE_SELECTIVE`),
all elements used by the default weather station variant are automatically
enabled. When neither is defined, all elements are compiled.

In particular, avoidance of the TLV element will save considerable footprint.

**Functional subsetting:**

| Define                     | Effect                                                           |
| -------------------------- | ---------------------------------------------------------------- |
| `IOTDATA_NO_DECODE`        | Exclude decoder functions (also excludes print and JSON encoder) |
| `IOTDATA_NO_ENCODE`        | Exclude encoder functions (also excludes JSON decoder)           |
| `IOTDATA_NO_PRINT`         | Exclude print functions                                          |
| `IOTDATA_NO_DUMP`          | Exclude dump functions                                           |
| `IOTDATA_NO_JSON`          | Exclude JSON functions                                           |
| `IOTDATA_NO_TLV_SPECIFIC`  | Exclude TLV specific type handling                               |
| `IOTDATA_NO_CHECKS_STATE`  | Exclude state checking logic                                     |
| `IOTDATA_NO_CHECKS_TYPES`  | Exclude type checking logic                                      |
| `IOTDATA_NO_ERROR_STRINGS` | Exclude error strings (and iotdata_strerror)                     |

These allow building an encoder-only image for a sensor node (smallest possible
footprint) or a decoder-only image for a gateway.

The JSON encoding functions have a dependency on the decoder (decode from wire
format and encode into JSON), and JSON decoding functions equivalently are
dependent on the encoder (decode from JSON and encode into wire format). The
print functions, for brevity, are also dependant upon the decoder. The dump
functions work directly upon the the wire format buffer are are not dependent on
either the encoder or decoder.

Be aware that `IOTDATA_NO_CHECKS_STATE` will cease verification of non null
`iotdata_encoder_t*` and the ordering of the encoding calls (i.e. that `begin`
must be first, followed by individual `encode_` functions before a final `end`.
This is moderately safe, and acceptable to turn on during development and off
for production. It will also turn off null checks for buffers passed into the
`dump`, `print` and `json` function.s

`IOTDATA_NO_CHECKS_TYPES` will cease verification of type boundaries on calls to
`encode_` functions, for example that temperatures passed are between
quantisable minimum and maximum values. This is less safe, but results only in
bad data (and badly quantised data) passed over the wire: this may fail to
interpret bad data obtained from sensors. This option will turn off length
checking in TLV encoded strings (and worst case, truncate them) as well as TLV
encoded string validity (and worst case, transmit these as spaces).

Unless there are considerable space constraints, such as on Class 1
microcontrollers (Appendix E), it is not recommended to engage either of the
`NO_CHECKS` options.

**Floating-point control:**

| Define                        | Effect                                           |
| ----------------------------- | ------------------------------------------------ |
| (default)                     | `double` for position, `float` for other fields  |
| `IOTDATA_NO_FLOATING_DOUBLES` | Use `float` instead of `double` everywhere       |
| `IOTDATA_NO_FLOATING`         | Integer-only mode: all values as scaled integers |

In integer-only mode (`IOTDATA_NO_FLOATING`), temperature is passed as
degrees×100 (e.g. 2250 for 22.50°C), wind speed as m/s×100, radiation dose as
µSv/h×100, position as degrees×10^7, and SNR as dB×10. This eliminates all
floating-point dependencies. Future implementations SHOULD utilise this
multiple-of-ten approach.

#### Test targets

The `test-versions` target will build each of versions across the Functional
subsetting and Floating-point control, including a combined `NO_JSON` and
`NO_FLOATING` version. This is intended as a build smoke test to verify
compilation control paths. Note that the combined version, will on x86
platforms, force the compiler to reject floating-point operations, so as to
ensure they are not latent in the implementation.

### 13.4. Build Size and Stack Usage

The `minimal` and `minimal-esp32` targets yield object files for the purpose of
establishing minimal build sizes (with a comparison to full build sizes) using
the host (`minimal`) or cross-compiler (`minimal-esp32`) tools.

#### Build summary for x86-64, aarch64 and esp32-c3 systems

The following measurements are from GCC on x86-64, aarch64 and ESP32-C3 using
the `minimal` build target. With space optimisation, the minimal implementation
is less than 1KB on the embedded target.

| Configuration                                       | x86-64 -O6 | x86-64 -Os | aarch64 -O6 | aarch64 -Os | esp32-c3 -O6 | esp32-c3 -Os |
| --------------------------------------------------- | ---------- | ---------- | ----------- | ----------- | ------------ | ------------ |
| Full library (all elements, encode + decode + JSON) | ~85 KB     | ~29 KB     | ~87 KB      | ~31 KB      | ~67 KB       | ~19 KB       |
| Encoder-only, battery + environment only            | ~5.5 KB    | ~1.1 KB    | ~5.4 KB     | ~1.1 KB     | ~5.0 KB      | ~0.7 KB      |

#### Build output for x86-64 (-O6 and -Os)

```text
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

```text
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

#### Build output for esp32-c3 (-Os)

```text
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

#### Stack usage for x86-64 (-Os)

The `test-example` target compiled with `gcc -fstack-usage -Os` on x86-64
illustrates per-function stack frames; nested calls accumulate.

| Function                   | Stack (bytes) | Notes                          |
| -------------------------- | ------------- | ------------------------------ |
| `iotdata_dump_to_string`   | 5872          | `iotdata_dump_t` on stack      |
| `iotdata_dump_to_file`     | 5872          | `iotdata_dump_t` on stack      |
| `iotdata_decode_to_json`   | 2768          | `iotdata_decoded_t` + cJSON    |
| `iotdata_print_to_string`  | 2224          | `iotdata_decoded_t` on stack   |
| `iotdata_print_to_file`    | 2208          | `iotdata_decoded_t` on stack   |
| `iotdata_encode_from_json` | 416           | Encoder context + JSON parsing |
| `iotdata_dump_build`       | 192           | Dynamic, bounded               |
| `iotdata_encode_begin`     | < 64          | —                              |
| `iotdata_encode_end`       | < 64          | —                              |
| `iotdata_decode`           | < 128         | —                              |

The dump and print functions dominate because they allocate `iotdata_dump_t`
(5.8 KB) or `iotdata_decoded_t` (2.2 KB) on the stack. These are
gateway/diagnostic functions not intended for constrained devices. The macro
`IOTDATA_MAX_DUMP_ENTRIES` defaults to 48, and may be reduced to tune down this
size. Note that `iotdata_decoded_t` contains a complete set of all decoded
variables, which in this example is the weather station variant with expensive
TLV entries.

The encode path — `encode_begin`, field calls, `encode_end` — peaks well under
500 bytes total stack depth. On a Class 2 device with 20 KB RAM, this leaves
ample room for the RTOS stack, radio driver, and application logic.

To reduce stack usage on memory-constrained targets, allocate `iotdata_dump_t`
or `iotdata_decoded_t` as a static or global rather than calling the convenience
wrappers which declare them locally.

### 13.5. Variant Table Extension

The variant table in the reference implementation is a compile-time array.
Adding a new variant requires defining a `variant_def_t` entry with the desired
field mapping and compiling with the `IOTDATA_VARIANT_MAPS` and
`IOTDATA_VARIANT_MAPS_COUNT` defines. No changes to the encoder or decoder logic
are needed — the dispatch mechanism automatically handles any valid variant
table entry.

If a new encoding type is needed (not just a relabelling of an existing type),
the implementer must:

1. Add a new `IOTDATA_FIELD_*` enum value.
2. Implement the six per-field functions (pack, unpack, json_add, json_read,
   dump, print).
3. Add a case to each of the six dispatcher functions.
4. Add the appropriate constants and quantisation helpers.

## 14. Security Considerations

This protocol provides no confidentiality, integrity, or authentication
mechanisms at the packet level. It is designed for environments where these
properties are provided at other layers:

- **LoRaWAN** provides AES-128 encryption and message integrity checks at the
  MAC layer.

- **TLS/DTLS** may be used for IP-based transports.

- **Physical security** may be sufficient for isolated deployments on private
  land.

Specific risks to consider:

- **Replay attacks:** An attacker could retransmit captured packets. The
  sequence number provides detection (not prevention) of replayed packets, but
  only if the receiver tracks per-station sequence state.

- **Spoofing:** Station IDs are not authenticated. An attacker within radio
  range could transmit packets with a forged station ID.

- **Eavesdropping:** The wire format is not encrypted. Sensor readings
  (temperature, position, etc.) are transmitted in the clear.

Deployments with security requirements MUST use an appropriate underlying
transport that provides the needed properties.

## 15. Future Work

The following items are identified for future revisions:

1. **Sensor metadata TLVs (types 0x10-0x1F):** Standardised TLV formats for
   conveying sensor type, accuracy class, time source, position source, and
   calibration metadata. This would enable interoperability between devices from
   different manufacturers or deployments without prior out-of-band knowledge.

2. **Quality indicator fields:** Per-field quality/confidence indicators (e.g.
   GNSS fix quality, HDOP, number of satellites). These would likely use the
   reserved TLV type range.

3. **Extended header (variant 15):** A future header format with more variant
   bits, larger station ID space, or additional structural fields.

4. **Implementation singularity limitation:** The wire format supports multiple
   instances of the same field type in different slots (e.g. two independent
   temperature readings). The current reference implementation uses fixed named
   storage in the encoder/decoder structs, limiting each field type to one
   instance. A future implementation could decouple field type from field
   storage, allowing the variant map to bind each slot to an independent storage
   location.

---

## Appendix A. 6-Bit Character Table

The packed string format (TLV Format = 1) encodes each character as 6 bits using
the following table:

| Value | Char  | Value | Char | Value | Char | Value | Char   |
| ----- | ----- | ----- | ---- | ----- | ---- | ----- | ------ |
| 0     | space | 16    | p    | 32    | 5    | 48    | L      |
| 1     | a     | 17    | q    | 33    | 6    | 49    | M      |
| 2     | b     | 18    | r    | 34    | 7    | 50    | N      |
| 3     | c     | 19    | s    | 35    | 8    | 51    | O      |
| 4     | d     | 20    | t    | 36    | 9    | 52    | P      |
| 5     | e     | 21    | u    | 37    | A    | 53    | Q      |
| 6     | f     | 22    | v    | 38    | B    | 54    | R      |
| 7     | g     | 23    | w    | 39    | C    | 55    | S      |
| 8     | h     | 24    | x    | 40    | D    | 56    | T      |
| 9     | i     | 25    | y    | 41    | E    | 57    | U      |
| 10    | j     | 26    | z    | 42    | F    | 58    | V      |
| 11    | k     | 27    | 0    | 43    | G    | 59    | W      |
| 12    | l     | 28    | 1    | 44    | H    | 60    | X      |
| 13    | m     | 29    | 2    | 45    | I    | 61    | Y      |
| 14    | n     | 30    | 3    | 46    | J    | 62    | Z      |
| 15    | o     | 31    | 4    | 47    | K    | 63    | (rsvd) |

Value 63 is reserved for a future escape mechanism to extend the character set.

The corresponding encode/decode functions in the reference implementation:

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

```text
q = round(75 / 100.0 * 31.0) = round(23.25) = 23
Decoded: round(23 / 31.0 * 100.0) = round(74.19) = 74%
Error: 1 percentage point
```

### B.2. Temperature

Input: -15.25°C

```text
q = round((-15.25 - (-40.0)) / 0.25) = round(24.75 / 0.25) = round(99.0) = 99
Decoded: -40.0 + 99 * 0.25 = -40.0 + 24.75 = -15.25°C
Error: 0.00°C (exact)
```

### B.3. Position (59.334591°N, 18.063240°E)

Latitude:

```text
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

```text
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

```text
ticks = 475200 / 5 = 95040
Decoded: 95040 * 5 = 475200 seconds
Error: 0 seconds (exact, since input is a multiple of 5)
```

Input: Day 5, 12:00:03 (475,203 seconds — not a multiple of 5)

```text
ticks = 475203 / 5 = 95040 (integer division, truncated)
Decoded: 95040 * 5 = 475200 seconds
Error: 3 seconds (truncation towards zero)
```

Note: the encoder uses integer division (truncation), not rounding, for the
datetime field. This means the decoded time is always ≤ the actual time, with a
maximum error of 4 seconds.

## Appendix C. Complete Encoder Example

The following example from the reference implementation test suite demonstrates
encoding a full weather station telemetry packet:

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
    iotdata_encode_air_quality_index(&enc, 75);
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

The iotdata protocol is designed so that a useful telemetry packet typically
fits within a single link-layer frame on the target medium. Fragmenting a packet
across multiple frames defeats the core design goals: each additional frame
incurs a separate preamble, MAC header, and — critically on duty-cycle-regulated
media — a separate transmission window. On LoRa at SF12, a single 24-byte frame
takes approximately 1.5 seconds to transmit; two frames would take over 3
seconds, consume twice the energy, and halve the effective reporting rate under
duty cycle constraints.

Implementers SHOULD select fields per packet to remain within the target
medium's payload limit. The presence flag mechanism makes this straightforward:
each packet is self-describing, so the receiver correctly handles any
combination of fields without prior negotiation.

### D.2. LoRa (Raw PHY)

LoRa is the primary target medium. At 125 kHz bandwidth with coding rate 4/5 and
explicit header, the time-on-air for representative iotdata packet sizes is:

| Packet            | Bytes | SF7   | SF8    | SF9    | SF10   | SF11   | SF12   |
| ----------------- | ----- | ----- | ------ | ------ | ------ | ------ | ------ |
| Minimal (battery) | 6     | 36 ms | 62 ms  | 124 ms | 248 ms | 496 ms | 991 ms |
| Typical (b+e+d)   | 10    | 41 ms | 72 ms  | 144 ms | 289 ms | 578 ms | 991 ms |
| With link+flags   | 12    | 41 ms | 82 ms  | 144 ms | 289 ms | 578 ms | 1.16 s |
| Full telemetry    | 24    | 62 ms | 113 ms | 206 ms | 371 ms | 823 ms | 1.48 s |

(Computed using the Semtech AN1200.13 formula with 8-symbol preamble and low
data rate optimisation enabled for SF11/SF12.)

All iotdata packets (6-24 bytes) fit well within the raw LoRa PHY payload limit
of 255 bytes at any spreading factor. The binding constraint is not payload size
but **time-on-air and duty cycle**.

In the EU868 ISM band, the regulatory duty cycle limit is typically 1%. This
means a device must remain silent for 99× the transmission duration after each
packet. The implications are significant:

| Packet         | Bytes | SF7       | SF12          |
| -------------- | ----- | --------- | ------------- |
| Minimal        | 6     | 1 per 4 s | 1 per 1.7 min |
| Typical        | 10    | 1 per 4 s | 1 per 1.7 min |
| Full telemetry | 24    | 1 per 6 s | 1 per 2.5 min |

The difference between a 10-byte typical packet and a 24-byte full packet at
SF12 is the difference between transmitting every 1.7 minutes and every 2.5
minutes — or equivalently, ~35 vs ~24 transmissions per hour. This means that
for bit-packing: the savings are not merely aesthetic, they directly translate
to reporting frequency, battery life, or both.

**Spreading factor selection** is an implementation decision that balances range
against airtime. SF7 provides the shortest airtime but the least range; SF12
provides maximum range (approximately 10-15 km line of sight) at the cost of 32×
the airtime. The iotdata protocol is agnostic to spreading factor — the same
packet is valid regardless of the underlying modulation parameters.

### D.3. LoRaWAN

LoRaWAN adds a MAC layer on top of LoRa, providing device management, adaptive
data rate (ADR), and AES-128 encryption with message integrity. The MAC overhead
consumes approximately 13 bytes of the LoRa PHY payload (MHDR, DevAddr, FCtrl,
FCnt, FPort, MIC), reducing the available application payload.

The maximum LoRaWAN application payload by data rate (EU868):

| Data Rate | Modulation | Max payload | Full iotdata (24B) | Headroom |
| --------- | ---------- | ----------- | ------------------ | -------: |
| DR0       | SF12/125   | 51 bytes    | fits               |     27 B |
| DR1       | SF11/125   | 51 bytes    | fits               |     27 B |
| DR2       | SF10/125   | 51 bytes    | fits               |     27 B |
| DR3       | SF9/125    | 115 bytes   | fits               |     91 B |
| DR4       | SF8/125    | 222 bytes   | fits               |    198 B |
| DR5       | SF7/125    | 222 bytes   | fits               |    198 B |

Full iotdata telemetry (24 bytes) fits comfortably at all LoRaWAN data rates,
with at least 27 bytes of headroom even at the lowest data rate.

Note that the AWS LoRaWAN documentation identifies 11 bytes as the safe
universal application payload across all global frequency plans and data rates.
The iotdata protocol's typical packet (battery + environment + depth) is 10
bytes, falling within this universal limit.

LoRaWAN's AES-128 encryption and MIC address the security considerations
discussed in Section 14. Deployments using LoRaWAN inherit these protections
without any additional work at the iotdata protocol layer.

### D.4. Sigfox

Sigfox imposes the tightest constraints of any common LPWAN medium: a maximum
uplink payload of **12 bytes** and a limit of **140 messages per day**
(approximately one every 10 minutes for uniform distribution).

| Packet configuration    | Bytes | Fits Sigfox? |
| ----------------------- | ----- | :----------: |
| Minimal (battery)       | 6     |      ✓       |
| Typical (bat+env+depth) | 10    |      ✓       |
| With link+flags         | 12    |  ✓ (exact)   |
| With position           | 19    |      ✗       |
| Full telemetry          | 24    |      ✗       |

The protocol's core telemetry packets fit within the 12-byte Sigfox limit.
Position and datetime, which require the extension byte and add 6-9 bytes, do
not fit alongside a full complement of sensor fields.

For Sigfox deployments, implementers SHOULD use a **field rotation strategy**:
transmit core telemetry (battery, environment, depth) on every message, and
rotate in less-frequently-needed fields across separate messages. For example:

- Every 10 minutes: battery + environment + depth (10 bytes)
- Once per hour: battery + position (12 bytes)
- Once per day: battery + datetime + flags (10 bytes)

The presence flag mechanism supports this natively — each packet is
self-describing, so the receiver assembles a complete picture from multiple
packets without any out-of-band configuration.

Sigfox provides its own authentication and anti-replay mechanisms at the network
level, but does not encrypt the payload. Implementers requiring payload
confidentiality on Sigfox must implement application-layer encryption within the
12-byte constraint.

### D.5. IEEE 802.11ah (Wi-Fi HaLow)

IEEE 802.11ah operates in the sub-GHz ISM bands (typically 868 MHz in Europe,
902-928 MHz in the US) and targets IoT applications with range up to 1 km.
Unlike LoRa and Sigfox, it is IP-based and supports standard Ethernet-class
MSDUs (up to 1500 bytes payload per frame), with A-MPDU aggregation for larger
transfers.

Packet size is not a meaningful constraint for iotdata on 802.11ah. However, the
efficiency argument still applies:

- **Power consumption** scales with transmission duration. 802.11ah introduced a
  reduced MAC header (18 bytes vs 28 bytes in legacy 802.11) specifically to
  reduce overhead for small IoT payloads. A 10-byte iotdata payload benefits
  from this optimisation more than a 200-byte JSON payload would.

- **EU duty cycle** regulations apply to the sub-GHz bands used by 802.11ah,
  though the specific constraints differ from LoRa (802.11ah typically uses
  listen-before-talk rather than pure duty cycle limits).

- **Contention** in dense deployments is reduced by shorter frame durations,
  improving effective throughput for all stations.

The iotdata payload would typically be carried as a UDP datagram within the
802.11ah frame. The receiver-side JSON conversion is well suited to 802.11ah
gateways, which have IP connectivity and typically run on more capable hardware.

### D.6. Cellular (NB-IoT, LTE-M, SMS)

Cellular technologies provide reliable, wide-area connectivity with
operator-managed infrastructure. Three cellular transports are relevant for
iotdata:

**NB-IoT and LTE-M** are purpose-built cellular IoT standards. NB-IoT supports
payloads of approximately 1600 bytes per message; LTE-M supports standard IP MTU
sizes. Payload size is not a constraint. Both provide encryption, integrity, and
authentication at the network layer, fully addressing the security
considerations of Section 14.

**SMS** is a widely overlooked but practical transport for low-rate telemetry.
The GSM 03.40 specification defines an 8-bit binary encoding mode (selected via
the TP-DCS field) that provides **140 bytes** of raw octet payload per message —
more than enough for any iotdata packet. Binary SMS is sent via AT commands in
PDU mode, which every GSM/3G/4G modem supports (SIM800, u-blox SARA, Quectel,
etc.).

| Cellular transport | Max payload | Full iotdata | IP stack needed |
| ------------------ | ----------- | :----------: | :-------------: |
| NB-IoT             | ~1600 B     |      ✓       |       Yes       |
| LTE-M              | ~MTU        |      ✓       |       Yes       |
| SMS (8-bit binary) | 140 B       |      ✓       |       No        |

SMS has several properties that distinguish it from IP-based cellular
transports:

- **Near-universal coverage.** SMS operates on the GSM signalling channel and
  works in 2G-only areas where NB-IoT or LTE-M may not be deployed.

- **Store-and-forward.** The SMSC holds messages if the receiver is temporarily
  unreachable, providing inherent buffering that IP-based transports must
  implement at the application layer.

- **No IP stack.** The sensor MCU needs only a UART connection to a GSM modem
  and a handful of AT commands (`AT+CMGS` in PDU mode). This significantly
  reduces firmware complexity compared to a full IP/CoAP/DTLS stack.

- **No data plan.** SMS-only SIM plans are available at low cost, avoiding the
  complexity of cellular data provisioning.

- **Fallback resilience.** SMS uses the control plane rather than the data
  plane, so it typically remains functional during network congestion that would
  affect data services.

The primary disadvantages of SMS are per-message cost (unlike LoRa which is
free, or bulk-metered data plans), latency (typically 1-30 seconds, occasionally
longer during congestion), and receiving-side complexity (the gateway requires
either a GSM modem or an SMS-to-HTTP gateway service). SMS provides no payload
encryption; content is visible to the carrier network.

The bit-packing efficiency of iotdata remains beneficial across all cellular
transports for two reasons:

1. **Energy per byte.** Cellular radio transmission energy is roughly
   proportional to transmission time. Shorter payloads mean shorter active radio
   periods and longer battery life.

2. **Data and message cost.** For IP-based transports, reducing payload from 200
   bytes (JSON) to 10 bytes (iotdata) reduces per-message data consumption by
   95%. For SMS, keeping packets within a single 140-byte message avoids the
   overhead and cost of concatenated multi-part SMS.

SMS is particularly well suited as a **fallback transport**: a sensor that
normally transmits via LoRa could fall back to SMS when connectivity is lost or
when an alarm condition requires guaranteed delivery via a different path. The
same encoded packet is valid on both transports without modification.

### D.7. Medium Selection Summary

| Medium   | Max payload | Full iotdata | Primary constraint      | Encryption | Notes                         |
| -------- | ----------- | :----------: | ----------------------- | :--------: | ----------------------------- |
| LoRa     | 255 B       |      ✓       | Duty cycle / airtime    |     No     | Primary target medium         |
| LoRaWAN  | 51-222 B    |      ✓       | Duty cycle / airtime    |  AES-128   | Managed network               |
| Sigfox   | 12 B        |   Partial    | Hard payload limit      | Auth only  | Field rotation needed         |
| 802.11ah | 1500 B      |      ✓       | Duty cycle (EU) / power |   WPA2/3   | IP-based, UDP transport       |
| NB-IoT   | ~1600 B     |      ✓       | Energy / data cost      |    Yes     | Operator infrastructure       |
| LTE-M    | ~MTU        |      ✓       | Energy / data cost      |    Yes     | Operator infrastructure       |
| SMS      | 140 B       |      ✓       | Per-message cost        |     No     | Fallback / universal coverage |

The protocol's presence flag mechanism makes medium-aware field selection a
runtime decision rather than a compile-time decision. The same encoder can
produce a 10-byte packet for Sigfox and a 24-byte packet for LoRa, with the
receiver handling both identically.

## Appendix E. System Implementation Considerations

### E.1. Microcontroller Class Taxonomy

The iotdata protocol is designed to run on a wide range of microcontrollers
(MCU), but the appropriate implementation strategy varies significantly by
device class:

| Class | Examples                  | RAM       | Flash      | FPU  | Typical role                   |
| ----- | ------------------------- | --------- | ---------- | ---- | ------------------------------ |
| 1     | PIC16F, ATtiny, MSP430G   | 256B-2KB  | 4-16KB     | No   | Sensor (encode only)           |
| 2     | PIC18F, STM32L0, nRF52    | 2-64KB    | 32-256KB   | No\* | Sensor (encode + basic decode) |
| 3     | ESP32-C3, STM32F4, RP2040 | 256-520KB | 384KB-16MB | Yes  | Sensor + gateway               |
| 4     | Raspberry Pi, Linux SBC   | 256MB+    | SD/eMMC    | Yes  | Gateway / server               |

\*nRF52840 has an FPU; most Class 2 devices do not.

The reference implementation currently targets Class 3 and 4 devices. Class 1
and 2 devices require implementation strategies discussed below. The design is
specifically intended to be extended to them.

### E.2. Memory Footprint

The reference implementation's data structures have the following sizes
(measured on a 64-bit platform; 32-bit targets will be smaller due to pointer
size):

| Structure           | Size    | Purpose                                     |
| ------------------- | ------- | ------------------------------------------- |
| `iotdata_encoder_t` | ~300 B  | Encoder context (all fields + TLV pointers) |
| `iotdata_decoded_t` | ~2000 B | Decoded packet (includes TLV data buffers)  |

The encoder context (~300 bytes) is dominated by the TLV pointer array (8
entries × 2 pointers × 8 bytes = 128 bytes on 64-bit). The core sensor fields
occupy approximately 60 bytes. On a 32-bit MCU:

- Full encoder context: ~200 bytes
- Encoder context without TLV: ~72 bytes
- Core sensor fields alone: ~50 bytes

The decoded struct (~2000 bytes) is dominated by the TLV data buffers (8 entries
× ~256 bytes = 2048 bytes). This structure is designed for gateway/server use
and is NOT appropriate for Class 1 or 2 devices. A minimal decoder that ignores
TLV data needs approximately 60 bytes.

TLV support can be excluded from the encoder, which would yield the most
considerable level of savings if resource constrained.

### E.3. Encoder Architecture: Store-Then-Pack

The current encoder uses a "store then pack" strategy:

```c
iotdata_encode_begin(&enc, buf, sizeof(buf), variant, station, seq);
iotdata_encode_battery(&enc, 84, false);    /* stores values */
iotdata_encode_environment(&enc, 21.5f, 1013, 45);
iotdata_encode_end(&enc, &out_len);         /* packs all at once */
```

**Advantages:**

- Fields can be added in any order — the variant table determines wire order at
  pack time.
- Validation happens eagerly, at the `encode_*()` call site.
- The presence bytes are computed once the full field set is known, avoiding
  backfill.

**Disadvantage:**

- The full encoder context must be held in RAM simultaneously: all field values
  plus the output buffer. On Class 3+ devices this is negligible; on Class 1
  devices (256 bytes RAM) it is prohibitive without stripping.

### E.4. Encoder Alternative: Pack-As-You-Go

For severely RAM-constrained devices (Class 1), a pack-as-you-go encoder could
eliminate the context struct entirely. The encoder would write bits directly to
the output buffer as each field is supplied.

The challenge is that the presence bytes (at bit offsets 32-39 and optionally
40-47) must appear in the wire format before the data fields they describe, but
the encoder does not know which fields will be present until all `encode_*()`
calls have been made.

Two approaches can resolve this:

**Approach A: Presence byte backfill.** Reserve the presence byte positions in
the output buffer (write zeros), then pack each field's bits immediately. After
the last field, go back and write the correct presence bytes. This requires
fields to be supplied in strict field order (S0, S1, S2...) so that the bit
cursor advances correctly.

```text
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

**Approach B: Two-pass encode.** First pass: call all `encode_*()` functions
which simply set bits in a `fields_present` bitmask (1 byte of RAM). Second
pass: iterate the variant field table and pack only the fields that are present.
This requires the field values to be available again on the second pass, either
from global/static variables or by re-reading the sensors.

**Trade-offs:**

| Property             | Store-then-pack | Pack-as-you-go (A) | Two-pass (B) |
| -------------------- | :-------------: | :----------------: | :----------: |
| RAM (no TLV)         |   ~72 B + buf   |     ~4 B + buf     | ~4 B + buf\* |
| Field order          |       Any       | Strict field order |     Any      |
| Code complexity      |       Low       |       Medium       |    Medium    |
| Re-read sensors      |       No        |         No         |     Yes      |
| Suitable for Class 1 |    Marginal     |        Yes         |     Yes      |

\*Two-pass requires field values to be available on the second pass, either
stored elsewhere or re-read from hardware.

The reference implementation uses store-then-pack because it is the most
developer-friendly and the target devices (ESP32-C3, Class 3) have ample RAM.
Implementers targeting Class 1 devices SHOULD consider pack-as-you-go with
backfill. Approach A maybe provided in a future version of the reference
implementation.

### E.5. Compile-Time Field Stripping (#ifdef)

The reference implementation factors all per-field operations into static inline
functions specifically to enable compile-time stripping:

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

Each field type has 6 functions (pack, unpack, json_add, json_read, dump,
print). On an embedded sensor that only transmits battery, environment, and
depth:

| Component                     | Functions | Approx. code size |
| ----------------------------- | :-------: | ----------------: |
| Core (header, presence, bits) |     —     |             ~1 KB |
| Battery (6 functions)         |     6     |            ~400 B |
| Environment (6 functions)     |     6     |            ~500 B |
| Depth (6 functions)           |     6     |            ~350 B |
| **Included total**            |    18     |       **~2.2 KB** |
| Solar (excluded)              |     6     |            -400 B |
| Link quality (excluded)       |     6     |            -350 B |
| Flags (excluded)              |     6     |            -300 B |
| Position (excluded)           |     6     |            -500 B |
| Datetime (excluded)           |     6     |            -400 B |
| JSON functions (excluded)     |    ~20    |             -2 KB |
| Print/dump (excluded)         |    ~20    |           -1.5 KB |

A fully stripped sensor-only build (encode path only, 3 field types, no
JSON/print/dump) can fit in approximately 2-3 KB of flash. This is achievable on
Class 1 devices.

### E.6. Floating Point Considerations

Several encode functions accept floating-point parameters:

- `iotdata_encode_environment()` takes `float` temperature
- `iotdata_encode_position()` takes `double` latitude/longitude
- `iotdata_encode_link()` takes `float` SNR

On MCUs without a hardware FPU (most Class 1 and many Class 2 devices),
floating-point arithmetic is emulated in software, which is both slow (~50-100
cycles per operation) and adds code size (~2-5 KB for soft-float library).

For Class 1 targets, implementers SHOULD consider:

- **Integer-only temperature API:** Accept temperature as a fixed-point integer
  in units of 0.25°C offset from -40°C. The caller performs
  `q = (temp_raw - (-40*4))` and passes `q` directly. No floating point needed.

- **Integer-only position API:** Accept pre-quantised 24-bit latitude and
  longitude values. The caller uses the GNSS receiver's native integer output
  and scales appropriately.

- **Integer-only SNR:** Accept SNR as an integer in dB.

The reference implementation uses float/double for developer convenience on
Class 3+ targets. However, the compile-time configurations of
`IOTDATA_NO_FLOATING_DOUBLES` or `IOTDATA_NO_FLOATING` can be used to remove
floating point operations and provide replacement integer-only entry points such
as temperature encoders that take values multiplied by 100, e.g. 152 to
represent a temperature of 15.2°C.

### E.7. Dependencies and Portability

The reference implementation has the following dependency profile:

| Component       | Dependencies                                          | Required for            |
| --------------- | ----------------------------------------------------- | ----------------------- |
| Encoder         | `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<math.h>` | All builds              |
| Decoder         | Same as encoder                                       | Gateway / bidirectional |
| JSON conversion | `libcjson`                                            | Gateway / server        |
| Print / dump    | `<stdio.h>`                                           | Debug / gateway         |

The core encoder has no external library dependencies. The `<math.h>` dependency
is for `round()` and `floor()` in the quantisation functions; on platforms where
`<math.h>` is unavailable or expensive, these can be replaced with integer
arithmetic equivalents:

```c
/* Integer-only round for non-negative values */
static inline uint16_t int_round(uint32_t num, uint32_t denom) {
    return (uint16_t)((num + denom / 2) / denom);
}
```

The `libcjson` dependency exists only for the JSON serialisation functions and
SHOULD be excluded from embedded builds via `#ifdef IOTDATA_NO_JSON`.

### E.8. Stack vs Heap Allocation

The encoder and decoder are designed for **stack allocation only**. No
`malloc()` or `free()` calls are made in any encode or decode path. This is
critical for:

- **Bare-metal systems** without a heap allocator.
- **RTOS environments** (FreeRTOS, Zephyr) where heap fragmentation must be
  avoided and stack sizes are fixed.
- **Safety-critical systems** where dynamic allocation is prohibited by coding
  standards (MISRA C, etc.).

The JSON conversion functions (`iotdata_decode_to_json`) do allocate heap memory
via `cJSON_CreateObject()` and return a `malloc`'d string. These functions are
gateway/server-only and are not intended for embedded use.

### E.9. Endianness

The bit-packing functions operate on individual bytes via bit manipulation and
are **endian-agnostic**. The `bits_write()` and `bits_read()` functions address
the output buffer byte-by-byte, computing bit offsets explicitly. No multi-byte
loads or stores are used in the packing/unpacking path.

This means the same code runs correctly on:

- Little-endian ARM (ESP32, STM32, RP2040)
- Big-endian PIC (in standard configuration)
- Any other byte-addressable architecture

No byte-swap operations are needed when moving between platforms.

### E.10. Real-Time Considerations

The encoder's `encode_end()` function performs a single linear pass over the
variant field table and packs all present fields. The execution time is bounded
and predictable:

- **Minimum** (battery only): ~50 bit operations
- **Maximum** (all fields + TLV): ~300 bit operations

Each bit operation is a constant-time shift-and-mask. There are no loops with
data-dependent iteration counts (except TLV string encoding, which iterates over
the string length). The encoder is suitable for use in interrupt service
routines or time-critical sections, though calling it from a main-loop context
is more typical.

### E.11. Platform-Specific Notes

**Raspberry Pi / Linux (Class 4):** The full implementation runs unmodified,
including JSON conversion, print, dump, and all 8 field types. Typically used as
a gateway, receiving packets via LoRa HAT or USB-connected radio module,
decoding to JSON, and forwarding via MQTT, HTTP, or database insertion.

**ESP32-C3 (Class 3, primary target):** The reference implementation runs
unmodified. The ESP32-C3 has 400 KB SRAM and 4 MB flash but no hardware FPU —
both single- and double-precision arithmetic is software-emulated. Use
`IOTDATA_NO_FLOATING` for best performance. Both the encoder and decoder,
including JSON functions, fit comfortably. The ESP-IDF build system supports
`#ifdef` stripping via `menuconfig` or `sdkconfig` defines.

**STM32L0 (Class 2):** With 20 KB RAM and 64-192 KB flash, both the encoder
context (328 bytes) and decoded struct (2176 bytes) fit on the stack
comfortably. Exclude JSON, print, and dump functions. Use
`-ffunction-sections -fdata-sections -Wl,--gc-sections` to strip unused code.

**PIC18F (Class 2):** Similar constraints to STM32L0 but with typically less RAM
(2-4 KB) and a more limited C compiler. The reference implementation's use of
`static inline` functions may need to be adjusted (some PIC compilers do not
inline effectively). Consider the pack-as-you-go approach (Section E.4) for
minimal RAM usage.

**PIC16F (Class 1):** With as little as 256 bytes of RAM, the full encoder
context does not fit. Use pack-as-you-go with backfill (Section E.4, Approach
A), integer-only APIs (Section E.6), and aggressive `#ifdef` stripping (Section
E.5). Target flash budget: 2-3 KB for a battery + environment + depth encoder.

### E.12. Class 1 Hand-Rolled Encoder Example

On devices with as little as 256 bytes of RAM (PIC16F, ATtiny), the library's
table-driven architecture is unnecessary overhead. The following self-contained
function encodes a weather station packet with battery, environment, and two TLV
entries directly into a caller-provided buffer. No structs, no function
pointers, no library linkage — just bit-packing arithmetic. The full weather
station packet requires a buffer of no more than 32 bytes (without TLV). If
really necessary, the implementation can avoid buffers and use ws_bits to write
directly to an output (e.g. serial port).

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

**Resource usage:** This function requires approximately 20 bytes of stack (loop
counters, bit pointer, temporaries) plus the output buffer (or not, if directly
writing to serial output). The code compiles to under 400 bytes on PIC18F or
AVR. The caller can reuse the output buffer between transmissions.

**Adapting for other variants:** Copy the function, change the presence byte
constant, and add or remove the field sections. Each field is a self-contained
block of `ws_bits` calls — the protocol document (Sections 4-6) gives the bit
widths and quantisation formulae for every field type.

## Appendix F. Example Weather Station Output

The `test-example` target generates pseudo sensor data simulating a weather
station to illustrate quantisation effects and ancillary (dump, print and JSON)
functionality.

```text
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

## Appendix G. Mesh Relay Protocol

### Overview

The iotdata mesh relay protocol extends the reach of sensor networks by allowing
dedicated relays to forward sensor data across multiple relays toward one or
more gateways. The protocol is designed to be **seamless** — existing sensors
require no firmware changes, the system works without mesh infrastructure, and
relays can be inserted into a live deployment to fill coverage gaps.

The mesh layer is carried within the existing iotdata wire format using variant
ID 15 (0x0F) for all control-plane traffic. This means mesh packets share the
same 4-byte header structure as sensor data, can coexist on the same radio
channel, and are handled by the same receive path up to the point of variant
dispatch. Relay nodes have a dedicated station ID and can also convey sensor
data under that ID.

### A. Use Cases and System Roles

#### A.1 The Problem

LoRa radio links between sensors and gateways are subject to terrain,
vegetation, buildings, and seasonal variation. A sensor that works reliably in
winter may become intermittent when foliage returns in spring. A sensor placed
in a valley or behind a structure may never reach the gateway directly.
Increasing transmit power or antenna height is not always practical or
permitted.

#### A.2 The Solution

Rather than requiring all sensors to participate in a mesh network (which adds
complexity, power consumption, and firmware requirements), the protocol
introduces a separate class of mesh-aware relays that transparently extend
range. Sensors remain simple transmit-only devices. The mesh is an overlay
infrastructure.

#### A.3 System Roles

The protocol defines three roles. A single physical device may implement one or
two of these roles simultaneously.

**Sensor** — A device that periodically transmits iotdata-encoded packets
containing measurement data. Sensors are transmit-only, fire-and-forget. They
have no awareness of the mesh, do not listen for packets, and do not participate
in routing. A sensor's firmware is identical whether or not mesh infrastructure
is deployed. Sensors use iotdata variant IDs 0–14 as defined by their
measurement type. Sensors are typically power constrained.

**Relay** — A mesh-aware device that listens for both sensor packets and mesh
control packets. Its primary function is to forward sensor data toward a gateway
when the sensor cannot reach the gateway directly. Relay nodes form a
self-organising tree topology rooted at gateways, using periodic beacon messages
to discover routes. A relay treats sensor payloads as opaque byte sequences — it
never inspects or interprets measurement fields. A relay may optionally also
function as a sensor (dual-role), transmitting its own measurement data (e.g.
position, battery level, environment) using a standard iotdata variant alongside
its mesh traffic on variant 15. Relays have higher power demand than sensors,
but are more than likely not to be mains powered.

**Gateway** — A mesh-aware device that receives sensor data (directly or via
relays) and delivers it to upstream systems for processing, storage, and
display. Gateways originate beacon messages that define the routing topology. A
deployment may include multiple gateways for redundancy or to cover a wide area.
Each gateway is identified by a unique station ID. Gateways perform duplicate
suppression — if the same sensor packet arrives both directly and via a relay,
only the first arrival is processed. Gateways are typically mains powered and
likely to be connected to network and internet infrastructure.

#### A.4 Role Capabilities

| Capability                       | Sensor            | Relay                | Gateway            |
| -------------------------------- | ----------------- | -------------------- | ------------------ |
| Transmits own sensor data        | yes               | optional (dual-role) | no                 |
| Listens for packets              | no                | yes                  | yes                |
| Forwards sensor data             | no                | yes                  | no (endpoint)      |
| Participates in mesh routing     | no                | yes                  | yes (root)         |
| Originates beacons               | no                | no                   | yes                |
| Rebroadcasts beacons             | no                | yes                  | no                 |
| Requires iotdata field knowledge | yes (own variant) | no (opaque relay)    | yes (all variants) |
| Firmware changes for mesh        | none              | mesh-specific        | mesh additions     |

### B. Design Principles

#### B.1 Seamless Operation

The mesh layer is an optional enhancement, not a prerequisite. A deployment
consisting only of sensors and gateways works exactly as it does today. Mesh
infrastructure can be added incrementally — deploying a relay between a
struggling sensor and the gateway immediately improves reliability without
touching the sensor.

#### B.2 Protocol Integration

Mesh control packets use iotdata variant ID 15 (0x0F). This reserves the final
variant slot for mesh traffic while leaving variants 0–14 available for sensor
data definitions. The 4-byte iotdata header (variant, station_id, sequence) is
shared by all packet types, meaning mesh packets are structurally valid iotdata
packets with a different interpretation of the payload.

Byte 4, which serves as a presence bitmap in sensor variants, serves as a
control type field in variant 15 packets. The upper nibble identifies the mesh
packet type (4 bits, supporting up to 16 types). This allows the receive path to
branch on variant ID alone: variants 0–14 route to the sensor data decoder,
variant 15 routes to the mesh handler.

#### B.3 Opaque Forwarding

Relays never inspect the contents of sensor packets beyond the 4-byte iotdata
header. The header is read only to extract the originating sensor's station_id
and sequence number for duplicate suppression. All remaining bytes are treated
as an opaque blob, copied verbatim during forwarding. This means the mesh layer
has zero coupling to field definitions, variant suites, encoding formats, or any
future changes to the iotdata measurement schema.

The sole structural dependency is the position and size of station_id (12 bits
at bytes 0–1) and sequence (16 bits at bytes 2–3) in the iotdata header. This is
the most stable contract in the protocol and is not expected to change.

#### B.4 Multiple Gateway Support

Each gateway originates its own beacon stream identified by its station_id
(carried as `gateway_id` in the beacon). Relays independently track which
gateway trees they belong to and select the best gateway by cost (relay count),
breaking ties by received signal strength. If a gateway fails, its beacons
cease, and so relays in its tree will time out after a configurable number of
missed beacon rounds, and automatically adopt an alternative gateway's tree.

#### B.5 Gradient-Based Routing

The mesh uses a simplified distance-vector approach where each relay knows its
cost (number of relays to reach the gateway) and forwards data toward lower-cost
neighbours. This is conceptually similar to RPL (RFC 6550, Routing Protocol for
Low-Power and Lossy Networks) but dramatically simplified — no full topology
state, no Directed Acyclic Graph computation, no IPv6 dependency. Each node
stores only its parent, a backup parent, and a small neighbour table.

### C. Protocol Flows

#### C.1 Topology Discovery

Topology is built through periodic beacon propagation from gateways outward.

```text
Gateway (cost=0)
    │
    │  BEACON (gateway_id=G, generation=N, cost=0)
    │
    ▼
Relay A hears beacon, adopts Gateway as parent, sets cost=1
    │
    │  BEACON (gateway_id=G, generation=N, cost=1)   [after random 1–5s jitter]
    │
    ▼
Relay B hears Relay A's rebroadcast, adopts Relay A as parent, sets cost=2
    │
    │  BEACON (gateway_id=G, generation=N, cost=2)   [after random 1–5s jitter]
    │
    ▼
...continues outward until no new nodes hear the beacon
```

Gateways transmit beacons at a regular interval (of which there is no
recommended default, as this should be a function of the periodicity and density
of sensor network, but 60 seconds is a reasonable figure). Each beacon carries a
generation counter that increments per round. Relays compare incoming beacons
against their current state:

- Newer generation (modular comparison within half the 12-bit range): update
  parent if cost is equal or better.
- Same generation, lower cost: adopt the new sender as parent.
- Same generation, equal or higher cost: suppress — do not rebroadcast.

The random rebroadcast jitter (1–5 seconds) prevents synchronised retransmission
from nodes that hear the same beacon simultaneously, reducing collisions in
dense areas.

#### C.2 Sensor Data Forwarding

Sensor data flows inward from sensors toward gateways, relayed transparently by
relays.

```text
Sensor S transmits raw iotdata packet (variant=V, station=S, seq=N)
    │
    │  [raw packet, no mesh awareness]
    │
    ├──────────────────────┐
    ▼                      ▼
Gateway (hears directly)   Relay A (hears sensor)
    │                      │
    │ process normally     │ wrap in FORWARD, send to parent
    │                      │
    │                      ▼
    │                  Gateway (receives FORWARD)
    │                      │
    │                      │ unwrap inner packet
    │                      │ dedup: {S, N} already seen? → discard
    │                      │ otherwise process normally
    ▼                      ▼
    [sensor data processed once]
```

When a relay hears a raw sensor packet (any variant 0–14), it waits a short
random backoff (200–1000ms). If during that backoff it hears another relay
forward the same packet (identified by matching origin station and sequence), it
suppresses its own forward. This Trickle-style suppression reduces redundant
airtime in areas where multiple relays overlap.

If no suppression occurs, the relay wraps the raw sensor packet in a FORWARD
control message (variant 15, ctrl_type 0x1) addressed to its parent and
transmits. The parent, if another relay, repeats the process — unwrap, dedup,
re-wrap with its own header, forward to its parent — until the packet reaches a
gateway.

#### C.3 Relay-by-Relay Acknowledgement

Each FORWARD is acknowledged by the receiving parent to confirm delivery.

```text
Relay A                          Relay B (A's parent)
  │                             │
  │──── FORWARD (seq=X) ───────>│
  │                             │
  │<──── ACK (fwd_station=A, ──>│
  │           fwd_seq=X)        |
  │                             |
  [clear retry timer]           [forward inner packet upstream]
```

If no ACK is received within a timeout (recommended 500ms for high frequency
sensor networks, up to 15-30 seconds for low frequency networks), the sender
retries up to a configurable number of attempts (recommended: 3). After
exhausting retries, the sender marks its parent as unreliable, promotes its
backup parent (if available), and retransmits the FORWARD to the new parent. If
no backup parent is available, the node broadcasts a ROUTE_ERROR and enters an
orphaned state, listening for beacons to reattach to the tree.

#### C.4 Fast Failover

When a relay loses all upstream paths, it broadcasts a ROUTE_ERROR so downstream
nodes can immediately reroute rather than waiting for beacon timeout.

```text
Relay B (was Relay C's parent)    Relay C (child of B)
  │                            │
  [B loses its parent]         │
  │                            │
  │──── ROUTE_ERROR ──────────>│
  │     (reason=parent_lost)   │
  │                            │
                               [C immediately seeks alternative parent from neighbour table]
```

This converts a multi-minute outage (waiting for 3 missed beacon rounds × 60s =
180s) into sub-second failover in the best case.

#### C.5 Network Monitoring

Relays periodically send NEIGHBOUR_REPORT messages upstream to the gateway,
providing a snapshot of their local topology view. These reports are forwarded
like any other data (wrapped in FORWARD by upstream relays). The gateway
aggregates reports from all relays to build a complete network topology graph,
enabling operators to visualise the mesh, identify weak links, and plan node
placement.

#### C.6 Reachability Testing (v2)

In a future protocol revision, the gateway may send PING messages routed
downstream toward a specific target node. The target responds with a PONG that
routes back upstream. This provides on-demand reachability confirmation and
round-trip-time measurement without waiting for the target's next scheduled data
or neighbour report transmission.

### D. Packet Structures

#### D.1 Standard iotdata Header (all packets, all variants)

| Byte | Bits        | Field                                                               |
| ---- | ----------- | ------------------------------------------------------------------- |
| 0    | [7:4]       | variant_id (4 bits: 0–14 = sensor data, 15 = mesh control)          |
| 0–1  | [3:0]+[7:0] | station_id (12 bits: 0–4095)                                        |
| 2–3  | [15:0]      | sequence (16 bits, big-endian)                                      |
| 4    | [7:0]       | presence bitmap (variants 0–14) \| ctrl_type + payload (variant 15) |

The variant and station_id are packed into a 4+12 bit structure:

```c
byte[0] = (variant << 4) | (station_id >> 8)
byte[1] = station_id & 0xFF
```

This packing primitive recurs throughout the mesh protocol wherever a 4-bit
field is paired with a 12-bit station_id or generation counter.

#### D.2 Variant 15 Common Header

All mesh control packets share this structure:

| Byte | Bits  | Field                     | Notes                                                                       |
| ---- | ----- | ------------------------- | --------------------------------------------------------------------------- |
| 0–1  | 4+12  | `0xF` \| `sender_station` | The mesh node transmitting this packet                                      |
| 2–3  | 16    | `sender_seq`              | Mesh sequence counter (separate from any sensor data sequence if dual-role) |
| 4    | [7:4] | `ctrl_type`               | Mesh packet type (0x0–0xF)                                                  |
| 4    | [3:0] | type-specific             | Upper nibble of first payload field                                         |

The remaining 4 bits of Byte 4 and the whole bytes of Byte 5 onward are
control-type-specific. Fields pack as a bitstream from byte 4, MSB-first, with
no padding except where explicitly noted.

#### D.3 BEACON (ctrl_type 0x0)

Originated by gateways, rebroadcast by relays. Flows outward from gateway.

| Byte | Bits | Field                         | Range   | Notes                                          |
| ---- | ---- | ----------------------------- | ------- | ---------------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station`     | 0–4095  | Who (re)broadcast this copy                    |
| 2–3  | 16   | `sender_seq`                  | 0–65535 |                                                |
| 4–5  | 4+12 | `ctrl=0x0` \| `gateway_id`    | 0–4095  | Originating gateway                            |
| 6    | 8    | `cost`                        | 0–255   | 0 at gateway, +1 per relay                     |
| 7    | 4+4  | `flags` \| `generation[11:8]` |         | flags: b0 = accepting forwards, b1–b3 reserved |
| 8    | 8    | `generation[7:0]`             | 0–4095  | Beacon round counter                           |

**Total: 9 bytes.**

Byte packing detail:

```c
buf[4] = (0x0 << 4) | (gateway_id >> 8)
buf[5] = gateway_id & 0xFF
buf[6] = cost
buf[7] = (flags << 4) | ((generation >> 8) & 0x0F)
buf[8] = generation & 0xFF
```

Generation uses wraparound comparison: beacon A is newer than B if
`(A - B) mod 4096` is in the range 1–2047. At a 60-second beacon interval,
generation wraps every ~68 hours.

#### D.4 FORWARD (ctrl_type 0x1)

Wraps a raw sensor packet for relay toward the gateway.

| Byte | Bits | Field                     | Range   | Notes                                          |
| ---- | ---- | ------------------------- | ------- | ---------------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station` | 0–4095  | This relay                                     |
| 2–3  | 16   | `sender_seq`              | 0–65535 |                                                |
| 4    | 4+4  | `ctrl=0x1` \| `ttl[7:4]`  |         |                                                |
| 5    | 4+4  | `ttl[3:0]` \| `0`         | 0–255   | 4-bit pad aligns inner packet to byte boundary |
| 6+   | 8×N  | `inner_packet`            |         | Raw iotdata bytes, opaque                      |

**Total: 6 + N bytes.**

Byte packing detail:

```c
buf[4] = (0x1 << 4) | (ttl >> 4)
buf[5] = (ttl & 0x0F) << 4           /* lower nibble is zero pad */
memcpy(&buf[6], inner_packet, N)     /* byte-aligned, no shifting */
```

The 4-bit pad at byte 5 lower nibble ensures the inner packet starts at a byte
boundary (offset 6). This is a deliberate trade-off: the pad may cause up to 11
bits of wasted space in the worst case (as the inner packet may already have up
to 7 bits wasted in the final byte alignment), but avoids requiring every relay
to bit-shift the entire opaque payload. For relay hot-path performance (just a
memcpy), this is the right choice. The pad nibble is reserved for future use
(e.g. priority, retry count).

Inner packet length is derived from the radio layer: `N = rx_packet_len - 6`.

For duplicate suppression, the relay reads bytes 6–9 of the radio frame (the
inner packet's iotdata header) to extract the originating sensor's station_id
and sequence:

```c
origin_station = ((buf[6] & 0x0F) << 8) | buf[7]
origin_sequence = (buf[8] << 8) | buf[9]
```

No FORWARD nesting occurs. Each relay creates a fresh FORWARD with its own
sender_station and sender_seq. The inner_packet bytes are always the original
sensor transmission, regardless of how many relays have occurred.

#### D.5 ACK (ctrl_type 0x2)

Relay-by-relay acknowledgement of a received FORWARD.

| Byte | Bits | Field                       | Range   | Notes                               |
| ---- | ---- | --------------------------- | ------- | ----------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station`   | 0–4095  | Parent sending the ACK              |
| 2–3  | 16   | `sender_seq`                | 0–65535 |                                     |
| 4–5  | 4+12 | `ctrl=0x2` \| `fwd_station` | 0–4095  | Child whose FORWARD is being ACKed  |
| 6–7  | 16   | `fwd_seq`                   | 0–65535 | Child's sender_seq from the FORWARD |

**Total: 8 bytes.**

#### D.6 ROUTE_ERROR (ctrl_type 0x3)

Broadcast by a relay that has lost all upstream paths.

| Byte | Bits | Field                     | Range   | Notes                                   |
| ---- | ---- | ------------------------- | ------- | --------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station` | 0–4095  | Orphaned node                           |
| 2–3  | 16   | `sender_seq`              | 0–65535 |                                         |
| 4    | 4+4  | `ctrl=0x3` \| `reason`    | 0–15    | 0=parent_lost, 1=overloaded, 2=shutdown |

**Total: 5 bytes.** The minimum possible mesh packet — just the common header
with a reason code.

Reason codes:

| Value   | Meaning                                       |
| ------- | --------------------------------------------- |
| 0x0     | parent_lost — all upstream links failed       |
| 0x1     | overloaded — too many children, shedding load |
| 0x2     | shutdown — graceful node shutdown             |
| 0x3–0xF | reserved                                      |

#### D.7 NEIGHBOUR_REPORT (ctrl_type 0x4)

Periodic topology snapshot sent upstream to the gateway.

**Header:**

| Byte | Bits | Field                                   | Range   | Notes                                   |
| ---- | ---- | --------------------------------------- | ------- | --------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station`               | 0–4095  | Reporting node                          |
| 2–3  | 16   | `sender_seq`                            | 0–65535 |                                         |
| 4–5  | 4+12 | `ctrl=0x4` \| `parent_id`               | 0–4095  | Current parent (0xFFF if orphaned)      |
| 6    | 8    | `my_cost`                               | 0–255   | Reporting node's cost                   |
| 7    | 6+2  | `num_neighbours` \| `gateway_id[11:10]` | 0–63    | Number of neighbour entries that follow |
| 8    | 8    | `gateway_id[9:2]`                       | 0–4095  | Current active gateway tree             |
| 9    | 2    | `gateway_id[1:0]`                       |         |                                         |

**Neighbour entry (3 bytes each):**

| Offset | Bits | Field                     | Range | Notes                                 |
| ------ | ---- | ------------------------- | ----- | ------------------------------------- |
| +0     | 8    | `cost`                    | 0–255 | Neighbour's advertised cost           |
| +1–2   | 4+12 | `rssi_q4` \| `station_id` |       | RSSI quantised to 4 bits + station_id |

**Total: 9.2 bytes + 3N bytes.**

RSSI quantisation uses 5 dBm steps from a floor of −120 dBm:

| rssi_q4 | Approximate dBm |
| ------- | --------------- |
| 0       | ≤ −120          |
| 1       | −115            |
| 2       | −110            |
| ...     | ...             |
| 10      | −70             |
| 15      | ≥ −45           |

Encode: `rssi_q4 = clamp((rssi_dbm + 120) / 5, 0, 15)`. Decode:
`rssi_dbm ≈ (rssi_q4 × 5) − 120`.

Example sizes:

| Neighbours | Total bytes |
| ---------- | ----------- |
| 4          | 22          |
| 8          | 34          |
| 16         | 58          |
| 32         | 106         |
| 63         | 199         |

All fit within standard LoRa maximum payload sizes (222 bytes at SF7/125kHz, up
to 255 at lower spreading factors).

#### D.8 PING (ctrl_type 0x5) — v2

Gateway-originated reachability test, routed downstream toward a target node.

| Byte | Bits | Field                     | Range   | Notes                                    |
| ---- | ---- | ------------------------- | ------- | ---------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station` | 0–4095  | Current forwarding relay                 |
| 2–3  | 16   | `sender_seq`              | 0–65535 |                                          |
| 4–5  | 4+12 | `ctrl=0x5` \| `target_id` | 0–4095  | Destination node                         |
| 6    | 8    | `ttl`                     | 0–255   | Decremented per relay on downstream path |
| 7    | 8    | `ping_id`                 | 0–255   | Correlates with PONG                     |

**Total: 8 bytes.**

#### D.9 PONG (ctrl_type 0x6) — v2

Response to PING, flows upstream toward the gateway.

| Byte | Bits | Field                      | Range   | Notes                                 |
| ---- | ---- | -------------------------- | ------- | ------------------------------------- |
| 0–1  | 4+12 | `0xF` \| `sender_station`  | 0–4095  | Responding node                       |
| 2–3  | 16   | `sender_seq`               | 0–65535 |                                       |
| 4–5  | 4+12 | `ctrl=0x6` \| `gateway_id` | 0–4095  | Route back to originating gateway     |
| 6    | 8    | `relays`                   | 0–255   | Incremented each relay on return path |
| 7    | 8    | `ping_id`                  | 0–255   | Echoed from PING                      |

**Total: 8 bytes.**

#### D.10 Reserved (ctrl_type 0x7–0xF)

Reserved for future use. Relays receiving an unrecognised ctrl_type should
silently discard the packet.

#### D.11 Packet Summary

| ctrl    | Name             | Direction                     | Bytes    | Version |
| ------- | ---------------- | ----------------------------- | -------- | ------- |
| 0x0     | BEACON           | outward (gateway → relays)    | 9        | v1      |
| 0x1     | FORWARD          | inward (relays → gateway)     | 6 + N    | v1      |
| 0x2     | ACK              | single relay (parent → child) | 8        | v1      |
| 0x3     | ROUTE_ERROR      | broadcast                     | 5        | v1      |
| 0x4     | NEIGHBOUR_REPORT | inward (relays → gateway)     | 9.2 + 3N | v1      |
| 0x5     | PING             | outward (gateway → target)    | 8        | v2      |
| 0x6     | PONG             | inward (target → gateway)     | 8        | v2      |
| 0x7–0xF | reserved         | —                             | —        | —       |

### E. Node Operation and Requirements

#### E.1 Relay Node State

A relay maintains the following state in RAM. Total memory footprint is under
512 bytes for typical configurations.

**Routing state:**

- `parent` — station_id, cost, RSSI, last beacon time (8 bytes)
- `backup_parent` — same structure (8 bytes)
- `my_cost` — current relay count to gateway (1 byte)
- `my_gateway` — gateway_id of the tree this node belongs to (2 bytes)
- `beacon_generation` — most recently processed generation (2 bytes)

**Neighbour table (up to 63 entries):**

- Per entry: station_id, cost, RSSI, last_heard timestamp (8 bytes each)
- Typical: 8–16 entries = 64–128 bytes
- Entries expire after a configurable timeout (recommended: 5× beacon interval)

**Duplicate suppression ring (32–64 entries):**

- Per entry: origin_station_id (12 bits) + origin_sequence (16 bits) = 4 bytes
  packed
- Ring of 64 entries = 256 bytes
- FIFO: oldest entry evicted when ring is full

**Forward retry queue (4–8 entries):**

- Per entry: pending FORWARD packet buffer, retry count, timestamp of last
  attempt, parent at time of send
- Entries cleared on ACK receipt or after max retries

#### E.2 Relay Node Main Loop

```text
initialise:
    listen for beacons to join a tree
    set status = orphaned

on receive packet:
    if variant == 15:
        switch (ctrl_type):
            BEACON:     process_beacon()
            FORWARD:    unwrap, dedup, re-wrap, forward to parent
            ACK:        match against forward retry queue, clear entry
            ROUTE_ERROR: if sender is my parent, trigger parent reselection
            other:      discard
    else:
        // raw sensor packet (variant 0–14)
        schedule_forward(packet)   // backoff, dedup, wrap, send to parent

periodic timers:
    beacon rebroadcast   — on beacon receipt, after 1–5s random jitter
    forward retry        — check pending queue, retransmit if ACK timeout
    parent timeout       — if no beacon for 3 rounds, orphan and reselect
    neighbour report     — send report upstream every N minutes
    own sensor readings  — if dual-role, encode and transmit own data
```

#### E.3 Gateway Additions

An existing iotdata gateway requires three additions to support mesh:

**Beacon origination:** Every N seconds (60 default), transmit a BEACON with
cost=0 and an incrementing generation counter. The gateway_id is the gateway's
own station_id.

**FORWARD handling:** On receiving a variant 15 packet with ctrl_type 0x1,
extract the inner packet starting at byte 6 and process it through the normal
iotdata receive path (decode, store, display). Send an ACK back to the FORWARD's
sender.

**Duplicate suppression:** Maintain a ring buffer of recently-seen {station_id,
sequence} pairs. Check every incoming sensor packet (whether received directly
or unwrapped from a FORWARD) against this ring. Discard duplicates, keeping the
first arrival.

The existing iotdata decode path for variants 0–14 is completely untouched.

#### E.4 Duplicate Suppression

Duplicate suppression is critical because the same sensor packet may arrive at a
gateway via multiple paths: directly, via one relay, or via different relay
chains. Without dedup, every measurement would be recorded multiple times.

The dedup key is {origin_station_id, origin_sequence}, extracted from the
iotdata header of the original sensor packet. Both relays and gateways maintain
dedup rings:

- **At the relays:** prevents forwarding the same sensor packet twice (e.g. two
  relays both hear the same sensor and both forward upstream — the upstream
  relay deduplicates).
- **At the gateway:** prevents processing the same data twice when it arrives
  both directly and via relay.

A ring buffer of 64 entries is sufficient for most deployments. With 16 sensors
transmitting every 5–15 seconds, the ring covers approximately 5–20 minutes of
history. The ring is FIFO — the oldest entry is evicted when the buffer is full.

#### E.5 Parent Selection and Failover

A relay selects its parent using the following priority:

1. Lowest cost (fewest relays to gateway)
2. If equal cost, highest RSSI (strongest signal)
3. If equal cost and RSSI, prefer existing parent (stability)

The backup parent is the second-best candidate by the same criteria.

**Failover triggers:**

- FORWARD ACK timeout after max retries — parent is unreachable.
- ROUTE_ERROR received from parent — parent has lost its own uplink.
- Beacon timeout — no beacon from parent's tree for 3 consecutive rounds.

On failover, the node promotes its backup parent, recalculates cost (new
parent's cost + 1), and continues forwarding. If no backup is available, the
node broadcasts a ROUTE_ERROR with reason=parent_lost and enters orphaned state,
listening for beacons from any tree.

#### E.6 Beacon Rebroadcast Rules

A relay rebroadcasts a received beacon only if:

1. The beacon's generation is newer than the last processed generation for this
   gateway_id (modular comparison: newer if difference mod 4096 is in range
   1–2047), OR
2. The beacon has the same generation but offers a strictly lower cost than the
   current best seen for this generation.

If the beacon does not meet either condition, it is suppressed. This prevents
beacon storms in dense deployments where many nodes hear the same beacon
simultaneously. The random rebroadcast jitter (1–5 seconds) further reduces
collision probability.

#### E.7 Forward Suppression (Trickle)

When a relay hears a raw sensor packet that it intends to forward, it waits a
random backoff period (200–1000ms) before transmitting the FORWARD. During this
backoff, if the node hears another relay transmit a FORWARD containing the same
inner packet (identified by matching origin station and sequence in the inner
header), it cancels its own forward.

This Trickle-style suppression (inspired by RFC 6206) significantly reduces
redundant airtime in areas where multiple relay nodes have overlapping coverage.
In the worst case (no other relay forwards), it adds 200–1000ms latency to the
first relay. In dense areas, it eliminates duplicate transmissions entirely.

### F. Deployment Considerations

#### F.1 Hardware

The mesh protocol is designed for off-the-shelf LoRa modules (e.g. Semtech
SX1262-based modules like Ebyte E22-900T22D) connected to ESP32-class
microcontrollers. No specialised radio hardware is required.

Recommended relay hardware:

- ESP32-C3 or ESP32-S3 (low cost, low power, sufficient RAM and compute)
- LoRa module with RSSI reporting (for neighbour quality assessment)
- Reliable power: mains, solar + battery, or PoE where available
- Weatherproof enclosure with external antenna for outdoor deployment

Relays should have reliable power supplies. Unlike sensors which can sleep
between transmissions, relays must listen continuously. Solar + battery is
viable in most climates with an appropriately sized panel (5–10W) and battery
(5–10Ah).

#### F.2 Range and Relay Budgets

Typical LoRa range per relay in different environments at commonly used settings
(SF7–SF9, 125kHz bandwidth, 14–22 dBm transmit power):

| Environment               | Typical range per relay | Notes                                       |
| ------------------------- | ----------------------- | ------------------------------------------- |
| Open farmland             | 2–8 km                  | Line-of-sight, minimal obstructions         |
| Rolling hills             | 1–4 km                  | Terrain shadowing, partial LOS              |
| Forest / dense vegetation | 0.5–2 km                | Significant attenuation, seasonal variation |
| Urban / buildings         | 0.3–1.5 km              | Multipath, reflection, penetration loss     |
| Indoor-to-outdoor         | 50–500 m                | Wall penetration, highly variable           |

With a maximum TTL of 255 relays, the theoretical network span is enormous
(hundreds of kilometres). In practice, latency and airtime constraints limit
useful depth to 5–10 relays in most deployments. Beyond 10 relays, per-packet
latency (including backoff, transmission time, and ACK round-trips) accumulates
significantly.

#### F.3 Latency Budget

Per-relay latency for a forwarded packet:

| Component                    | Typical     | Notes                                     |
| ---------------------------- | ----------- | ----------------------------------------- |
| Trickle backoff              | 200–1000 ms | Random, first-relay only for sensor→relay |
| LoRa TX time (30 bytes, SF7) | ~50 ms      | Higher SF increases proportionally        |
| ACK wait                     | 0–500 ms    | Timeout before retry                      |
| Processing overhead          | < 1 ms      | Negligible on ESP32                       |

Estimated end-to-end latency by relay count:

| Relays | Best case | Typical |
| ------ | --------- | ------- |
| 1      | ~300 ms   | ~700 ms |
| 2      | ~400 ms   | ~1.2 s  |
| 3      | ~500 ms   | ~1.8 s  |
| 5      | ~700 ms   | ~3.0 s  |
| 10     | ~1.2 s    | ~6.0 s  |

For environmental sensor data with transmission intervals of 5–15 seconds, these
latencies are entirely acceptable. The data is not time-critical — a few seconds
of additional delay has no impact on the value of temperature, moisture, or
depth readings.

#### F.4 Airtime and Duty Cycle

Every relay consumes airtime. A packet forwarded across 3 relays uses 3× the
airtime of a direct transmission plus ACK overhead. In regions with regulatory
duty cycle limits (e.g. 1% in EU 868MHz sub-band h1.4), this constrains the
aggregate throughput.

Example: 16 sensors transmitting every 10 seconds, average packet 25 bytes.

| Scenario                        | Packets/sec             | Airtime/sec (SF7) | Effective duty cycle |
| ------------------------------- | ----------------------- | ----------------- | -------------------- |
| All direct to gateway           | 1.6                     | ~80 ms            | ~0.8%                |
| All via 1 relay                 | 1.6 × 2 (fwd+ack)       | ~210 ms           | ~2.1%                |
| All via 2 relays                | 1.6 × 4 (2×fwd + 2×ack) | ~420 ms           | ~4.2%                |
| Mixed (50% direct, 50% 1-relay) | ~2.4                    | ~145 ms           | ~1.5%                |

In practice, most sensors will be direct to gateway. Only sensors with poor
direct links use mesh relays. A typical deployment with 3–5 sensors using
1-relay stays well within 1% duty cycle for the relay and gateway.

For deployments requiring higher throughput, use the 915MHz ISM band (Americas,
Australia) which has more relaxed duty cycle requirements, or use LoRa spreading
factor 7 (fastest airtime) with forward error correction.

#### F.5 Recommended Maximum Configuration Per Deployment

| Parameter                      | Recommended limit | Hard limit                                 |
| ------------------------------ | ----------------- | ------------------------------------------ |
| Sensors per gateway            | 50–100            | 4095 (station_id space)                    |
| Relays per gateway             | 10–20             | No hard limit, bounded by airtime          |
| Maximum useful relays          | 5–10              | 255 (TTL field)                            |
| Network span                   | 10–50 km          | Limited by relay count × range             |
| Gateways per deployment        | 2–5               | No hard limit (each runs independent tree) |
| Neighbour table size per relay | 8–16 typical      | 63 (protocol limit)                        |
| Total nodes (sensors + relays) | 100–200           | 4095 (station_id space)                    |

### G. Example Deployments

#### G.1 Moderate Farm (Mixed Arable and Livestock)

A 500-hectare farm with a central farmhouse, outlying barns, fields extending
2–3 km in each direction, and a river valley at the property boundary.

**Challenge:** Sensors in low-lying fields near the river are 3 km from the
farmhouse with a ridge blocking line-of-sight. A weather station on a
north-facing hillside has intermittent connectivity.

**Deployment:**

```text
                [WS-4] weather station (hilltop)
                              |
                           direct
                             |
[SM-1] soil ──direct──> [GATEWAY] farmhouse
[SM-2] soil ──direct──/      |
[WL-1] water ─direct──/      |
                             |
                         direct
                             |
                    [HOP-A] barn roof (solar powered)
                             |
                         1 relay
                         /       \
               [SM-3] soil    [SM-4] soil      (low field, behind ridge)
               [WS-5] weather station          (river valley)
               [WL-2] water level              (river gauge)
```

Configuration: 1 gateway, 1 relay, 8 sensors. Relay A sits on a barn roof at
mid-elevation with clear line-of-sight to both the gateway and the river valley.
Sensors SM-3, SM-4, WS-5, and WL-2 transmit normally. Relay A hears them (they
cannot reach the gateway directly), wraps their data in FORWARD messages, and
sends upstream to the gateway. Total relay load on Relay A: 4 sensors × ~0.1
packets/sec = ~0.4 FORWARD packets/sec. Well within capacity.

**Seasonal variation:** In summer, tree canopy growth may degrade Relay A's link
to WS-5. If WS-5's data becomes intermittent, deploy Relay B near the river to
provide a 2-relay path: WS-5 → Relay B → Relay A → Gateway. Relay B
automatically integrates — it hears Relay A's rebroadcast beacon (cost=1), sets
itself as cost=2, and begins forwarding.

#### G.2 Forest Research Station

A 2000-hectare managed forest with environmental sensors distributed along
trails and watercourses. Dense canopy limits per-relay range to 500m–1.5km. A
research cabin at the forest edge serves as the gateway location, with a second
gateway at a fire lookout tower 4 km away.

**Challenge:** Sensors deep in the forest are 3–5 km from either gateway with no
line-of-sight. Canopy attenuation is severe.

**Deployment:**

```text
[GATEWAY-1] research cabin          [GATEWAY-2] fire tower
      |                                    |
   direct                              direct
      |                                    |
  [HOP-1] ridge clearing              [HOP-4] trail junction
      |                                    |
  1 relay from GW-1                     1 relay from GW-2
      |                                    |
  [HOP-2] stream crossing             [HOP-5] canopy gap
      |                                    |
  2 relays from GW-1                    2 relays from GW-2
      |                                    |
  [SM-1..4] soil sensors              [ENV-1..3] environment
  [WL-1] water level                  [WS-1] weather station
      |
  [HOP-3] deep forest
      |
  3 relays from GW-1
      |
  [SM-5..8] deep soil sensors
  [AQ-1] air quality
```

Configuration: 2 gateways, 5 relays, ~16 sensors. Maximum depth is 3 relays.
Relays are placed at natural clearings, ridge lines, trail junctions, and stream
crossings where canopy gaps improve radio propagation.

**Redundancy:** HOP-2 can hear beacons from both GW-1 (via HOP-1, cost=2) and
GW-2 (via HOP-4 and HOP-5, cost=3). It normally routes via GW-1 (lower cost). If
HOP-1 fails, HOP-2 receives no beacons from GW-1's tree, times out after 3
rounds, and adopts GW-2's tree via HOP-5 at cost=3. Sensors SM-1..4 and WL-1
continue to operate without interruption — they are unaware of the topology
change.

#### G.3 Considerations for Moving Sensors

If a sensor is mounted on a vehicle (e.g. a tractor, livestock tracker, or
patrol vehicle) that moves between coverage areas, the protocol handles this
naturally because sensors are not mesh-aware.

**How it works:** The vehicle-mounted sensor transmits periodically as always.
As it moves, different relays (or the gateway directly) hear its transmissions.
Whichever relay hears the packet forwards it. If multiple relays hear it,
Trickle suppression ensures only one forwards. As the vehicle moves out of one
relay's range and into another's, forwarding seamlessly transitions.

**What works well:** Slow-moving vehicles (tractors, livestock) that spend
minutes to hours within each relay's coverage area. The sensor's transmission
interval (5–15 seconds) means multiple packets are sent during each coverage
window.

**What works less well:** Fast-moving vehicles passing through a relay's range
in seconds. If the sensor's transmission interval is longer than the transit
time through coverage, packets may be missed during the transition between
nodes. This is inherent to any non-continuous transmission scheme and is not
specific to the mesh protocol.

**The sensor firmware needs no changes.** The mesh adapts to the sensor's
location in real time. The gateway sees the same station_id and sequence numbers
regardless of which relay forwarded the data. Duplicate suppression handles
cases where the sensor is within range of multiple relays simultaneously.

### H. Protocol Version History

| Version      | Description                                                                                                                                                         |
| ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| v1           | Initial mesh protocol. BEACON, FORWARD, ACK, ROUTE_ERROR, NEIGHBOUR_REPORT. Gradient-based routing with single parent selection and relay-by-relay acknowledgement. |
| v2 (planned) | Adds PING/PONG for gateway-initiated reachability testing. Requires downstream routing capability at relays.                                                        |

### I. Reserved Identifiers

| Identifier   | Value     | Meaning                           |
| ------------ | --------- | --------------------------------- |
| variant_id   | 0x0F (15) | Mesh control packet               |
| ctrl_type    | 0x0–0x6   | Defined mesh packet types         |
| ctrl_type    | 0x7–0xF   | Reserved for future use           |
| parent_id    | 0xFFF     | Orphaned (no parent)              |
| station_id   | 0x000     | Reserved (do not assign to nodes) |
| reason codes | 0x3–0xF   | Reserved for future use           |

### J. Future Considerations

#### J.1 Cross-Gateway Duplicate Suppression

In multi-gateway deployments, a sensor positioned between two gateways (or a
relay node with paths to both) may deliver the same packet to multiple gateways.
Each gateway performs local dedup correctly, but the upstream system (database,
MQTT broker, dashboard) receives the same measurement data twice from different
gateway sources.

**Lightweight solution: UDP dedup broadcast.** Each gateway UDP-broadcasts a
compact dedup notification on the local network whenever it processes a sensor
packet. The notification contains only the dedup key — no measurement data:

```text
[gateway_id]        2 bytes     (12-bit station_id of the broadcasting gateway)
[num_entries]       1 byte      (number of dedup tuples in this batch, 1–32)
[entries...]        4 bytes each:
    [station_id]    2 bytes     (12-bit origin sensor, zero-padded to 16 bits)
    [sequence]      2 bytes     (16-bit origin sequence)
```

Maximum batch: 32 entries × 4 bytes + 3 byte header = 131 bytes per UDP
datagram.

On receipt, other gateways add these tuples to their local dedup ring. If a
subsequent FORWARD or direct sensor packet arrives with a station_id and
sequence already in the ring (whether from local receive or cross-gateway
notification), it is suppressed.

**Timing:** On a LAN, UDP broadcast latency is under 1ms. LoRa packet
transmission plus relay backoff is typically 200–1000ms. This means the
cross-gateway notification almost always arrives before the second copy of the
sensor data, achieving reliable suppression. In the rare case where two gateways
receive the same packet within 1ms of each other (both heard the sensor
directly), both may process it. This is acceptable — the upstream system can
perform its own dedup on {station_id, sequence} as a final safety net.

**Implementation:** This is entirely optional. A deployment with a single
gateway has no need for it. A multi-gateway deployment works correctly without
it — the upstream system simply sees occasional duplicates. The UDP broadcast
layer can be added to gateways independently of the mesh protocol and requires
no changes to relays or sensors.

**Alternative approaches:**

- **Shared MQTT topic** — gateways publish dedup tuples to a topic such as
  `iotdata/mesh/dedup/{gateway_id}`. Other gateways subscribe. Adds dependency
  on the MQTT broker being available but piggybacks on infrastructure that
  likely already exists for sensor data delivery.
- **Upstream dedup only** — skip gateway-to-gateway coordination entirely. The
  database or ingestion layer deduplicates on {station_id, sequence,
  time_window}. Simplest to implement, slightly higher upstream load from
  duplicate records.
- **Station-id range assignment** — the operator assigns non-overlapping
  station_id ranges to gateways. A gateway only processes packets from its
  assigned range. Simple but inflexible — a sensor that moves or a relay that
  reroutes to a different gateway may fall outside the assigned range. Not
  recommended for dynamic mesh deployments.

#### J.2 Potential Additional Control Packet Types

The ctrl_type field has 10 unused values (0x7–0xF, plus 0x5–0x6 reserved for
PING/PONG v2). Future protocol revisions may define additional packet types. The
following have been identified as candidates:

**CONFIG_PUSH (candidate: 0x7)** — Gateway pushes configuration parameters to a
specific relay. Routed downstream like PING. Enables remote adjustment of beacon
intervals, transmit power, parent selection thresholds, and reporting frequency
without physical access to the node.

Possible payload: target_station(12), config_key(8), config_value(16). Config
keys could include:

| Key  | Meaning                               | Value range         |
| ---- | ------------------------------------- | ------------------- |
| 0x01 | Beacon rebroadcast interval (seconds) | 10–600              |
| 0x02 | Forward retry count                   | 0–15                |
| 0x03 | Forward ACK timeout (ms / 100)        | 1–50 (100ms–5000ms) |
| 0x04 | Parent timeout (missed beacon rounds) | 1–15                |
| 0x05 | Neighbour report interval (minutes)   | 1–60                |
| 0x06 | Transmit power level                  | Module-specific     |
| 0x07 | Force rejoin (clear routing state)    | 1 = trigger         |

**CONFIG_ACK (candidate: 0x8)** — Target node acknowledges receipt of
CONFIG_PUSH. Routes upstream. Confirms the configuration was applied.

**PATH_TRACE (candidate: 0x9)** — Diagnostic packet that records the station_id
of every relay it traverses, building a full path trace from sensor to gateway.
A relay appends its own station_id (2 bytes) to the payload before forwarding.
The gateway receives a complete ordered list of the relay chain.

Possible payload: origin_station(12), ttl(8), relay_count(8), then relay_count ×
station_id(12) entries packed sequentially. Maximum path of 15 relays = 5 + 1 +
1 + 23 = 30 bytes. This would be triggered by the gateway wrapping a specific
sensor's next FORWARD in a PATH_TRACE envelope, or by a relay when it detects a
new sensor (first time seeing a station_id).

**NETWORK_RESET (candidate: 0xA)** — Gateway broadcasts a command for all relays
to flush routing state and re-discover the topology from scratch. Nuclear option
for when the mesh becomes wedged in a suboptimal configuration. Payload: just a
confirmation nonce to prevent accidental triggering.

**TIME_SYNC (candidate: 0xB)** — If a future deployment requires coordinated
sleep windows or TDMA-style channel access, a dedicated time synchronisation
packet could carry a high-resolution timestamp from the gateway. Relays would
estimate propagation delay from relay count and adjust their local clocks.
However, for the current protocol's CSMA-based channel access, this is
unnecessary.

**GROUP_FORWARD (candidate: 0xC)** — Aggregation packet where a relay bundles
multiple small sensor packets into a single transmission to reduce per-packet
overhead and ACK traffic. Payload: num_packets(6), then concatenated inner
packets with 1-byte length prefixes. Most useful in dense deployments where a
single relay forwards for many sensors. Trade-off: increases single-packet
airtime and failure blast radius (losing one aggregated transmission loses
multiple sensor readings).

#### J.3 Extended Neighbour Metrics

The current NEIGHBOUR_REPORT carries cost, RSSI, and station_id per neighbour.
Future revisions may extend neighbour entries with additional quality metrics:

- **Packet delivery ratio (PDR)** — percentage of expected packets actually
  received from this neighbour over a window. 4 bits (16 levels, ~6%
  granularity) would suffice. Better parent selection metric than instantaneous
  RSSI.
- **Asymmetric link detection** — a flag indicating whether the neighbour has
  acknowledged hearing this node. A neighbour with good inbound RSSI but no
  evidence of hearing this node's transmissions is a poor parent candidate
  (asymmetric link, common with differing antenna heights or transmit powers).
- **Neighbour role** — 2 bits indicating whether the neighbour is a gateway,
  relay, or sensor. Currently inferred from behaviour (gateways originate
  beacons, relays rebroadcast, sensors don't participate), but an explicit role
  field simplifies topology visualisation.

These extensions would increase neighbour entry size from 3 to 4 bytes. The
num_neighbours field (6 bits, max 63) and LoRa payload limits (222 bytes at SF7)
would support up to 53 extended entries — still more than sufficient.

#### J.4 Security Considerations

The v1 protocol includes no authentication or encryption. For agricultural and
environmental monitoring deployments, the threat model is typically low — the
data has no commercial sensitivity and the radio channel is shared ISM spectrum.
However, for deployments where integrity matters:

**Packet authentication** — a shared 4-byte key (pre-configured on all mesh
nodes and gateways) could be used to compute a 2-byte truncated HMAC appended to
every mesh control packet. This prevents rogue nodes from injecting false
beacons or FORWARD packets. The key would be distributed during provisioning and
is not expected to change frequently. 2 bytes provides 65536 possible values —
sufficient to prevent casual injection, though not secure against a determined
attacker with radio access.

**Replay protection** — the existing sequence number provides partial replay
protection. A replayed packet with a previously-seen sequence number is caught
by dedup. However, a replayed BEACON with a valid-looking generation could
disrupt routing. Binding the HMAC to the generation counter and gateway_id
prevents this.

**Encryption** — encrypting the inner packet within FORWARD would prevent
eavesdropping on sensor data. AES-128 in CTR mode adds zero overhead to the
packet size (ciphertext is same length as plaintext) and requires only a shared
key and a nonce derivable from {station_id, sequence}. However, this adds
computational cost at every relay (decrypt to verify, re-encrypt to forward)
which is unnecessary if the relay treats the inner packet as opaque — the relay
does not need to read the inner packet's contents, so the inner packet can
remain encrypted end-to-end between sensor and gateway with no relay
involvement. The sensor would encrypt before transmission, the gateway would
decrypt after receipt, and relay nodes would forward the encrypted blob
unchanged.

#### J.5 Power Management for Relay Nodes

Relays must listen continuously, which prevents the aggressive sleep modes used
by transmit-only sensors. Typical listen-mode current for an SX1262 LoRa module
is 5–8 mA; combined with an ESP32-C3 in active mode (~25–50 mA average with WiFi
disabled), total system draw is 30–60 mA.

**Solar viability:** At 12V with a 30mA average draw, daily consumption is
~8.6Wh. A 10W solar panel in temperate latitudes produces 20–40Wh/day (seasonal
variation). A 10Ah 12V battery provides ~3–4 days of autonomy in complete cloud
cover. This is viable for most deployments.

**Duty-cycled listening (future optimisation):** If beacon intervals are
synchronised, relays could sleep between expected beacon windows and wake only
during scheduled listen slots. This requires the TIME_SYNC mechanism described
in J.2 and adds complexity to the parent selection logic (must account for clock
drift during sleep). Not recommended for v1 but noted as a path to lower power
consumption if needed.

**Low-power relay mode:** A relay with no downstream children (leaf relay — only
forwarding for sensors it directly hears) could adopt a semi-synchronised
schedule: listen for a window after each expected sensor transmission, forward
any received packets, then sleep until the next expected window. This requires
the relay to learn sensor transmission intervals through observation, which is
feasible since sensors typically transmit at regular (if slightly randomised)
intervals.

#### J.6 Network Capacity Planning

The mesh protocol's capacity is fundamentally limited by shared airtime on the
LoRa channel. All nodes — sensors, relays, and gateways — share a single
frequency and spreading factor (assuming no frequency planning).

**Single-channel capacity at SF7/125kHz:**

A 30-byte LoRa packet at SF7/125kHz takes approximately 50ms of airtime. At 1%
duty cycle (EU 868MHz regulatory limit), one device can transmit 20 packets per
100 seconds, or 0.2 packets/second sustained.

In a mesh deployment, the bottleneck is the gateway's immediate neighbourhood —
all forwarded packets must pass through the last relay to the gateway. If 10
relay paths converge on a single relay-1 node, that node must forward 10× the
traffic of any individual sensor. With 100 sensors at 10-second intervals
producing 10 packets/second aggregate, the relay-1 node must forward all 10 plus
transmit ACKs, consuming ~1 second of airtime per second — impossible under any
duty cycle regulation.

**Mitigation strategies:**

- **Multiple relay-1 nodes** — deploy 2–4 relays within direct range of the
  gateway, each serving a different angular sector or downstream branch.
  Distributes the forwarding load.
- **Multiple gateways** — each gateway serves a subset of the network.
  Cross-gateway dedup (J.1) prevents duplicate processing.
- **Frequency planning** — assign different LoRa channels to different branches
  of the mesh. Requires relays to manage multiple frequencies, adding hardware
  or scheduling complexity.
- **Adaptive transmission intervals** — sensors or the CONFIG_PUSH mechanism
  could adjust transmission rates based on network load. Sensors deeper in the
  mesh (more relay) could transmit less frequently.
- **Aggregation** — the GROUP_FORWARD packet type (J.2) could reduce per-packet
  overhead and ACK count at the cost of increased single-transmission airtime.

**Rule of thumb:** For a single LoRa channel at SF7 with 1% duty cycle, plan for
no more than 30–50 sensors per gateway, with no more than 20 forwarded through
any single relay-1 relay. Scale beyond this by adding gateways, not by deepening
the mesh.

#### J.7 Interoperability and Versioning

The protocol currently has no version negotiation mechanism. All mesh nodes are
expected to run the same protocol version. For future-proofing:

- **Reserved ctrl_types (0x7–0xF)** should be silently discarded by nodes that
  do not recognise them. This allows new packet types to be deployed
  incrementally — gateways can be updated first, followed by relays, without
  causing errors on nodes still running older firmware.
- **Reserved flag bits** in the BEACON packet provide a forward-compatible
  extension point. A new capability can be signalled by setting a flag bit.
  Older nodes ignore unknown flags but continue to process the beacon normally.
- **The FORWARD packet is inherently version-agnostic** — relay nodes do not
  interpret the inner payload, so changes to iotdata sensor variants, field
  definitions, or encoding formats require no mesh firmware updates.

If a formal version field becomes necessary, it could be encoded in the BEACON's
reserved flag bits (e.g. flags bits 2–3 as a 2-bit protocol version, supporting
4 versions). Alternatively, a VERSION_ANNOUNCE packet type could be defined
using one of the reserved ctrl_type slots.

---

_This document and the reference implementation are maintained at
[https://libiotdata.org]._
