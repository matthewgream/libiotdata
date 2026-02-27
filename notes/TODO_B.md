# iotdata — Sensor Field Types TODO

Working notes for expanding field type coverage. This document captures the
architecture for a layered field type system and catalogues candidate
measurements.

---

## Architecture

### Three-Layer Field Type Hierarchy

Field types are organised in three layers. Higher layers build on lower layers.

**Layer 1 — Generic / Flexible** Raw bit-width types with no protocol-defined
semantics. The variant label provides all meaning. No quantisation, no units, no
JSON structure — just N bits in the packet.

**Layer 2 — Unit-Based** Types that encode a specific physical unit with a
defined quantisation but no specific measurement context. "Distance in cm" is
unit-based; "snow depth" is specific. Unit-based types can be reused across many
variant labels.

**Layer 3 — Specific / Domain** Types with fully defined semantics: measurement
context, quantisation, range, and JSON structure. Some are newly defined; others
are built upon Layer 1 or Layer 2 types with additional protocol-level meaning
(bundles, specific ranges, specific JSON output).

Existing fields (Temperature, Pressure, Wind, etc.) are Layer 3.

### Presence Bit Semantics: Absolute vs Rate of Change

A presence bit in the variant table can be configured as either **absolute** or
**rate of change** for the same underlying field encoding. The protocol does not
need separate field types for absolute temperature and temperature rate — the
variant definition determines interpretation.

This gives three deployment patterns:

**(a) Fixed interpretation.** The variant defines a field as always absolute or
always rate. Simplest case, no overhead.

```
Field S2: TEMPERATURE (absolute)      — always transmits absolute temp
```

**(b) Dynamic selector.** The variant allocates two presence slots: a 1-bit flag
(BIT1) indicating interpretation, followed by the measurement field. The flag
costs 1 bit when present, but allows the sensor to dynamically choose absolute
or rate per packet.

```
Field S2: BIT1 label="temp_is_rate"    — 0 = absolute, 1 = rate of change
Field S3: TEMPERATURE label="temp"     — value interpreted per S2
```

**(c) Separate fields.** The variant allocates separate slots for absolute and
rate, each independently present/absent.

```
Field S2: TEMPERATURE label="temperature"
Field S3: TEMPERATURE label="temperature_rate"
```

Additionally, a presence bit with no content (zero-width field) is permitted.
The mere presence of the bit in the packet carries 1 bit of information. This is
the BIT1 type at Layer 1 — but could also be a **NULL / flag-only** type with
zero data bits, where the presence bit itself IS the data.

```
Field S4: NULL label="motion_detected"  — 0 bits of data; present = true
```

### Expanded Variants: `_expanded` Naming Convention

Where a measurement needs a wider range or finer resolution than the standard
encoding, a second field type is defined with the suffix `_expanded`. Both
encodings coexist in the protocol; the variant chooses which to use.

```
TEMPERATURE          — 9 bits, -40 to +80°C, 0.25°C
TEMPERATURE_EXPANDED — 12 bits, -60 to +150°C, 0.05°C (or similar)

PRESSURE             — 8 bits, 850–1105 hPa, 1 hPa
PRESSURE_EXPANDED    — 10 bits, 300–1100 hPa, ~0.8 hPa

DEPTH                — 10 bits, 0–1023 cm, 1 cm
DEPTH_EXPANDED       — 16 bits, 0–65535 mm, 1 mm
```

The `_expanded` type is a separate field type ID. The variant picks one or the
other — they never coexist for the same measurement in the same variant. This
avoids the complexity of in-band signalling while giving deployments the choice
between compact and precise.

Convention: the base type optimises for bit efficiency in the common case. The
`_expanded` type covers edge cases (high altitude pressure, extreme
temperatures, precision distance) at the cost of more bits.

---

## Layer 1 — Generic / Flexible Field Types

No protocol-defined units or quantisation. Variant label gives meaning.

| Type   | Bits | Range            | Description                                            |
| ------ | ---- | ---------------- | ------------------------------------------------------ |
| NULL   | 0    | presence only    | Zero-width flag. Presence bit = true, absence = false. |
| BIT1   | 1    | 0/1              | Boolean. Door open/closed, alarm, mode select.         |
| UINT4  | 4    | 0–15             | Small enum, category, state machine value.             |
| UINT8  | 8    | 0–255            | Generic unsigned byte.                                 |
| UINT10 | 10   | 0–1023           | Medium unsigned.                                       |
| UINT16 | 16   | 0–65535          | Large unsigned, raw ADC.                               |
| INT8   | 8    | -128 to +127     | Signed byte.                                           |
| INT16  | 16   | -32768 to +32767 | Signed word.                                           |
| RAW8   | 8    | opaque           | Opaque byte, no interpretation.                        |
| RAW16  | 16   | opaque           | Opaque 2 bytes.                                        |
| RAW24  | 24   | opaque           | Opaque 3 bytes.                                        |
| RAW32  | 32   | opaque           | Opaque 4 bytes.                                        |

Notes:

- UINT vs RAW: UINT is numeric (JSON outputs a number), RAW is opaque (JSON
  outputs hex or base64). Semantics differ at the gateway.
- Variant map may optionally carry scale/offset hints for generic types in
  future (J.27 variant advertisement), but for now interpretation is
  out-of-band.

---

## Layer 2 — Unit-Based Field Types

Defined quantisation for a physical unit. Reusable across many variant labels.
No domain-specific semantics — "what it measures" comes from the variant label.

### Length / Distance

| Type                 | Bits | Range      | Resolution | Units |
| -------------------- | ---- | ---------- | ---------- | ----- |
| DISTANCE_CM          | 10   | 0–1023 cm  | 1 cm       | cm    |
| DISTANCE_CM_EXPANDED | 16   | 0–65535 cm | 1 cm       | cm    |
| DISTANCE_MM          | 16   | 0–65535 mm | 1 mm       | mm    |
| DISTANCE_MM_SHORT    | 10   | 0–1023 mm  | 1 mm       | mm    |

Use cases: snow depth, water level, soil depth, ultrasonic range, lidar, object
distance, ice thickness, clearance.

Note: existing DEPTH (10 bits, 0–1023 cm) becomes an alias for DISTANCE_CM.

### Percentage

| Type            | Bits | Range    | Resolution | Units |
| --------------- | ---- | -------- | ---------- | ----- |
| PERCENTAGE      | 7    | 0–100%   | 1%         | %     |
| PERCENTAGE_FINE | 10   | 0–100.0% | 0.1%       | %     |

Use cases: soil moisture, leaf wetness, dissolved oxygen saturation, humidity
(existing HUMIDITY is an alias), battery (different encoding though), signal
quality, generic fill level.

### Ratio

| Type    | Bits | Range         | Resolution | Units |
| ------- | ---- | ------------- | ---------- | ----- |
| RATIO8  | 8    | 0.000–1.000   | ~1/255     | ratio |
| RATIO16 | 16   | 0.0000–1.0000 | ~1/65535   | ratio |

Use cases: NDVI, duty cycle, confidence score, reflectance, normalised index.

### Temperature

| Type                 | Bits | Range         | Resolution | Units |
| -------------------- | ---- | ------------- | ---------- | ----- |
| TEMPERATURE          | 9    | -40 to +80°C  | 0.25°C     | °C    |
| TEMPERATURE_EXPANDED | 12   | -60 to +150°C | ~0.05°C    | °C    |

Existing TEMPERATURE unchanged. TEMPERATURE_EXPANDED for industrial, desert,
high-altitude, or precision applications.

### Pressure

| Type              | Bits | Range        | Resolution | Units |
| ----------------- | ---- | ------------ | ---------- | ----- |
| PRESSURE          | 8    | 850–1105 hPa | 1 hPa      | hPa   |
| PRESSURE_EXPANDED | 10   | 300–1100 hPa | ~0.8 hPa   | hPa   |
| PRESSURE_KPA      | 14   | 0–10,000 kPa | ~0.6 kPa   | kPa   |

PRESSURE_EXPANDED addresses J.1 (altitude range issue). PRESSURE_KPA is
non-atmospheric: pipe, hydraulic, tyre, water pressure.

### Electrical

| Type                | Bits | Range          | Resolution | Units                 |
| ------------------- | ---- | -------------- | ---------- | --------------------- |
| VOLTAGE_MV          | 16   | 0–65535 mV     | 1 mV       | mV                    |
| CURRENT_MA          | 16   | 0–65535 mA     | 1 mA       | mA                    |
| POWER_W             | 16   | 0–65535 W      | 1 W        | W                     |
| ENERGY_WH           | 16   | 0–65535 Wh     | 1 Wh       | Wh (wrapping counter) |
| CONDUCTIVITY        | 16   | 0–65535 µS/cm  | 1 µS/cm    | µS/cm                 |
| CONDUCTIVITY_COARSE | 10   | 0–51,150 µS/cm | 50 µS/cm   | µS/cm                 |

Notes:

- CONDUCTIVITY serves both soil EC and water EC — variant label distinguishes.
- Two resolutions: fine (1 µS/cm, 16 bits, freshwater precision) and coarse (50
  µS/cm, 10 bits, covers brackish/seawater in fewer bits).

### Volume / Flow

| Type              | Bits | Range         | Resolution | Units                |
| ----------------- | ---- | ------------- | ---------- | -------------------- |
| FLOW_LPM          | 10   | 0–1023 L/min  | 1 L/min    | L/min                |
| FLOW_LPM_EXPANDED | 16   | 0–65535 L/min | 1 L/min    | L/min                |
| VOLUME_L          | 16   | 0–65535 L     | 1 L        | L (wrapping counter) |

### Weight / Force

| Type              | Bits | Range          | Resolution | Units            |
| ----------------- | ---- | -------------- | ---------- | ---------------- |
| WEIGHT_G          | 16   | 0–65535 g      | 1 g        | g                |
| WEIGHT_G_EXPANDED | 24   | 0–16,777,215 g | 1 g        | g (~16.7 tonnes) |
| FORCE_N           | 16   | 0–65535 N      | 1 N        | N                |

### Speed

| Type              | Bits | Range       | Resolution | Units |
| ----------------- | ---- | ----------- | ---------- | ----- |
| SPEED_MS          | 7    | 0–63.5 m/s  | 0.5 m/s    | m/s   |
| SPEED_MS_EXPANDED | 10   | 0–127.5 m/s | ~0.125 m/s | m/s   |

Existing wind speed is an alias for SPEED_MS. \_EXPANDED addresses J.2.

### Angle

| Type           | Bits | Range         | Resolution | Units   |
| -------------- | ---- | ------------- | ---------- | ------- |
| ANGLE_360      | 8    | 0–355°        | ~1.41°     | degrees |
| ANGLE_360_FINE | 10   | 0–359.6°      | ~0.35°     | degrees |
| ANGLE_SIGNED   | 10   | -180 to +180° | ~0.35°     | degrees |
| TILT           | 8    | -90 to +90°   | ~0.7°      | degrees |

ANGLE_360 = existing wind direction encoding. TILT for inclinometers.

### Counter

| Type      | Bits | Range   | Resolution | Units          |
| --------- | ---- | ------- | ---------- | -------------- |
| COUNTER8  | 8    | 0–255   | 1          | wrapping count |
| COUNTER16 | 16   | 0–65535 | 1          | wrapping count |

Semantically distinct from UINT — counters wrap, measurements don't. JSON should
note wrapping semantics. Use cases: rain tips, flow pulses, lightning strikes,
event counts, energy totaliser.

### Sound

| Type               | Bits | Range     | Resolution | Units |
| ------------------ | ---- | --------- | ---------- | ----- |
| SOUND_DBA          | 8    | 0–140 dBA | ~0.55 dBA  | dBA   |
| SOUND_DBA_EXPANDED | 10   | 0–140 dBA | ~0.14 dBA  | dBA   |

### Illuminance

| Type    | Bits | Range          | Resolution | Units           |
| ------- | ---- | -------------- | ---------- | --------------- |
| LUX     | 16   | 0–65535 lux    | 1 lux      | lux             |
| LUX_LOG | 8    | ~1–120,000 lux | ~12%/step  | lux (log scale) |

LUX_LOG covers the full outdoor range in 8 bits using logarithmic encoding.
First log-scale type — sets precedent for other skewed-distribution fields.

### Concentration (generic)

| Type | Bits | Range         | Resolution | Units |
| ---- | ---- | ------------- | ---------- | ----- |
| PPM  | 16   | 0–65535 ppm   | 1 ppm      | ppm   |
| PPB  | 16   | 0–65535 ppb   | 1 ppb      | ppb   |
| UGM3 | 16   | 0–65535 µg/m³ | 1 µg/m³    | µg/m³ |

Generic concentration types for gas/particulate measurements not covered by the
existing AQ fields.

### Magnetic

| Type        | Bits | Range           | Resolution | Units       |
| ----------- | ---- | --------------- | ---------- | ----------- |
| MAGNETIC_UT | 10   | -512 to +511 µT | 1 µT       | µT (signed) |

### Acceleration

| Type             | Bits | Range            | Resolution | Units      |
| ---------------- | ---- | ---------------- | ---------- | ---------- |
| ACCEL_G          | 10   | -16.0 to +15.9 g | ~0.03 g    | g (signed) |
| ACCEL_G_EXPANDED | 16   | -32.0 to +32.0 g | ~0.001 g   | g (signed) |

### RPM

| Type | Bits | Range       | Resolution | Units |
| ---- | ---- | ----------- | ---------- | ----- |
| RPM  | 14   | 0–16383 RPM | 1 RPM      | RPM   |

---

## Layer 3 — Specific / Domain Field Types

Fully defined measurement context. May be built on Layer 1/2 types or have
unique encodings. These are the types with protocol-defined JSON output
structure and semantic meaning.

### Existing Layer 3 types (no changes needed unless noted)

- Battery (6 bits) — level + charging flag
- Link (6 bits) — RSSI + SNR
- Environment bundle (24 bits) — temp + pressure + humidity
- Wind bundle (22 bits) — speed + direction + gust
- Rain bundle (12 bits) — rate + size
- Solar bundle (14 bits) — irradiance + UV
- Clouds (4 bits) — okta
- Air Quality bundle (21+ bits) — index + PM + gas
- Radiation bundle (28 bits) — CPM + dose
- Depth (10 bits) — alias for DISTANCE_CM
- Position (48 bits) — lat + lon
- Datetime (24 bits) — year-relative ticks
- Flags (8 bits) — deployment bitmask
- Image (variable) — pixel data

### New standalone types (split from existing bundles — J.15)

- **Standalone Irradiance** — 10 bits, 0–1023 W/m², same as solar bundle
  component
- **Standalone UV Index** — 4 bits, 0–15, same as solar bundle component

### New specific types

| Type                     | Built On            | Bits   | Range                                   | Resolution | Notes                                     |
| ------------------------ | ------------------- | ------ | --------------------------------------- | ---------- | ----------------------------------------- |
| Soil Moisture            | PERCENTAGE          | 7      | 0–100% VWC                              | 1%         | Variant label distinguishes from humidity |
| Soil EC                  | CONDUCTIVITY        | 16     | 0–65535 µS/cm                           | 1 µS/cm    | Or CONDUCTIVITY_COARSE for 10 bits        |
| Soil Water Potential     | —                   | 10     | -1023 to 0 kPa                          | 1 kPa      | Matric potential, always negative         |
| Soil Bundle              | —                   | 33     | VWC(7) + EC(16) + Temp(9) + Charging(1) | mixed      | Teros 12 pattern                          |
| Water pH                 | —                   | 8      | 0–14.0 pH                               | ~0.055     | 255 steps over 14.0                       |
| Water pH (expanded)      | —                   | 10     | 0–14.00 pH                              | ~0.014     | Lab-grade resolution                      |
| Water DO                 | —                   | 8      | 0–25.5 mg/L                             | 0.1 mg/L   | Dissolved oxygen                          |
| Water Turbidity          | LUX_LOG-style       | 8      | ~0.1–10,000 NTU                         | ~12%/step  | Log-scale, right-skewed                   |
| Water ORP                | —                   | 10     | -1000 to +1000 mV                       | ~2 mV      | Redox potential                           |
| Water Quality Bundle     | —                   | 34     | pH(8) + EC(10) + DO(8) + Temp(9)        | mixed      | Multi-parameter sonde                     |
| Temperature Rate         | INT8                | 8      | ±12.7 °C/min                            | 0.1 °C/min | Fire/frost detection                      |
| Pressure Rate            | INT8                | 8      | ±12.7 hPa/hr                            | 0.1 hPa/hr | Storm prediction                          |
| Leaf Wetness             | UINT4 or PERCENTAGE | 4 or 7 | 0–15 or 0–100%                          | varies     | Categorical or continuous                 |
| PAR                      | UINT16              | 16     | 0–2500 µmol/m²/s                        | —          | Scale TBD                                 |
| Lightning Distance       | UINT8               | 8      | 0–63 km                                 | ~0.25 km   | AS3935 range                              |
| Lightning Count          | COUNTER8            | 8      | 0–255                                   | 1          | Strikes per period                        |
| Lightning Bundle         | —                   | 16     | Distance(8) + Count(8)                  | mixed      |                                           |
| Sound Level              | SOUND_DBA           | 8      | 0–140 dBA                               | ~0.55 dBA  | Environmental noise                       |
| Colour RGB               | —                   | 24     | 3×8 bits, 0–255 per channel             | 1          | TCS34725 etc.                             |
| Non-Atmospheric Pressure | PRESSURE_KPA        | 14     | 0–10,000 kPa                            | ~0.6 kPa   | Pipe, hydraulic, tyre                     |

### New gas species (extend existing AQ Gas or standalone)

| Gas                    | Units | Typical Range | Priority                      |
| ---------------------- | ----- | ------------- | ----------------------------- |
| Methane (CH₄)          | ppm   | 0–10,000      | High (agriculture, landfill)  |
| Ammonia (NH₃)          | ppm   | 0–500         | High (livestock)              |
| Hydrogen sulfide (H₂S) | ppm   | 0–100         | Medium (wastewater, mining)   |
| Sulfur dioxide (SO₂)   | ppb   | 0–2000        | Medium (volcanic, industrial) |
| Oxygen concentration   | %     | 0–25          | Medium (confined space)       |
| Radon (Rn)             | Bq/m³ | 0–10,000      | Low (indoor air quality)      |

Decision needed: extend AQ Gas mask bits, or define as standalone PPM/PPB types
with variant labels.

---

## Design Decisions (resolved or in-progress)

### Resolved

- **Rate of change:** Handled via presence bit semantics, not separate field
  types. Variant defines whether a field slot is absolute or rate. BIT1 flag
  field enables dynamic per-packet selection.

- **Expanded ranges:** `_expanded` suffix convention. Separate field type IDs.
  Variant picks one or the other. No in-band signalling.

- **Zero-width fields (NULL type):** Permitted. Presence bit alone carries the
  information. Useful for binary state flags with zero data overhead.

- **EC unification:** Single CONDUCTIVITY unit-based type at Layer 2. Variant
  label distinguishes soil vs water vs other. Two bit-width options (10 and 16)
  for range flexibility.

- **Colour:** RGB (24 bits) included. CMYK dropped — not a sensor output.

### Open

- **Log-scale encoding:** Proposed for turbidity and lux. Define a general
  log-scale encoding formula (e.g. `value = base^(q/steps) * min`) or per-field?
  Need to decide on base and step count.

- **Gas expansion strategy:** Extend AQ Gas mask from 8 to 16 bits (adds CH₄,
  NH₃, H₂S, SO₂, O₂, Rn to existing structure) vs define standalone gas types
  (simpler variants, but no bundle benefit).

- **Generic type metadata in variant map:** For Layer 1 types (UINT8 etc.),
  should the variant map carry scale/offset/unit metadata to enable automatic
  JSON output with correct units? Or is that purely out-of-band? Connects to
  J.27 (variant advertisement).

- **Bundle policy:** When to define a new bundle vs let deployments compose from
  individual types? Bundles save presence bits but are inflexible. Proposed
  rule: bundles only for sensor outputs that are always generated together
  (BME280 → environment, Teros 12 → soil).

- **Field type ID allocation:** Need to define the global ID numbering scheme.
  Layer 1 in one range, Layer 2 in another, Layer 3 in another? Or flat?

---

## Priority Tiers

### Tier 1 — Before v1.0

**Layer 1 generics:**

- NULL (0 bits)
- BIT1 (1 bit)
- UINT8, UINT16, INT8, INT16
- COUNTER16
- RAW8, RAW16

**Layer 2 units:**

- PERCENTAGE (7 bits)
- DISTANCE_CM_EXPANDED (16 bits)
- CONDUCTIVITY (16 bits)
- SPEED_MS_EXPANDED (10 bits)
- PRESSURE_EXPANDED (10 bits)
- TEMPERATURE_EXPANDED (12 bits)

**Layer 3 specifics:**

- Standalone Irradiance (split from Solar)
- Standalone UV Index (split from Solar)
- Soil Moisture
- Soil EC
- Soil Bundle
- Water pH
- Water EC (alias for CONDUCTIVITY with label)
- Water Dissolved Oxygen
- Temperature Rate of Change
- Pressure Rate of Change

### Tier 2 — v1.x

**Layer 1/2:**

- UINT4, UINT10, INT8
- RAW24, RAW32
- RATIO8
- SOUND_DBA
- LUX / LUX_LOG
- TILT
- ACCEL_G
- PRESSURE_KPA
- WEIGHT_G

**Layer 3:**

- Water Turbidity
- Water ORP
- Water Quality Bundle
- Soil Water Potential
- Leaf Wetness
- PAR
- Lightning Bundle
- Sound Level
- Colour RGB
- Non-Atmospheric Pressure

### Tier 3 — Future

- All remaining gas species
- Flow rate / volume totaliser
- NDVI, Chlorophyll-a, Dendrometer
- Vibration, RPM, Strain
- Magnetic field
- CMYK (if ever)
- Mains frequency
- All other \_EXPANDED variants not in Tier 1
- WEIGHT_G_EXPANDED, FLOW_LPM_EXPANDED, etc.
- RATIO16, ACCEL_G_EXPANDED, ANGLE_360_FINE, etc.

---

_Last updated: 2026-02-27_
