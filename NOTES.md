# iotdata — Sensor Field Type Architecture

Working reference for the iotdata field type system: architecture, domain
coverage, device constraints, and prioritisation.

---

## 0. Design Rationale & Epistemological Framing

This section captures the reasoning frameworks and external knowledge that
informed the organisation of this document. It is deliberately separated from
the actionable specification content in subsequent sections so that the "why we
think this way" is preserved without cluttering the "what we do."

### 0.1 Applicable Standards and Frameworks

Several established standards address sensor data modelling. None solve the
bit-packing and bandwidth-constrained encoding problem that iotdata targets, but
they validate the architectural shape and provide vocabulary.

| Framework                                   | Relevance to iotdata                                                                                                                                                                            |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| OGC Observations & Measurements (ISO 19156) | Separates *observable property* (what), *procedure* (sensor/how), *feature of interest* (soil, water body, air mass), and *result* (encoded value). Maps to Sections 2/3/4 of this document.    |
| QUDT Ontology                               | Distinguishes *quantity kind* (length, temperature — abstract) from *unit* (cm, °C — concrete). This is essentially the Layer 2 concept: encoding a physical unit without domain semantics.     |
| SenML (RFC 8428)                            | Lightweight sensor measurement format. Faces scale, sign, and packing problems similar to iotdata but solves them with JSON verbosity rather than bit efficiency.                               |
| CayenneLPP                                  | LoRaWAN-oriented compact encoding. iotdata replaces this with richer type semantics and variant-driven flexibility.                                                                             |
| IEEE 1451 TEDS                              | Self-describing transducer metadata embedded in the device. Connects to the variant-map metadata question (§2.13) — whether scale/offset/unit hints travel with the data.                       |
| W3C SSN/SOSA Ontology                       | Formalises observation types: measurement, count, truth/boolean, category. Maps onto the scale-type vocabulary (§2.2).                                                                          |

**Key takeaway:** The distinction between *quantity kind* and *encoding* is
fundamental and well-established. iotdata's three-layer model captures this.
What no existing standard addresses is the bit-packing problem under severe
bandwidth constraints — that is the novel contribution.

### 0.2 Organising Principles Applied

The following principles guided the restructuring of these notes:

**First-class treatment of recurring cross-cutting concerns.** Several issues
(log-scale encoding, signed vs unsigned, rate-of-change interpretation)
appeared repeatedly across domain-specific sections as ad-hoc observations.
These are not domain-specific — they are architectural properties. They have
been extracted and defined as vocabulary in Section 2 (Encoding Architecture).

**Separation of encoding from domain from device.** Three distinct questions:
(a) how do we encode a value? (b) what are we measuring? (c) what sensor
produces the data? Each question has its own section. Cross-references link
them. This prevents sensor-specific details from leaking into encoding
decisions, and domain-specific semantics from constraining generic types.

**Spatial, temporal, practical.** Within the encoding architecture (Section 2),
three orthogonal concerns structure the design. §2.2 (scale types and sign
domains) addresses the **spatial** axis — how a value is interpreted within its
measurement space. §2.4 (temporal interpretation) addresses the **temporal**
axis — how a value relates to time, whether as a raw instantaneous sample, a
statistical summary (average, min, max), an accumulation, or a derivative. §2.5
onward addresses **practical** encoding mechanics — bit-width trade-offs,
expansion/contraction, bundling, naming. These three concerns are independent:
any scale type can combine with any temporal mode and any bit-width strategy.

**Precision about what the protocol controls.** The protocol defines encoding
resolution (quantisation step size). It does not define sensor accuracy,
calibration state, or measurement methodology. These are device-layer concerns
(Section 4). Being explicit about this boundary prevents over-specification at
the protocol level and under-specification at the device level.

**Derivation awareness.** Some quantities in the catalogue are derivable from
others (salinity from EC+temperature, flow from depth via rating curve, TDS
from EC). The architecture should acknowledge which types are fundamental
(require encoding) and which are derivable (could be computed at the gateway).
This does not mean derived types are excluded — a sensor may transmit a derived
value directly — but the relationship should be visible.

**Prioritisation by leverage.** A Layer 2 type that unlocks multiple Layer 3
domains across common, affordable sensor deployments has higher priority than a
Layer 3 type serving a single niche. Section 5 applies this principle
explicitly.

### 0.3 Scope

Covered: environmental monitoring, agriculture, smart buildings, weather
stations, water quality, general IoT deployments using low-power, scalable
electronics.

Not covered: factory/heavy-industrial sensors, extreme environment sensors
(deep-sea, space, nuclear), medical-grade devices, high-frequency trading or
sub-millisecond timing.

---

## 1. Current State (Reference Baseline)

These fields are already defined in the iotdata protocol specification. No work
is needed except where issues are flagged.

| Field            | Bits     | Range                              | Resolution     | Notes                                            |
| ---------------- | -------- | ---------------------------------- | -------------- | ------------------------------------------------ |
| Battery          | 6        | 0–100%, charging flag              | ~3.2%          |                                                  |
| Link             | 6        | RSSI -120–-60 dBm, SNR -20–+10 dB  | 4 dBm / 10 dB  |                                                  |
| Temperature      | 9        | -40 to +80°C                       | 0.25°C         | Standalone + in Environment bundle               |
| Pressure (baro)  | 8        | 850–1105 hPa                       | 1 hPa          | **J.1: range too narrow, needs fix**             |
| Humidity         | 7        | 0–100%                             | 1%             | Standalone + in Environment bundle               |
| Environment      | 24       | Temp+Press+Humid bundle            | as above       |                                                  |
| Wind Speed       | 7        | 0–63.5 m/s                         | 0.5 m/s        | **J.2: max may be too low**                      |
| Wind Direction   | 8        | 0–355°                             | ~1.41°         |                                                  |
| Wind Gust        | 7        | 0–63.5 m/s                         | 0.5 m/s        |                                                  |
| Wind             | 22       | Speed+Dir+Gust bundle              | as above       |                                                  |
| Rain Rate        | 8        | 0–255 mm/hr                        | 1 mm/hr        |                                                  |
| Rain Size        | 4        | 0–6.0 mm                           | 0.25 mm        | **J.7: semantics undefined**                     |
| Rain             | 12       | Rate+Size bundle                   | as above       |                                                  |
| Solar Irradiance | 10       | 0–1023 W/m²                        | 1 W/m²         | **J.11: may need 11 bits; J.15: no standalone**  |
| UV Index         | 4        | 0–15                               | 1              | **J.8: max 15 may be low; J.15: no standalone**  |
| Solar            | 14       | Irradiance+UV bundle               | as above       |                                                  |
| Clouds           | 4        | 0–8 okta                           | 1 okta         |                                                  |
| AQ Index         | 9        | 0–500 AQI                          | 1              |                                                  |
| AQ PM            | 4–36     | 0–1275 µg/m³ per channel           | 5 µg/m³        | 4 channels: PM1, PM2.5, PM4, PM10                |
| AQ Gas           | 8–84     | Variable per gas                   | Variable       | VOC, NOx, CO₂, CO, HCHO, O₃ + 2 reserved         |
| Air Quality      | 21+      | Index+PM+Gas bundle                | as above       |                                                  |
| Radiation CPM    | 14       | 0–16383 CPM                        | 1 CPM          |                                                  |
| Radiation Dose   | 14       | 0–163.83 µSv/h                     | 0.01 µSv/h     |                                                  |
| Radiation        | 28       | CPM+Dose bundle                    | as above       |                                                  |
| Depth            | 10       | 0–1023 cm                          | 1 cm           | Generic: snow, water level, ice, soil depth      |
| Position         | 48       | Lat/lon ±90°/±180°                 | ~1.2 m         |                                                  |
| Datetime         | 24       | Year-relative, 5s ticks            | 5 s            |                                                  |
| Flags            | 8        | 8-bit bitmask                      | —              | Deployment-defined                               |
| Image            | Variable | Pixel data + control               | —              | Experimental                                     |

Known issues carried forward: J.1 (barometric pressure range), J.2 (wind speed
max), J.7 (rain size semantics), J.8 (UV Index max), J.11 (irradiance bits),
J.15 (no standalone irradiance/UV).

---

## 2. Encoding Architecture (Abstract Building Blocks)

This section defines the vocabulary and mechanisms for encoding sensor values.
It is domain-agnostic — "what it measures" comes from the variant label and
domain layer (Section 3). The architecture here answers: how do we pack a value
into bits, and what structural properties does that encoding have?

### 2.1 Three-Layer Field Type Hierarchy

Field types are organised in three layers that represent increasing levels of
semantic commitment.

**Layer 1 — Generic / Flexible.** Raw bit-width types with no protocol-defined
semantics. The variant label provides all meaning. No quantisation, no units, no
JSON structure — just N bits in the packet. Examples: UINT8, RAW16, BIT1,
FLOAT32.

**Layer 2 — Unit-Based.** Types that encode a specific physical unit with a
defined quantisation but no specific measurement context. "Distance in cm" is
unit-based; "snow depth" is specific. Unit-based types can be reused across many
variant labels. Each Layer 2 type has defined: bit width, value range,
quantisation step, SI or conventional unit, scale type (§2.2), and sign domain
(§2.2).

**Layer 3 — Specific / Domain.** Types with fully defined semantics:
measurement context, quantisation, range, and JSON structure. Existing fields
(Temperature, Pressure, Wind, etc.) are Layer 3. New Layer 3 types are defined
in Section 3 under their respective domains.

**The layers are not a strict compositional hierarchy.** A higher layer *may*
build on a lower layer — Soil Moisture is built on PERCENTAGE (Layer 2), which
is a constrained form of UINT8 (Layer 1). But a higher layer type can equally
originate as a root with its own encoding, owing nothing to the layers below.
Water pH (Layer 3) has a bespoke 8-bit encoding with its own quantisation
formula; it is not built on any Layer 2 or Layer 1 type. The Environment bundle
(Layer 3) packs three measurements with a unique bit layout that doesn't
decompose into reusable Layer 2 components.

What the layers provide is **vocabulary and optionality**, not a mandatory
inheritance chain. When a natural building block exists at a lower layer, use
it — this gives you reuse, decoder simplicity, and a shorter path to
implementation. When it doesn't, define the type fresh at the layer where it
belongs. The layer number reflects the level of semantic commitment (none →
unit → full domain context), not a dependency relationship.

This layering also determines decoder capability. A decoder that knows only
Layer 1 can extract and forward raw values from any field. A decoder that knows
Layer 2 can produce correctly-typed numeric values with units. A decoder that
knows Layer 3 can produce structured, domain-specific JSON. Each layer is a
useful capability plateau — a deployment can operate at whatever layer its
tooling supports.

### 2.2 Scale Types and Sign Domains

Every encoded value has two interpretation properties: a **scale type** (how
raw bits map to real-world values) and a **sign domain** (what region of the
number line the value occupies). Together these fully characterise the
decoder's job for a given field. Both are first-class properties of each
Layer 2 type — not ad-hoc per-field decisions.

#### Scale Types

| Scale Type          | Description                                                                                    | Examples                                                | Encoding Behaviour                                                             |
| ------------------- | ---------------------------------------------------------------------------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------ |
| **Linear**          | Uniform quantisation steps across the range. Most common.                                      | Temperature, pressure, distance, voltage                | `value = offset + (raw × step)`                                                |
| **Logarithmic**     | Steps proportional to value. Covers large dynamic ranges in few bits.                          | Lux, turbidity, some gas concentrations                 | `value = base^(raw / steps_per_decade) × min_value` (formula TBD — see §2.13)  |
| **Categorical**     | Discrete named levels from a finite, known set. No meaningful arithmetic between levels.       | Cloud cover (okta), leaf wetness levels, Beaufort scale | `value = lookup[raw]`; no interpolation                                        |
| **Wrapping/Cyclic** | Value wraps at boundaries. For counters: monotonic until overflow. For angles: continuous wrap.| Rain tips, energy totaliser, compass bearing            | Counter: increment-only, wrap at max. Angle: 0° = 360°.                        |
| **Ratio**           | Normalised 0.0–1.0 (or -1.0 to +1.0). Semantically a proportion, not a count.                  | NDVI, duty cycle, confidence score, reflectance         | `value = raw / max_raw`                                                        |
| **Identifier**      | Opaque reference to an entity. No ordering, no arithmetic, no pre-defined set.                 | RFID UID, barcode, QR content hash, UUID, NFC tag ID    | Raw bytes; equality comparison only.                                           |

The Identifier type is distinct from Categorical in an important way.
Categorical values are drawn from a finite, pre-defined set — the decoder knows
the possible values and can map them to labels. Identifier values are drawn from
an open, unbounded space — the decoder cannot enumerate them in advance. It can
only test equality ("is this the same tag I saw before?") or pass the value
through for external resolution. Identifiers have no scale at all — they are
not measurements. They carry provenance or reference information alongside
measurements. In practice, Identifier maps to the RAW types (RAW8 through
RAW128) at Layer 1. The scale type annotation tells the decoder not to attempt
numeric interpretation, aggregation, or interpolation — just store and compare.

#### Sign Domains

| Sign Domain  | Description                                                         | Examples                                     |
| ------------ | ------------------------------------------------------------------- | -------------------------------------------- |
| **Unsigned** | Non-negative values only. All bits encode magnitude.                | Distance, percentage, lux, counter, weight   |
| **Signed**   | Positive and negative values. MSB or offset encoding.               | Acceleration, ORP, water potential, tilt     |
| **Cyclic**   | Wraps at domain boundaries. Neither positive nor negative — directional. | Compass bearing (0–360°), phase angle   |
| **N/A**      | Not applicable. Value is not numeric.                               | RAW types, Identifier scale type             |

Cyclic sign domain is related to but distinct from the Wrapping/Cyclic scale
type. A wrapping counter (rain tips, energy totaliser) has **unsigned** sign
domain and **wrapping** scale type — it wraps due to overflow of a
non-negative accumulator. A compass bearing has **cyclic** sign domain and
**linear** scale type — it wraps because the domain is inherently circular
(359° + 2° = 1°, not 361°). The combination of scale type and sign domain
together captures this distinction.

#### Why these matter

When scale type and sign domain are defined per-type rather than per-deployment,
decoders can apply the correct reconstruction formula without out-of-band
metadata. A LOG type is decoded differently from a LINEAR type even if both are
8 bits. A SIGNED type is bit-shifted differently from an UNSIGNED type at the
same width. Defining these properties at the type level means a generic decoder
can handle any field it encounters, even one it has never seen before, provided
it knows the Layer 2 type.

### 2.3 (Reserved)

### 2.4 Temporal Interpretation

A single physical quantity can be transmitted with different temporal semantics.
These are not different field types — they are interpretation modes defined by
the variant configuration.

| Mode              | Meaning                                             | Examples                                          |
| ----------------- | --------------------------------------------------- | ------------------------------------------------- |
| **Instantaneous** | Value at the moment of sampling.                    | Temperature reading, pH, GPS position             |
| **Averaged**      | Mean over the reporting interval.                   | Mean wind speed, average sound level              |
| **Peak / Max**    | Maximum observed during the reporting interval.     | Wind gust, peak vibration                         |
| **Minimum**       | Minimum observed during the reporting interval.     | Overnight low temperature                         |
| **Cumulative**    | Accumulated total, typically wrapping.              | Rain accumulation, energy totaliser, flow volume  |
| **Rate**          | First derivative: change per unit time.             | Temperature rate (°C/min), pressure rate (hPa/hr) |

The variant definition determines which mode a field slot uses (see §2.6 for
the presence bit mechanism). The protocol encoding is identical regardless of
mode — interpretation is semantic, not structural.

Note: averaged, peak, and minimum modes imply that the sensor is performing
local aggregation between transmissions. The protocol does not mandate a
specific aggregation interval — that is a deployment concern.

### 2.5 The `_expanded` Convention

Where a measurement needs a wider range or finer resolution than the standard
encoding, a second field type is defined with the suffix `_expanded`. Both
encodings coexist in the protocol; the variant chooses which to use.

```text
TEMPERATURE          — 9 bits, -40 to +80°C, 0.25°C
TEMPERATURE_EXPANDED — 12 bits, -60 to +150°C, 0.05°C

PRESSURE             — 8 bits, 850–1105 hPa, 1 hPa
PRESSURE_EXPANDED    — 10 bits, 300–1100 hPa, ~0.8 hPa

DEPTH / DISTANCE_CM  — 10 bits, 0–1023 cm, 1 cm
DISTANCE_CM_EXPANDED — 16 bits, 0–65535 cm, 1 cm
```

Rules:

- The `_expanded` type is a separate field type ID.
- The variant picks one or the other — they never coexist for the same
  measurement in the same variant.
- The base type optimises for bit efficiency in the common case.
- The `_expanded` type covers edge cases (high altitude, extreme temperatures,
  precision distance) at the cost of more bits.
- No in-band signalling is needed — the variant advertisement declares which
  encoding is used.

### 2.6 Presence and Inclusion Semantics

A presence bit in the variant table controls whether a field is included in a
given packet. Beyond simple present/absent, the presence bit mechanism supports
several patterns.

**(a) Fixed interpretation.** The variant defines a field as always absolute or
always rate. Simplest case, no overhead.

```text
Field S2: TEMPERATURE (absolute)      — always transmits absolute temp
```

**(b) Dynamic selector.** The variant allocates two presence slots: a 1-bit
flag (BIT1) indicating interpretation, followed by the measurement field. The
flag costs 1 bit when present, but allows the sensor to dynamically choose
absolute or rate per packet.

```text
Field S2: BIT1 label="temp_is_rate"    — 0 = absolute, 1 = rate of change
Field S3: TEMPERATURE label="temp"     — value interpreted per S2
```

**(c) Separate fields.** The variant allocates separate slots for absolute and
rate, each independently present/absent.

```text
Field S2: TEMPERATURE label="temperature"
Field S3: TEMPERATURE label="temperature_rate"
```

**(d) Zero-width / NULL field.** A presence bit with no content (zero-width
field). The mere presence of the bit in the packet carries 1 bit of
information. This is the NULL type at Layer 1.

```text
Field S4: NULL label="motion_detected"  — 0 bits of data; present = true
```

The NULL type is the cheapest possible signalling mechanism — useful for binary
state flags (motion, door, tamper, alarm) where the event itself is the data.

### 2.7 Bundles and Co-location

A bundle is a Layer 3 type that packs multiple related measurements into a
single field with a single presence bit. Bundles save presence bits but are
inflexible — all components are present or absent together.

**Bundle policy (proposed rule):** Define a bundle only when:

- A common sensor or sensor class always outputs all component values together
  (e.g., BME280 → temp + pressure + humidity; Teros 12 → VWC + EC + temp).
- The components are routinely consumed together (e.g., wind speed + direction +
  gust).
- The presence-bit savings are meaningful relative to the total packet size.

If components may be independently present/absent, use separate fields.

Existing bundles: Environment (24 bits), Wind (22 bits), Rain (12 bits), Solar
(14 bits), Air Quality (21+ bits), Radiation (28 bits).

Candidate new bundles are defined in Section 3 under their respective domains.

### 2.8 Naming Conventions

| Element                | Convention                                          | Examples                                   |
| ---------------------- | --------------------------------------------------- | ------------------------------------------ |
| Layer 1 types          | ALL_CAPS, bit-width suffix                          | UINT8, INT16, RAW24, BIT1, NULL            |
| Layer 2 types          | ALL_CAPS, unit suffix                               | DISTANCE_CM, PERCENTAGE, CONDUCTIVITY, LUX |
| Alternative encodings  | Base name + qualifier suffix                        | TEMPERATURE_EXPANDED, CONDUCTIVITY_COARSE, DISTANCE_MM_SHORT |
| Layer 3 types          | Title Case in documentation, ALL_CAPS in protocol   | Soil Moisture, Water pH, Lightning Bundle  |
| Variant labels         | snake_case, descriptive                             | `soil_moisture`, `water_ec`, `temp_is_rate`|
| JSON output keys       | snake_case, matching variant label where applicable | `soil_moisture_vwc`, `water_ph`            |

#### Alternative Encoding Variants

Where a single physical unit needs more than one encoding, alternative variants
are defined with a qualifying suffix. These are all instances of the same
concept — trading bit cost against range or resolution — viewed from different
directions.

The two independent axes of variation are:

- **Range:** The span of values the encoding can represent (e.g., -40 to +80°C
  vs -60 to +150°C).
- **Resolution:** The smallest distinguishable step (e.g., 0.25°C vs 0.05°C).

An alternative encoding may change one or both of these relative to the base
type. More bits typically buys more range, finer resolution, or both — but the
designer chooses where to spend those bits.

| Direction    | What it means                                      | Suffix style         | Examples                                             |
| ------------ | -------------------------------------------------- | -------------------- | ---------------------------------------------------- |
| Expansion    | Wider range and/or finer resolution, more bits     | `_EXPANDED`          | TEMPERATURE_EXPANDED (12 bits vs 9)                  |
| Contraction  | Narrower range or coarser resolution, fewer bits   | `_REDUCED`           | CONDUCTIVITY_REDUCED (10 bits vs 16), DISTANCE_MM_REDUCED (10 bits vs 16) |

Both are "alternatives" — neither is inherently better. The base type optimises
for bit efficiency in the common case. Expansions cover edge cases at higher
bit cost. Contractions sacrifice precision or range to save bits where the full
encoding is unnecessary.

Rules:

- Each alternative is a separate field type ID.
- The variant picks exactly one encoding per measurement — alternatives never
  coexist for the same measurement in the same variant.
- No in-band signalling is needed. The variant advertisement declares which
  encoding is used.
- The base type name (without suffix) should represent the most broadly useful
  encoding for the target deployment class. This is a judgement call per type.
- Preferable to use `_EXPANDED` and `_REDUCED` labels, but permissible to use
  alternatives (e.g. `_LONGER`, `_SHORTER`, `_COARSER`)

When defining a new Layer 2 type, consider whether the common case and the
edge case differ enough on range or resolution to justify an alternative. If
they do, decide which axis matters: a high-altitude pressure sensor needs range
expansion (300–1100 hPa vs 850–1105 hPa); a precision laboratory sensor needs
resolution expansion (0.05°C vs 0.25°C); a brackish-water EC sensor needs
range with resolution contraction (50 µS/cm steps to cover 0–51,150 in 10 bits
vs 1 µS/cm steps in 16 bits). Making the axis explicit helps choose the right
suffix and avoids defining alternatives that don't serve a real deployment need.

### 2.9 Derivation Relationships

Some quantities are derivable from others. The protocol does not forbid
encoding a derived value directly — a sensor may compute and transmit it — but
awareness of the derivation network informs priority decisions and avoids
redundant type definitions.

| Derived Quantity         | Derived From                   | Notes                                                     |
| ------------------------ | ------------------------------ | --------------------------------------------------------- |
| Total dissolved solids   | Electrical conductivity        | TDS ≈ EC × factor (0.5–0.7 typical)                      |
| Salinity                 | EC + temperature               | Standard oceanographic formula                            |
| Flow rate (open channel) | Water depth + rating curve     | Sensor transmits depth; gateway computes flow             |
| Dew point                | Temperature + humidity         | Magnus formula                                            |
| Heat index               | Temperature + humidity         | Steadman formula                                          |
| Wind chill               | Temperature + wind speed       | Standard formula                                          |
| UV Index (from sensor)   | UV irradiance                  | Weighted spectral integral; some sensors output directly  |
| AQI                      | PM2.5, PM10, O₃, etc.         | EPA breakpoint interpolation                              |
| Power                    | Voltage × current              | Ohm's law                                                 |
| Energy                   | Accumulated power × time       | Totaliser                                                 |
| Altitude                 | Barometric pressure + temp     | Barometric formula                                        |
| Colour temperature       | RGB values                     | Computed from chromaticity                                |

**Implication for type priority:** Types that are easily derived at the gateway
(TDS, salinity, dew point, heat index, wind chill, altitude) have lower
priority for dedicated encoding than fundamental types.

### 2.10 Resolution vs Accuracy Boundary

The protocol defines **encoding resolution** — the quantisation step size. This
is a protocol-layer concern.

The protocol does NOT define **sensor accuracy**, **calibration state**, or
**measurement methodology**. These are device-layer concerns (Section 4).

Example: TEMPERATURE has 0.25°C resolution. A TMP117 sensor has ±0.1°C
accuracy (better than the encoding). A DHT11 has ±2°C accuracy (much worse
than the encoding). Both use the same field type. The encoding resolution sets a
floor on what precision the protocol can represent, but does not guarantee that
the sensor achieves it.

This boundary prevents two failure modes:

- **Over-specification:** Defining encoding resolution to match a specific
  high-end sensor, wasting bits for most deployments.
- **Under-specification:** Failing to document that a cheap sensor's accuracy is
  far coarser than the encoding suggests, misleading consumers of the data.

Section 4 (Device Coverage) documents sensor accuracy alongside the encoding
resolution from Section 2, making the gap visible.

### 2.11 Layer 1 — Generic / Flexible Field Types

No protocol-defined units or quantisation. Variant label gives meaning.

#### Integer Types

| Type   | Bits | Range              | Scale  | Sign     | Description                              |
| ------ | ---- | ------------------ | ------ | -------- | ---------------------------------------- |
| NULL   | 0    | presence only      | —      | —        | Zero-width flag. Presence bit = true.    |
| BIT1   | 1    | 0/1                | Categ. | Unsigned | Boolean. Door, alarm, mode select.       |
| UINT4  | 4    | 0–15               | Linear | Unsigned | Small enum, category, state machine.     |
| UINT8  | 8    | 0–255              | Linear | Unsigned | Generic unsigned byte.                   |
| UINT10 | 10   | 0–1023             | Linear | Unsigned | Medium unsigned.                         |
| UINT16 | 16   | 0–65535            | Linear | Unsigned | Large unsigned, raw ADC.                 |
| UINT32 | 32   | 0–4,294,967,295    | Linear | Unsigned | Very large unsigned.                     |
| UINT64 | 64   | 0–1.8×10¹⁹         | Linear | Unsigned | Extended unsigned. Expensive (8 bytes).  |
| INT8   | 8    | -128 to +127       | Linear | Signed   | Signed byte.                             |
| INT16  | 16   | -32768 to +32767   | Linear | Signed   | Signed word.                             |
| INT32  | 32   | ±2,147,483,647     | Linear | Signed   | Large signed integer.                    |
| INT64  | 64   | ±9.2×10¹⁸          | Linear | Signed   | Extended signed. Expensive (8 bytes).    |

#### Floating Point Types

| Type    | Bits | Range                   | Scale | Sign   | Description                              |
| ------- | ---- | ----------------------- | ----- | ------ | ---------------------------------------- |
| FLOAT16 | 16   | ±65,504 (~3 digits)     | —     | Signed | IEEE 754 half-precision.                 |
| FLOAT32 | 32   | ±3.4×10³⁸ (~7 digits)   | —     | Signed | IEEE 754 single-precision.               |
| FLOAT64 | 64   | ±1.8×10³⁰⁸ (~15 digits) | —     | Signed | IEEE 754 double-precision. Expensive.    |

#### Opaque / Raw Types

| Type   | Bits | Range   | Scale | Sign | Description                              |
| ------ | ---- | ------- | ----- | ---- | ---------------------------------------- |
| RAW8   | 8    | opaque  | —     | —    | Opaque byte, no interpretation.          |
| RAW16  | 16   | opaque  | —     | —    | Opaque 2 bytes.                          |
| RAW24  | 24   | opaque  | —     | —    | Opaque 3 bytes.                          |
| RAW32  | 32   | opaque  | —     | —    | Opaque 4 bytes.                          |
| RAW64  | 64   | opaque  | —     | —    | Opaque 8 bytes.                          |
| RAW128 | 128  | opaque  | —     | —    | Opaque 16 bytes.                         |

Notes:

- **Integer vs RAW:** UINT/INT types are numeric (JSON outputs a number). RAW
  types are opaque (JSON outputs hex or base64). Semantics differ at the
  gateway.
- **Float scale type is "—"** because IEEE 754 encodes its own scale via
  mantissa and exponent. This is a meaningful distinction from integer types
  where the protocol defines scale via quantisation parameters.
- **FLOAT16** (half-precision) is the recommended default for uncalibrated or
  prototype measurements where no domain-specific quantisation has been defined.
  Same cost as UINT16, but self-scaling.
- **Cost awareness:** 64-bit and 128-bit types are expensive in constrained
  payloads. A single FLOAT64 or RAW128 consumes 8–16 bytes — potentially
  15–30% of a LoRa payload. Use only when narrower types cannot serve.
- **Identifier payloads:** RAW types (especially RAW64 and RAW128) serve
  non-quantifiable data such as barcode values, QR code content hashes, RFID
  UIDs, NFC tag identifiers, UUID device references, or cryptographic
  signatures. These are not measurements — they carry identity or provenance
  information. The variant label defines interpretation (e.g.,
  `label="rfid_uid"`, `label="asset_barcode"`).
- **BIT1** scale type is Categorical because 0/1 are discrete states. However,
  BIT1 also serves as a dynamic selector flag (§2.6b) — a structural role
  rather than a measurement.
- Variant map may optionally carry scale/offset hints for generic types in
  future (J.27 variant advertisement), but for now interpretation is
  out-of-band.

### 2.12 Layer 2 — Unit-Based Field Types

Defined quantisation for a physical unit. Reusable across many variant labels.
No domain-specific semantics — "what it measures" comes from the variant label.

Each type specifies: bits, range, resolution, unit, scale type, and sign domain.

#### Length / Distance

| Type                 | Bits | Range        | Resolution | Unit | Scale   | Sign     |
| -------------------- | ---- | ------------ | ---------- | ---- | ------- | -------- |
| DISTANCE_CM          | 10   | 0–1023 cm    | 1 cm       | cm   | Linear  | Unsigned |
| DISTANCE_CM_EXPANDED | 16   | 0–65535 cm   | 1 cm       | cm   | Linear  | Unsigned |
| DISTANCE_MM          | 16   | 0–65535 mm   | 1 mm       | mm   | Linear  | Unsigned |
| DISTANCE_MM_SHORT    | 10   | 0–1023 mm    | 1 mm       | mm   | Linear  | Unsigned |

Use cases: snow depth, water level, soil depth, ultrasonic range, lidar, object
distance, ice thickness, clearance.

Note: existing DEPTH (10 bits, 0–1023 cm) becomes an alias for DISTANCE_CM.

#### Percentage

| Type            | Bits | Range     | Resolution | Unit | Scale  | Sign     |
| --------------- | ---- | --------- | ---------- | ---- | ------ | -------- |
| PERCENTAGE      | 7    | 0–100%    | 1%         | %    | Linear | Unsigned |
| PERCENTAGE_FINE | 10   | 0–100.0%  | 0.1%       | %    | Linear | Unsigned |

Use cases: soil moisture, leaf wetness, dissolved oxygen saturation, humidity
(existing HUMIDITY is an alias), battery (different encoding though), signal
quality, generic fill level.

#### Ratio

| Type    | Bits | Range           | Resolution | Unit  | Scale | Sign     |
| ------- | ---- | --------------- | ---------- | ----- | ----- | -------- |
| RATIO8  | 8    | 0.000–1.000     | ~1/255     | ratio | Ratio | Unsigned |
| RATIO16 | 16   | 0.0000–1.0000   | ~1/65535   | ratio | Ratio | Unsigned |

Use cases: NDVI, duty cycle, confidence score, reflectance, normalised index.

#### Temperature

| Type                 | Bits | Range          | Resolution | Unit | Scale  | Sign   |
| -------------------- | ---- | -------------- | ---------- | ---- | ------ | ------ |
| TEMPERATURE          | 9    | -40 to +80°C   | 0.25°C     | °C   | Linear | Signed |
| TEMPERATURE_EXPANDED | 12   | -60 to +150°C  | ~0.05°C    | °C   | Linear | Signed |

Existing TEMPERATURE unchanged. TEMPERATURE_EXPANDED for industrial, desert,
high-altitude, or precision applications.

#### Pressure

| Type              | Bits | Range         | Resolution | Unit | Scale  | Sign     |
| ----------------- | ---- | ------------- | ---------- | ---- | ------ | -------- |
| PRESSURE          | 8    | 850–1105 hPa  | 1 hPa      | hPa  | Linear | Unsigned |
| PRESSURE_EXPANDED | 10   | 300–1100 hPa  | ~0.8 hPa   | hPa  | Linear | Unsigned |
| PRESSURE_KPA      | 14   | 0–10,000 kPa  | ~0.6 kPa   | kPa  | Linear | Unsigned |

PRESSURE_EXPANDED addresses J.1 (altitude range issue). PRESSURE_KPA is
non-atmospheric: pipe, hydraulic, tyre, water pressure.

#### Electrical

| Type                | Bits | Range           | Resolution | Unit   | Scale  | Sign     |
| ------------------- | ---- | --------------- | ---------- | ------ | ------ | -------- |
| VOLTAGE_MV          | 16   | 0–65535 mV      | 1 mV       | mV     | Linear | Unsigned |
| CURRENT_MA          | 16   | 0–65535 mA      | 1 mA       | mA     | Linear | Unsigned |
| POWER_W             | 16   | 0–65535 W       | 1 W        | W      | Linear | Unsigned |
| ENERGY_WH           | 16   | 0–65535 Wh      | 1 Wh       | Wh     | Linear | Wrapping |
| CONDUCTIVITY        | 16   | 0–65535 µS/cm   | 1 µS/cm    | µS/cm  | Linear | Unsigned |
| CONDUCTIVITY_COARSE | 10   | 0–51,150 µS/cm  | 50 µS/cm   | µS/cm  | Linear | Unsigned |

Notes:

- CONDUCTIVITY serves both soil EC and water EC — variant label distinguishes.
- Two resolutions: fine (1 µS/cm, 16 bits, freshwater precision) and coarse (50
  µS/cm, 10 bits, covers brackish/seawater in fewer bits).
- ENERGY_WH is a wrapping totaliser (scale type = Wrapping).

#### Volume / Flow

| Type              | Bits | Range          | Resolution | Unit  | Scale  | Sign     |
| ----------------- | ---- | -------------- | ---------- | ----- | ------ | -------- |
| FLOW_LPM          | 10   | 0–1023 L/min   | 1 L/min    | L/min | Linear | Unsigned |
| FLOW_LPM_EXPANDED | 16   | 0–65535 L/min  | 1 L/min    | L/min | Linear | Unsigned |
| VOLUME_L          | 16   | 0–65535 L      | 1 L        | L     | Linear | Wrapping |

#### Weight / Force

| Type              | Bits | Range             | Resolution | Unit | Scale  | Sign     |
| ----------------- | ---- | ----------------- | ---------- | ---- | ------ | -------- |
| WEIGHT_G          | 16   | 0–65535 g         | 1 g        | g    | Linear | Unsigned |
| WEIGHT_G_EXPANDED | 24   | 0–16,777,215 g    | 1 g        | g    | Linear | Unsigned |
| FORCE_N           | 16   | 0–65535 N         | 1 N        | N    | Linear | Unsigned |

WEIGHT_G_EXPANDED covers up to ~16.7 tonnes — sufficient for silo/tank
monitoring and livestock scales.

#### Speed

| Type              | Bits | Range        | Resolution  | Unit | Scale  | Sign     |
| ----------------- | ---- | ------------ | ----------- | ---- | ------ | -------- |
| SPEED_MS          | 7    | 0–63.5 m/s   | 0.5 m/s     | m/s  | Linear | Unsigned |
| SPEED_MS_EXPANDED | 10   | 0–127.5 m/s  | ~0.125 m/s  | m/s  | Linear | Unsigned |

Existing wind speed is an alias for SPEED_MS. SPEED_MS_EXPANDED addresses J.2.

#### Angle

| Type           | Bits | Range          | Resolution | Unit    | Scale  | Sign    |
| -------------- | ---- | -------------- | ---------- | ------- | ------ | ------- |
| ANGLE_360      | 8    | 0–355°         | ~1.41°     | degrees | Linear | Cyclic  |
| ANGLE_360_FINE | 10   | 0–359.6°       | ~0.35°     | degrees | Linear | Cyclic  |
| ANGLE_SIGNED   | 10   | -180 to +180°  | ~0.35°     | degrees | Linear | Signed  |
| TILT           | 8    | -90 to +90°    | ~0.7°      | degrees | Linear | Signed  |

ANGLE_360 = existing wind direction encoding. TILT for inclinometers.

#### Counter

| Type      | Bits | Range    | Resolution | Unit           | Scale    | Sign     |
| --------- | ---- | -------- | ---------- | -------------- | -------- | -------- |
| COUNTER8  | 8    | 0–255    | 1          | wrapping count | Wrapping | Unsigned |
| COUNTER16 | 16   | 0–65535  | 1          | wrapping count | Wrapping | Unsigned |

Semantically distinct from UINT — counters wrap, measurements don't. JSON
should note wrapping semantics. Use cases: rain tips, flow pulses, lightning
strikes, event counts, energy totaliser.

#### Sound

| Type               | Bits | Range      | Resolution  | Unit | Scale  | Sign     |
| ------------------ | ---- | ---------- | ----------- | ---- | ------ | -------- |
| SOUND_DBA          | 8    | 0–140 dBA  | ~0.55 dBA   | dBA  | Linear | Unsigned |
| SOUND_DBA_EXPANDED | 10   | 0–140 dBA  | ~0.14 dBA   | dBA  | Linear | Unsigned |

Note: dBA is itself a logarithmic unit (decibels), but the encoding of the dBA
value is linear. The log transformation has already occurred at the sensor.

#### Illuminance

| Type    | Bits | Range           | Resolution  | Unit | Scale       | Sign     |
| ------- | ---- | --------------- | ----------- | ---- | ----------- | -------- |
| LUX     | 16   | 0–65535 lux     | 1 lux       | lux  | Linear      | Unsigned |
| LUX_LOG | 8    | ~1–120,000 lux  | ~12%/step   | lux  | Logarithmic | Unsigned |

LUX_LOG covers the full outdoor range (starlight to bright sun) in 8 bits using
logarithmic encoding. This is the first log-scale type — it sets the precedent
for other skewed-distribution fields (see §2.13).

#### Concentration (generic)

| Type | Bits | Range          | Resolution | Unit  | Scale  | Sign     |
| ---- | ---- | -------------- | ---------- | ----- | ------ | -------- |
| PPM  | 16   | 0–65535 ppm    | 1 ppm      | ppm   | Linear | Unsigned |
| PPB  | 16   | 0–65535 ppb    | 1 ppb      | ppb   | Linear | Unsigned |
| UGM3 | 16   | 0–65535 µg/m³  | 1 µg/m³    | µg/m³ | Linear | Unsigned |

Generic concentration types for gas/particulate measurements not covered by the
existing AQ fields.

#### Magnetic

| Type        | Bits | Range             | Resolution | Unit | Scale  | Sign   |
| ----------- | ---- | ----------------- | ---------- | ---- | ------ | ------ |
| MAGNETIC_UT | 10   | -512 to +511 µT   | 1 µT       | µT   | Linear | Signed |

#### Acceleration

| Type             | Bits | Range             | Resolution | Unit | Scale  | Sign   |
| ---------------- | ---- | ----------------- | ---------- | ---- | ------ | ------ |
| ACCEL_G          | 10   | -16.0 to +15.9 g  | ~0.03 g    | g    | Linear | Signed |
| ACCEL_G_EXPANDED | 16   | -32.0 to +32.0 g  | ~0.001 g   | g    | Linear | Signed |

#### RPM

| Type | Bits | Range        | Resolution | Unit | Scale  | Sign     |
| ---- | ---- | ------------ | ---------- | ---- | ------ | -------- |
| RPM  | 14   | 0–16383 RPM  | 1 RPM      | RPM  | Linear | Unsigned |

### 2.13 Open Design Questions (Architecture Level)

These are unresolved questions that live at the encoding architecture level,
not in any specific domain.

**Log-scale encoding formula.** LUX_LOG and Water Turbidity (§3.2) both need
logarithmic encoding. Define a general formula
(`value = base^(raw / steps_per_decade) × min_value`) or allow per-field
formulas? Need to decide on base and step count. A single formula with
per-type parameters (min, max, bits) would be cleanest.

**Gas expansion strategy.** Extend existing AQ Gas mask from 8 to 16 bits
(adding CH₄, NH₃, H₂S, SO₂, O₂, Rn to existing structure) vs define
standalone gas types using generic PPM/PPB with variant labels (simpler
variants, but no bundle benefit). See §3.1.4 for the gas species list.

**Generic type metadata in variant map.** For Layer 1 types (UINT8 etc.),
should the variant map (J.27 variant advertisement) carry scale/offset/unit
metadata to enable automatic JSON output with correct units? Or is that purely
out-of-band? If yes, this allows UINT16 with variant metadata "scale=0.1,
offset=0, unit=kPa" to become a self-describing type. If no, the deployment
documentation must provide this.

**Bundle policy formalisation.** The rule in §2.7 is "proposed" — needs to be
confirmed. Open sub-question: should bundles allow optional components (some
present, some absent within the bundle), or is a bundle strictly all-or-nothing?
All-or-nothing is simpler and matches existing bundles.

**Field type ID allocation.** Need to define the global ID numbering scheme.
Options: Layer 1 in one range, Layer 2 in another, Layer 3 in another? Or flat?
Currently ~25 existing types. This document adds ~40-50 more candidates. The
7-bit global field type ID (J.27) supports 128 types — comfortable, but should
we reserve ranges?

**Canonical units.** Should the protocol enforce a single canonical unit per
physical quantity (e.g., always µS/cm for conductivity, always hPa for
pressure), with unit conversion happening at the edge? Or support unit variants?
Canonical units simplify decoders; unit variants add flexibility.

---

## 3. Domain Coverage (What We Measure)

This section catalogues measurement types organised by environmental and
application domain. For each domain: the measurements, their ranges and units,
which Layer 2 types they use, and what Layer 3 specific types or bundles are
needed.

### 3.1 Atmosphere

#### 3.1.1 Weather (Temperature, Pressure, Humidity, Wind, Rain, Clouds)

Fully covered by existing protocol fields (Section 1). Key issues carried
forward:

- **J.1 — Barometric pressure range:** 850–1105 hPa is too narrow for
  high-altitude deployments. PRESSURE_EXPANDED (10 bits, 300–1100 hPa) resolves
  this.
- **J.2 — Wind speed maximum:** 63.5 m/s may be too low for severe weather.
  SPEED_MS_EXPANDED (10 bits, 0–127.5 m/s) resolves this.
- **J.7 — Rain size semantics:** Undefined. Needs specification work.
- **J.15 — Solar/UV standalone:** Irradiance and UV Index currently exist only
  as bundle components. Standalone types needed (see §3.1.2).

No new Layer 3 types needed for basic weather beyond the expanded variants and
standalone splits.

#### 3.1.2 Solar and Light

Existing: Solar bundle (14 bits: irradiance + UV).

**New standalone types (split from Solar bundle — J.15):**

| Type                    | Built On | Bits | Range         | Resolution | Notes                        |
| ----------------------- | -------- | ---- | ------------- | ---------- | ---------------------------- |
| Standalone Irradiance   | —        | 10   | 0–1023 W/m²   | 1 W/m²     | Same encoding as in bundle   |
| Standalone UV Index     | —        | 4    | 0–15          | 1          | Same encoding as in bundle   |

**Additional optical types (beyond solar):**

| Type                     | Built On | Bits    | Range                | Resolution  | Notes                                              |
| ------------------------ | -------- | ------- | -------------------- | ----------- | -------------------------------------------------- |
| Illuminance              | LUX      | 16      | 0–65535 lux          | 1 lux       | BH1750, VEML7700 range                             |
| Illuminance (compact)    | LUX_LOG  | 8       | ~1–120,000 lux       | ~12%/step   | Log scale; full outdoor range in 8 bits            |
| Colour RGB               | —        | 24      | 3×8 bits per channel | 1           | TCS34725 etc.                                      |
| Infrared band power      | UINT16   | 16      | 0–65535 (raw)        | variant     | Thermopile, pyroelectric; variant defines scaling  |

Notes:

- Lux has enormous dynamic range — 0.001 (starlight) to 120,000 (bright sun).
  LUX_LOG is the primary encoding for outdoor deployments. Linear LUX is for
  indoor or constrained-range applications.
- CMYK colour (4×7=28 bits) was considered and dropped — not a sensor output in
  IoT contexts. If needed, compute from RGB at the gateway (derivable — §2.9).
- Colour temperature (K) is derivable from RGB (§2.9). Not a dedicated type.
- Infrared band power is niche (fire detection, presence sensing). Covered by
  UINT16 with variant label until demand justifies a specific type.

#### 3.1.3 Air Quality (Particulates)

Fully covered by existing AQ PM fields (4–36 bits, 4 channels: PM1, PM2.5,
PM4, PM10, 0–1275 µg/m³, 5 µg/m³ resolution).

No changes needed.

#### 3.1.4 Air Quality (Gases)

Existing AQ Gas field covers: VOC, NOx, CO₂, CO, HCHO, O₃ + 2 reserved slots
(8-bit mask).

**Additional gas species not covered:**

| Gas                    | Units | Typical Range                         | Priority                      | Notes                                                   |
| ---------------------- | ----- | ------------------------------------- | ----------------------------- | ------------------------------------------------------- |
| Methane (CH₄)          | ppm   | 0–10,000 (LEL ~50,000)                | High (agriculture, landfill)  | MQ-4, Figaro TGS2611, Sensirion SGP41                   |
| Ammonia (NH₃)          | ppm   | 0–100 (livestock), 0–500 (industrial) | High (livestock)              | MQ-137, Sensirion SFA30, EC sense NH3/M-100             |
| Hydrogen sulfide (H₂S) | ppm   | 0–100                                 | Medium (wastewater, mining)   | Alphasense H2S-A4, Membrapor H2S/S-20                   |
| Sulfur dioxide (SO₂)   | ppb   | 0–2000                                | Medium (volcanic, industrial) | Alphasense SO2-A4, Membrapor SO2/S-20                   |
| Oxygen concentration   | %     | 0–25 (ambient ~20.9)                  | Medium (confined space)       | Alphasense O2-A2, SST LOX-02                            |
| Radon (Rn)             | Bq/m³ | 0–10,000                              | Low (indoor air quality)      | Airthings Wave, RadonEye RD200                          |
| Gas flow rate          | L/min | 0–100                                 | Low (medical/industrial)      | Mass flow sensors                                       |

**Decision needed (§2.13):** Extend AQ Gas mask from 8 to 16 bits to
accommodate these, or define as standalone types using generic PPM/PPB/UGM3 with
variant labels.

Arguments for mask extension: keeps the bundle benefit; consistent with existing
AQ structure. Arguments for standalone: simpler variants for single-gas sensors
(MQ-4, SFA30); doesn't force all gases into one structure.

CH₄ and NH₃ are highest priority for agricultural deployments. O₂% is distinct
from dissolved oxygen (§3.2) — same quantity, different medium.

#### 3.1.5 Radiation

Fully covered by existing Radiation bundle (28 bits: CPM + dose).

No changes needed.

#### 3.1.6 Lightning

Not currently covered. Candidate for new Layer 3 types.

| Type               | Built On | Bits | Range    | Resolution | Notes                      |
| ------------------ | -------- | ---- | -------- | ---------- | -------------------------- |
| Lightning Distance | UINT8    | 8    | 0–63 km  | ~0.25 km   | AS3935 range               |
| Lightning Count    | COUNTER8 | 8    | 0–255    | 1          | Strikes per period         |
| Lightning Bundle   | —        | 16   | Dist+Cnt | mixed      | Distance(8) + Count(8)     |

The AS3935 is the only widely available IC-level lightning detector. Its output
maps cleanly to this encoding.

### 3.2 Hydrosphere (Water Quality, Flow, Volume)

#### 3.2.1 Water Quality

| Type                 | Built On            | Bits | Range                  | Resolution  | Notes                                       |
| -------------------- | ------------------- | ---- | ---------------------- | ----------- | ------------------------------------------- |
| Water pH             | —                   | 8    | 0–14.0 pH              | ~0.055      | 255 steps over 14.0                         |
| Water pH (expanded)  | —                   | 10   | 0–14.00 pH             | ~0.014      | Lab-grade resolution                        |
| Water EC             | CONDUCTIVITY        | 16   | 0–65535 µS/cm          | 1 µS/cm     | Alias for CONDUCTIVITY with variant label   |
| Water EC (coarse)    | CONDUCTIVITY_COARSE | 10   | 0–51,150 µS/cm         | 50 µS/cm    | Brackish/seawater in fewer bits             |
| Water DO             | —                   | 8    | 0–25.5 mg/L            | 0.1 mg/L    | Dissolved oxygen                            |
| Water DO (%)         | PERCENTAGE          | 7    | 0–200% (supersaturation needs expansion) | 1% | Saturation mode; may need UINT8 for >100%   |
| Water Turbidity      | LUX_LOG-style       | 8    | ~0.1–10,000 NTU        | ~12%/step   | Log-scale, heavily right-skewed             |
| Water ORP            | —                   | 10   | -1000 to +1000 mV      | ~2 mV       | Redox potential                             |
| Water Quality Bundle | —                   | 34   | pH(8)+EC(10)+DO(8)+T(9)| mixed       | Multi-parameter sonde pattern               |

Additional water quality measurements (lower priority):

| Measurement            | Units | Typical Range               | Notes                                                     |
| ---------------------- | ----- | --------------------------- | --------------------------------------------------------- |
| Total dissolved solids | ppm   | 0–50,000                    | **Derivable from EC** (§2.9). Not a dedicated type.       |
| Salinity               | ppt   | 0–40 (seawater ~35)         | **Derivable from EC + temp** (§2.9). Not a dedicated type.|
| Chlorophyll-a          | µg/L  | 0–500                       | Niche but important for aquaculture/eutrophication        |

Notes:

- Turbidity has massive dynamic range — strong candidate for log-scale encoding,
  same formula family as LUX_LOG (§2.13).
- EC is unified with soil EC at Layer 2 (CONDUCTIVITY). The variant label
  distinguishes domain.
- DO saturation >100% (supersaturation) is physically possible. If using
  PERCENTAGE (7 bits, 0–100), this is truncated. A UINT8 variant (0–200% with
  0.78% resolution) or PERCENTAGE_FINE may be needed.

#### 3.2.2 Water Flow and Volume

| Type                | Built On         | Bits | Range          | Resolution | Notes                                        |
| ------------------- | ---------------- | ---- | -------------- | ---------- | -------------------------------------------- |
| Flow rate (small)   | FLOW_LPM         | 10   | 0–1023 L/min   | 1 L/min    | Garden hose, small pipe, YF-S201             |
| Flow rate (large)   | FLOW_LPM_EXPANDED| 16   | 0–65535 L/min  | 1 L/min    | Larger pipe, ultrasonic transit-time         |
| Flow volume         | VOLUME_L         | 16   | 0–65535 L      | 1 L        | Wrapping totaliser                           |

Notes:

- Huge dynamic range problem — a garden hose vs a river. The two FLOW_LPM
  variants handle the smaller end. Very large flows (m³/s) are out of scope for
  the sensor class iotdata targets.
- Flow volume is a wrapping counter. Gateway must handle wrap-around.
- Many flow measurements are derived from water level via stage-discharge curve.
  Sensor transmits depth (existing DEPTH field), gateway computes flow. This is
  a derivation relationship (§2.9).

#### 3.2.3 Water Level / Stage

Covered by existing DEPTH field (10 bits, 0–1023 cm) via variant label. For
deeper water or mm-precision, use DISTANCE_CM_EXPANDED or DISTANCE_MM.

No new types needed.

### 3.3 Lithosphere (Soil)

| Type                 | Built On     | Bits | Range                   | Resolution | Notes                                       |
| -------------------- | ------------ | ---- | ----------------------- | ---------- | ------------------------------------------- |
| Soil Moisture (VWC)  | PERCENTAGE   | 7    | 0–100% VWC              | 1%         | Variant label distinguishes from humidity   |
| Soil Moisture (raw)  | UINT16       | 16   | 1–80 ε (permittivity)   | variant    | Raw sensor output mode                      |
| Soil EC              | CONDUCTIVITY | 16   | 0–65535 µS/cm           | 1 µS/cm    | Or CONDUCTIVITY_COARSE for 10 bits          |
| Soil Water Potential | —            | 10   | -1023 to 0 kPa          | 1 kPa      | Matric potential, always negative (signed)  |
| Soil Temperature     | TEMPERATURE  | 9    | -40 to +80°C            | 0.25°C     | Reuse via variant label                     |
| Soil Bundle          | —            | 33   | VWC(7)+EC(16)+Temp(9)+Charging(1) | mixed | Teros 12 output pattern                |

Notes:

- Soil moisture and humidity share the same 0–100% range. Reusing PERCENTAGE
  (Layer 2) with variant label is the correct approach.
- Soil EC and water EC are the same physical measurement (conductivity) at
  different typical ranges. Unified at Layer 2 as CONDUCTIVITY.
- Water potential is critical for irrigation scheduling. It is distinct from
  moisture content — a dry sandy soil and a dry clay soil can have the same
  potential but very different VWC.
- Many soil probes output all values simultaneously (SDI-12, I2C). The Soil
  Bundle matches the Teros 12 output pattern.
- Raw permittivity (ε) is the uncalibrated sensor output. Covered by UINT16
  with variant label; calibration to VWC happens at the gateway or in
  post-processing.

### 3.4 Biosphere

#### 3.4.1 Vegetation and Agriculture

| Type               | Built On            | Bits    | Range                          | Resolution | Notes                                        |
| ------------------ | ------------------- | ------- | ------------------------------ | ---------- | -------------------------------------------- |
| Leaf Wetness       | UINT4 or PERCENTAGE | 4 or 7  | 0–15 or 0–100%                 | varies     | Categorical (4 levels) or continuous         |
| Surface Moisture   | PERCENTAGE          | 7       | 0–100%                         | 1%         | Same encoding as leaf wetness continuous     |
| PAR                | UINT16              | 16      | 0–2500 µmol/m²/s               | variant    | Scale TBD; distinct from solar irradiance    |
| NDVI               | RATIO8              | 8       | 0.0–1.0                        | ~1/255     | Normalised difference vegetation index       |
| Dendrometer        | UINT16              | 16      | 0–10,000 µm                    | variant    | Stem diameter change                         |

Notes:

- PAR (photosynthetically active radiation) measures photosynthetic wavelengths
  only (400–700 nm). It is distinct from broadband solar irradiance.
- Leaf wetness could be 2-bit categorical (dry/dew/wet/saturated) or 7-bit
  percentage. Categorical is cheaper; percentage is more informative. The
  variant can choose.
- NDVI is a normalised ratio, perfectly served by RATIO8. No dedicated type
  needed at Layer 3 beyond the variant label.
- Dendrometer values are micro-scale (µm) and slow-changing. UINT16 with
  variant-defined scaling is adequate.

#### 3.4.2 Acoustic / Wildlife Monitoring

| Type                | Built On           | Bits | Range      | Resolution  | Notes                                          |
| ------------------- | ------------------ | ---- | ---------- | ----------- | ---------------------------------------------- |
| Sound Level         | SOUND_DBA          | 8    | 0–140 dBA  | ~0.55 dBA   | Environmental noise compliance, wildlife       |
| Sound Level (fine)  | SOUND_DBA_EXPANDED | 10   | 0–140 dBA  | ~0.14 dBA   | Precision monitoring                           |
| Sound Frequency     | UINT16             | 16   | 20–20,000 Hz | variant   | Dominant frequency from FFT                    |
| Sound Bundle        | —                  | 24   | Level(8)+Freq(16) | mixed| Level + dominant frequency                     |

Notes:

- Sound level monitoring use cases: environmental noise compliance, wildlife
  acoustic triggers, beehive health (bee buzz frequency changes with colony
  state).
- Frequency is niche — mostly for species ID triggers or machinery fault
  detection. Covered by UINT16 with variant label.
- A Sound Bundle (level + dominant frequency) could serve beehive monitoring and
  environmental compliance simultaneously.

#### 3.4.3 Biological / Ecological (Gaps Identified)

The following measurement types are relevant to ecological monitoring but are
not currently covered by dedicated types. Most can be served by Layer 1/2
generic types with variant labels.

| Measurement            | Covered By                  | Notes                                               |
| ---------------------- | --------------------------- | --------------------------------------------------- |
| Species count          | COUNTER8 / COUNTER16        | Acoustic or camera-trap triggered                   |
| Activity index         | UINT8                       | Derived from accelerometer (livestock monitoring)   |
| Beehive weight         | WEIGHT_G / WEIGHT_G_EXPANDED| Continuous monitoring for colony health             |
| Beehive internal temp  | TEMPERATURE                 | Via variant label                                   |
| Beehive acoustics      | SOUND_DBA + Sound Frequency | Bee health indicator                                |

No dedicated Layer 3 types needed — the generic and unit-based types cover
these with appropriate variant labels.

### 3.5 Built Environment

#### 3.5.1 Power and Electrical Monitoring

| Type                | Built On    | Bits | Range            | Resolution | Notes                                   |
| ------------------- | ----------- | ---- | ---------------- | ---------- | --------------------------------------- |
| Supply Voltage      | VOLTAGE_MV  | 16   | 0–65535 mV       | 1 mV       | Solar panel, load monitoring            |
| Current             | CURRENT_MA  | 16   | 0–65535 mA       | 1 mA       | Load monitoring                         |
| Power               | POWER_W     | 16   | 0–65535 W        | 1 W        | Computed from V×I or CT clamp           |
| Energy Totaliser    | ENERGY_WH   | 16   | 0–65535 Wh       | 1 Wh       | Wrapping counter                        |
| Mains Frequency     | UINT8       | 8    | 45.0–65.0 Hz     | variant    | ×10 for 0.1 Hz resolution; zero-crossing|

Notes:

- Voltage is already present in the Health TLV as supply_mv. As a data field,
  it serves solar panel monitoring, load monitoring, grid monitoring.
- Current + voltage → power → energy is a natural derivation chain (§2.9).
- Energy is a wrapping totaliser like flow volume.
- Mains frequency monitoring is niche but relevant for grid stability sensing.
  Covered by UINT8 with variant-defined scaling (value = 45.0 + raw × 0.1).

#### 3.5.2 Structural Health (Gaps Identified)

| Measurement              | Units            | Typical Range | Covered By            | Notes                                    |
| ------------------------ | ---------------- | ------------- | --------------------- | ---------------------------------------- |
| Strain                   | µε (microstrain) | ±10,000       | INT16                 | Foil strain gauges, fibre Bragg gratings |
| Tilt / inclination       | degrees          | ±90°          | TILT (Layer 2)        | Landslide, structural monitoring         |
| Crack width              | mm               | 0–50          | DISTANCE_MM_SHORT     | LVDT or potentiometric                   |
| Vibration RMS            | g                | 0–10          | ACCEL_G               | Processed from accelerometer FFT/RMS     |
| Vibration frequency      | Hz               | 0–1000        | UINT16                | Dominant frequency from FFT              |

Notes:

- Strain is structural health monitoring — bridges, buildings, dams. INT16
  covers the range. Dedicated type not justified until demand emerges.
- Tilt is critical for landslide monitoring and is well served by the TILT
  Layer 2 type.
- Vibration RMS/peak is a processed derivative of raw acceleration data. ACCEL_G
  covers the encoding.
- Crack width is very niche. DISTANCE_MM_SHORT handles it.

#### 3.5.3 Indoor Environment (Gaps Identified)

| Measurement         | Covered By               | Notes                                               |
| ------------------- | ------------------------ | --------------------------------------------------- |
| Occupancy count     | UINT8                    | PIR arrays, thermal arrays, radar                   |
| Motion detected     | BIT1 or NULL             | Binary: motion yes/no                               |
| Door state          | BIT1                     | Open/closed (reed switch, hall effect)              |
| Vibration / tamper  | BIT1                     | Tamper alert flag                                   |
| Comfort index (PMV) | INT8                     | Predicted Mean Vote: -3 to +3, derivable            |
| Presence            | BIT1 or NULL             | Room occupied yes/no                                |
| Water leak          | BIT1                     | Floor-level detection (binary)                      |
| Water leak depth    | DISTANCE_MM_SHORT        | If depth measurement available                      |

Notes:

- Indoor environment measurements are largely served by existing types and
  Layer 1 generics. The CO₂ field (existing AQ Gas) doubles as an occupancy
  proxy.
- PMV/PPD comfort indices are derivable from temperature, humidity, air speed,
  clothing, and metabolic rate (§2.9). Not a dedicated type.
- Aqara, Xiaomi, Shelly, and similar smart home sensors produce most of these
  measurements.

### 3.6 Motion, Vibration, and Mechanical

| Type                            | Built On         | Bits | Range            | Resolution | Notes                                      |
| ------------------------------- | ---------------- | ---- | ---------------- | ---------- | ------------------------------------------ |
| Acceleration (per-axis or mag)  | ACCEL_G          | 10   | ±16g             | ~0.03g     | ADXL345, MPU6050, LIS3DH, BMI270          |
| Acceleration (expanded)         | ACCEL_G_EXPANDED | 16   | ±32g             | ~0.001g    | High-precision structural monitoring       |
| Tilt / inclination              | TILT             | 8    | -90 to +90°      | ~0.7°      | SCA100T, SCL3300, derived from accel       |
| Rotation / RPM                  | RPM              | 14   | 0–16,383 RPM     | 1 RPM      | Hall effect, optical encoders              |
| Speed / velocity (generic)      | SPEED_MS         | 7    | 0–63.5 m/s       | 0.5 m/s    | Derived (GPS, wheel, ultrasonic)           |

Notes:

- Acceleration: many sensors output 3-axis, but for telemetry often just
  magnitude or per-axis max is transmitted. The variant label distinguishes
  (e.g., `accel_x`, `accel_y`, `accel_z`, `accel_magnitude`).
- Vibration RMS/peak is a processed derivative of raw acceleration — same
  encoding, different variant label and temporal mode (§2.4: peak mode).
- RPM is relevant for wind turbines, machinery monitoring. Low priority for
  environmental deployments.

### 3.7 Electromagnetic

| Type                   | Built On    | Bits | Range             | Resolution | Notes                                               |
| ---------------------- | ----------- | ---- | ----------------- | ---------- | --------------------------------------------------- |
| Magnetic field (axis)  | MAGNETIC_UT | 10   | -512 to +511 µT   | 1 µT       | QMC5883L, LIS3MDL; per-axis via variant label       |
| RF power / EMF         | INT8        | 8    | -128 to +127 dBm  | 1 dBm      | Spectrum analysers, EMF meters; covered by INT8     |

Notes:

- Magnetic field is useful for vehicle detection (parking), compass heading,
  geomagnetic monitoring. Per-axis measurements use three variant slots
  (`mag_x`, `mag_y`, `mag_z`).
- RF power monitoring overlaps with the existing Link field (RSSI). As a data
  measurement (not a link quality indicator), INT8 covers it.
- Electric field strength (V/m) is a known gap — relevant for power line
  monitoring and EMC compliance, but no common low-cost sensor IC exists in the
  target deployment class. Revisit if affordable E-field sensors emerge.

### 3.8 Rate of Change (Cross-Domain)

Rate of change is not a domain — it is a temporal interpretation mode (§2.4).
However, certain rate-of-change measurements are common enough to warrant
specific Layer 3 type definitions for clarity and to set encoding ranges.

| Type              | Built On | Bits | Range           | Resolution  | Notes                               |
| ----------------- | -------- | ---- | --------------- | ----------- | ----------------------------------- |
| Temperature Rate  | INT8     | 8    | ±12.7 °C/min    | 0.1 °C/min  | Fire/frost detection (J.17.9)       |
| Pressure Rate     | INT8     | 8    | ±12.7 hPa/hr    | 0.1 hPa/hr  | Storm prediction                    |

Notes:

- Temperature rate is highest priority (fire/frost detection).
- Pressure rate is useful for storm prediction — a rapid drop (>5 hPa/3hr)
  indicates approaching severe weather.
- Other rate measurements (PM2.5 rate, humidity rate) can use INT8 with
  variant-defined scaling. Dedicated types are not needed for these lower
  priority cases.
- All rate-of-change values are computed sensor-side from consecutive readings.
  No extra hardware needed — it is a firmware function.

---

## 4. Device Coverage (What Sensors Produce)

This section catalogues sensor chips, modules, breakout boards, and integrated
products commonly used in the deployments iotdata targets. It provides the
practical constraints — ranges, accuracy, interfaces, cost tiers — that feed
back into encoding decisions (Section 2) and domain type definitions
(Section 3).

**What the protocol cares about:** The sensor's output range and resolution
constrain the minimum useful encoding. If the best available sensor for a
measurement has ±2°C accuracy, encoding at 0.01°C resolution wastes bits.

**What the protocol does NOT care about:** Sensor interface (I2C, SPI, UART,
SDI-12), power consumption, physical form factor. These are deployment concerns.
They are documented here for reference but do not affect encoding design.

### 4.1 Sensor Chips and Modules

These are ICs or small modules typically connected to a microcontroller (ESP32,
STM32, nRF52, etc.) via I2C, SPI, UART, SDI-12, or analog output.

#### 4.1.1 Temperature

| Sensor               | Manufacturer                  | Interface | Range & Accuracy                                          | Notes                                                                  |
| -------------------- | ----------------------------- | --------- | --------------------------------------------------------- | ---------------------------------------------------------------------- |
| DS18B20              | Maxim/Analog Devices          | 1-Wire    | -55 to +125°C, ±0.5°C (0.0625°C res)                      | Ubiquitous, waterproof probe versions, parasitic power, multi-drop bus |
| TMP117               | Texas Instruments             | I2C       | -55 to +150°C, ±0.1°C (0.0078°C res)                      | High accuracy, NIST-traceable, low power                               |
| TMP36                | Analog Devices                | Analog    | -40 to +125°C, ±2°C                                       | Simple analog output, cheap, no calibration needed                     |
| MAX31865             | Maxim/Analog Devices          | SPI       | PT100/PT1000 RTD interface, -200 to +850°C                | RTD-to-digital converter, very high precision                          |
| MAX31855             | Maxim/Analog Devices          | SPI       | K-type thermocouple, -200 to +1350°C, ±2°C                | Cold junction compensated, also J/N/T variants                         |
| MCP9808              | Microchip                     | I2C       | -40 to +125°C, ±0.25°C (0.0625°C res)                     | Programmable alert thresholds                                          |
| Si7051               | Silicon Labs                  | I2C       | -40 to +125°C, ±0.1°C (0.01°C res)                        | Very high precision, tiny package                                      |
| NTC thermistor (10k) | Various (Murata, TDK, Vishay) | Analog    | -40 to +125°C, accuracy depends on calibration            | Cheapest option, needs ADC + Steinhart-Hart                            |

**Encoding implications:** Standard TEMPERATURE (9 bits, -40 to +80°C, 0.25°C)
covers most environmental sensors. TMP117 and Si7051 exceed the encoding
resolution — TEMPERATURE_EXPANDED (12 bits, 0.05°C) is needed for these.
MAX31865 and MAX31855 exceed the range — these are industrial/lab sensors
outside iotdata's primary scope but serviceable via TEMPERATURE_EXPANDED
(-60 to +150°C).

#### 4.1.2 Humidity (typically includes temperature)

| Sensor                 | Manufacturer      | Interface   | Range & Accuracy                                                       | Notes                                              |
| ---------------------- | ----------------- | ----------- | ---------------------------------------------------------------------- | -------------------------------------------------- |
| SHT40 / SHT41 / SHT45  | Sensirion         | I2C         | RH: 0–100%, ±1.8% (SHT40) to ±1% (SHT45); Temp: -40 to +125°C, ±0.2°C  | Current generation, replaces SHT3x. Very low power |
| SHT31                  | Sensirion         | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.3°C                           | Previous gen, still widely used                    |
| HDC1080                | Texas Instruments | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.2°C                           | Low power, cheap, integrated heater                |
| DHT22 / AM2302         | Aosong            | 1-Wire-like | RH: 0–100%, ±2–5%; Temp: -40 to +80°C, ±0.5°C                          | Very cheap, slow (2s sample), unreliable long-term |
| DHT11                  | Aosong            | 1-Wire-like | RH: 20–80%, ±5%; Temp: 0–50°C, ±2°C                                    | Even cheaper, limited range, integer resolution    |
| AHT20                  | Aosong            | I2C         | RH: 0–100%, ±2%; Temp: -40 to +85°C, ±0.3°C                            | Cheap I2C upgrade from DHT series                  |
| SHTC3                  | Sensirion         | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.2°C                           | Ultra-low power, wearable/battery focus            |
| HIH6130                | Honeywell         | I2C         | RH: 10–90%, ±4%; Temp: -25 to +85°C                                    | Industrial grade, condensation resistant           |
| BME280                 | Bosch             | I2C/SPI     | See multi-sensor section (§4.1.4)                                      |                                                    |

**Encoding implications:** Existing HUMIDITY (7 bits, 0–100%, 1%) matches all
sensors listed. Even the best sensor (SHT45, ±1%) is coarser than 1% resolution.
No changes needed.

#### 4.1.3 Barometric Pressure (typically includes temperature)

| Sensor    | Manufacturer       | Interface | Range & Accuracy                                                       | Notes                                       |
| --------- | ------------------ | --------- | ---------------------------------------------------------------------- | ------------------------------------------- |
| BME280    | Bosch              | I2C/SPI   | See multi-sensor section (§4.1.4)                                      |                                             |
| BMP280    | Bosch              | I2C/SPI   | 300–1100 hPa, ±1 hPa (0.16 Pa res); Temp: -40 to +85°C                 | Like BME280 minus humidity                  |
| BMP390    | Bosch              | I2C/SPI   | 300–1250 hPa, ±0.5 hPa (±0.03 hPa rel); Temp: -40 to +85°C             | Higher accuracy than BMP280, lower noise    |
| MS5611    | TE Connectivity    | I2C/SPI   | 10–1200 mbar, ±1.5 mbar (0.012 mbar res); Temp: -40 to +85°C           | Popular in altimeters, very high resolution |
| LPS22HB   | STMicroelectronics | I2C/SPI   | 260–1260 hPa, ±0.1 hPa (abs); Temp included                            | Low power, high accuracy                    |
| DPS310    | Infineon           | I2C/SPI   | 300–1200 hPa, ±1 hPa (0.002 hPa precision); Temp: -40 to +85°C         | Very high precision, good for indoor nav    |
| SPL06-007 | Goertek            | I2C/SPI   | 300–1100 hPa, ±1 hPa; Temp: -40 to +85°C                               | Budget alternative to BMP280                |
| HP206C    | HopeRF             | I2C       | 300–1200 hPa; Temp; Altitude computed                                  | Integrated altimeter function               |

**Encoding implications:** The BMP280 and most sensors support 300–1100+ hPa.
The existing PRESSURE (8 bits, 850–1105 hPa) is too narrow — confirmed by
multiple sensors going down to 300 hPa. PRESSURE_EXPANDED (10 bits, 300–1100
hPa) is essential (J.1).

#### 4.1.4 Multi-Sensor Environment (temp + humidity + pressure)

| Sensor         | Manufacturer       | Interface | Range & Accuracy                                                        | Notes                                                             |
| -------------- | ------------------ | --------- | ----------------------------------------------------------------------- | ----------------------------------------------------------------- |
| BME280         | Bosch              | I2C/SPI   | Temp: -40 to +85°C, ±1°C; RH: 0–100%, ±3%; Press: 300–1100 hPa, ±1 hPa  | The classic IoT weather sensor. Ubiquitous, cheap, well-supported |
| BME680         | Bosch              | I2C/SPI   | As BME280 + Gas resistance (VOC proxy), IAQ index                       | Adds MOX gas sensor for indoor air quality                        |
| BME688         | Bosch              | I2C/SPI   | As BME680 + AI gas scanning mode                                        | Can identify gas signatures with Bosch AI library                 |
| ENS160 + AHT21 | ScioSense + Aosong | I2C       | AQI, TVOC (0–65000 ppb), eCO₂ (400–65000 ppm) + RH + Temp               | Common combo board (e.g. SparkFun)                                |

**Encoding implications:** The BME280's triple output (temp + humidity +
pressure) maps directly to the existing Environment bundle (24 bits). This is
the canonical example of why bundles work — the sensor always outputs all three.
BME680/688 add gas, which maps to AQ Gas fields.

#### 4.1.5 Air Quality — Particulate Matter

| Sensor              | Manufacturer | Interface | Range & Accuracy                                            | Notes                                                         |
| ------------------- | ------------ | --------- | ----------------------------------------------------------- | ------------------------------------------------------------- |
| PMS5003 / PMS7003   | Plantower    | UART      | PM1.0, PM2.5, PM10: 0–500 µg/m³; particle counts per 0.1L   | Most common low-cost PM sensor. Fan-based, ~100mA active      |
| PMS5003T            | Plantower    | UART      | As PMS5003 + Temp + Humidity                                | Integrated T/H, saves external sensor                         |
| SPS30               | Sensirion    | I2C/UART  | PM1.0, PM2.5, PM4, PM10: 0–1000 µg/m³; mass + number conc.  | Higher quality than Plantower, auto-clean, longer life (~8yr) |
| SEN5x (SEN54/SEN55) | Sensirion    | I2C       | PM1–10 + VOC index + NOx index + Temp + RH (SEN55)          | All-in-one environmental module                               |
| PMSA003I            | Plantower    | I2C       | As PMS5003 but I2C interface                                | Easier integration than UART versions                         |
| SDS011              | Nova Fitness | UART      | PM2.5, PM10: 0–999.9 µg/m³                                  | Older design, larger, cheaper, noisier                        |
| HM3301              | Seeed/Grove  | I2C       | PM1.0, PM2.5, PM10: 1–500 µg/m³                             | Grove ecosystem compatible                                    |

**Encoding implications:** Existing AQ PM encoding (0–1275 µg/m³, 5 µg/m³
resolution) covers all listed sensors. The SPS30 goes to 1000 µg/m³ — within
range. Resolution of 5 µg/m³ is appropriate given sensor accuracy (typically
±10 µg/m³ for low-cost sensors). No changes needed.

#### 4.1.6 Air Quality — Gas Sensors

| Sensor                                       | Manufacturer        | Interface   | Range & Accuracy                                     | Notes                                                                  |
| -------------------------------------------- | ------------------- | ----------- | ---------------------------------------------------- | ---------------------------------------------------------------------- |
| SCD41 / SCD40                                | Sensirion           | I2C         | CO₂: 400–5000 ppm, ±50 ppm; Temp; RH                 | True NDIR CO₂ (photoacoustic), gold standard for indoor CO₂            |
| SCD30                                        | Sensirion           | I2C/UART    | CO₂: 400–10,000 ppm, ±30 ppm; Temp; RH               | Older NDIR module, larger, higher power                                |
| MH-Z19B / MH-Z19C                            | Winsen              | UART/PWM    | CO₂: 400–5000 ppm (up to 10,000), ±50 ppm            | Cheap NDIR CO₂, widely used in DIY projects                            |
| SGP41                                        | Sensirion           | I2C         | VOC index: 0–500; NOx index: 0–500                   | MOX sensor, outputs processed index values, needs algorithm library    |
| SGP30                                        | Sensirion           | I2C         | TVOC: 0–60,000 ppb; eCO₂: 400–60,000 ppm             | Older than SGP41, direct ppb/ppm output                                |
| CCS811                                       | ScioSense (was AMS) | I2C         | eCO₂: 400–8192 ppm; TVOC: 0–1187 ppb                 | MOX, estimated CO₂ (not true NDIR), being discontinued                 |
| ENS160                                       | ScioSense           | I2C         | AQI (1–5); TVOC: 0–65,000 ppb; eCO₂: 400–65,000 ppm  | CCS811 successor, multi-element MOX                                    |
| MQ series (MQ-2, MQ-4, MQ-7, MQ-135, MQ-137)| Winsen/Hanwei        | Analog      | Various gases, ranges depend on model                | Very cheap, analog, high power (~150mA heater), poor selectivity       |
| MICS-6814                                    | SGX Sensortech      | Analog      | CO: 1–1000 ppm; NO₂: 0.05–10 ppm; NH₃: 1–500 ppm     | Three-channel MOX, better than MQ series                               |
| ZE07-CO                                      | Winsen              | UART/Analog | CO: 0–500 ppm, ±3 ppm                                | Electrochemical, much better selectivity than MOX                      |
| ZE25-O3                                      | Winsen              | UART/Analog | O₃: 0–10 ppm                                         | Electrochemical ozone sensor                                           |
| ZMOD4410                                     | Renesas             | I2C         | TVOC, IAQ, eCO₂                                      | Low power MOX, firmware-based gas detection                            |

**Encoding implications:** Existing AQ Gas field covers the most common gases.
The MQ-4 (CH₄ to 10,000 ppm) and MQ-137/MICS-6814 (NH₃ to 500 ppm) produce
values that need PPM or extended AQ Gas mask (§3.1.4 decision). MQ sensors
have high power draw and poor selectivity — suitable for alarm thresholds but
not precision monitoring.

#### 4.1.7 Soil Sensors

| Sensor                             | Manufacturer                      | Interface  | Range & Accuracy                                            | Notes                                                               |
| ---------------------------------- | --------------------------------- | ---------- | ----------------------------------------------------------- | ------------------------------------------------------------------- |
| Teros 12                           | Meter Group                       | SDI-12/DDI | VWC: 0–100%; EC: 0–20,000 µS/cm (bulk); Temp: -40 to +60°C  | Research grade, capacitive FDR. Expensive (~$150+)                  |
| Teros 21                           | Meter Group                       | SDI-12/DDI | Water potential: -100,000 to -9 kPa; Temp: -40 to +60°C     | Matric potential sensor (MPS-6 successor)                           |
| 5TE                                | Meter/Decagon                     | SDI-12     | VWC, EC, Temp                                               | Older Decagon design, still common in field                         |
| Capacitive soil moisture (generic) | Various (DFRobot, Adafruit, etc.) | Analog     | VWC: ~0–100% (uncalibrated)                                 | Very cheap (£2–5), no calibration, corrosion-resistant vs resistive |
| Watermark 200SS                    | Irrometer                         | Resistance | Water potential: 0–239 kPa (centibars)                      | Granular matrix sensor, widely used in irrigation                   |
| SoilWatch 10                       | Pino-Tech                         | Analog/I2C | VWC: 0–60% calibrated                                       | Open-source-friendly, well-documented                               |
| Chirp!                             | Catnip Electronics                | I2C        | Capacitive moisture (raw), Temp, Light                      | Open-source, good for hobby use                                     |

**Encoding implications:** Teros 12 outputs VWC + EC + Temp — this is the
canonical Soil Bundle pattern. EC range 0–20,000 µS/cm fits comfortably in
CONDUCTIVITY (16 bits, 0–65535). Cheap capacitive sensors output uncalibrated
0–100% — PERCENTAGE (7 bits) is adequate. Watermark 200SS outputs water
potential in centibars — needs conversion to kPa for Soil Water Potential type.

#### 4.1.8 Water Quality Sensors

| Sensor                   | Manufacturer     | Interface | Range & Accuracy                                  | Notes                                                             |
| ------------------------ | ---------------- | --------- | ------------------------------------------------- | ----------------------------------------------------------------- |
| EZO-pH                   | Atlas Scientific | I2C/UART  | pH: 0.001–14.000, ±0.002                          | Lab-grade, isolated, calibratable. Board (~$40) + probe (~$30–80) |
| EZO-EC                   | Atlas Scientific | I2C/UART  | EC: 0.07–500,000 µS/cm; TDS; Salinity; SG         | K1.0 or K10 probe depending on range                              |
| EZO-DO                   | Atlas Scientific | I2C/UART  | DO: 0–100 mg/L, ±0.05 mg/L                        | Galvanic probe, needs periodic membrane replacement               |
| EZO-ORP                  | Atlas Scientific | I2C/UART  | ORP: -1019.9 to +1019.9 mV                        | Platinum electrode                                                |
| EZO-RTD                  | Atlas Scientific | I2C/UART  | Temp: -200 to +850°C (PT100/1000)                 | For temperature compensation of other EZO sensors                 |
| EZO-HUM                  | Atlas Scientific | I2C/UART  | RH + Temp + Dew point                             | Humidity module in same ecosystem                                 |
| SEN0161                  | DFRobot          | Analog    | pH: 0–14, ±0.1                                    | Budget pH, analog output, less precise than Atlas                 |
| SEN0244                  | DFRobot          | Analog    | EC: 1–20,000 µS/cm (K=1) or up to 200,000 (K=10)  | Budget EC, analog output                                          |
| SEN0189                  | DFRobot          | Analog    | Turbidity: 0–3000 NTU (approx)                    | IR-based, analog, rough readings                                  |
| SEN0237                  | DFRobot          | Analog    | DO: 0–20 mg/L                                     | Budget dissolved oxygen                                           |
| Gravity TDS Meter        | DFRobot          | Analog    | TDS: 0–1000 ppm                                   | Very cheap, derived from conductivity                             |
| TSS/turbidity (Seapoint) | Seapoint Sensors | Analog    | Turbidity: 0–750 FTU (up to 1500)                 | Research-grade, underwater, expensive                             |

**Encoding implications:** Atlas EZO-pH (±0.002) exceeds Water pH (8 bits,
~0.055 resolution) — Water pH Expanded (10 bits, ~0.014) serves this. EZO-EC
range goes to 500,000 µS/cm, exceeding CONDUCTIVITY (16 bits, 65535 µS/cm) —
very high EC (seawater and above) needs CONDUCTIVITY_COARSE or out-of-band
scaling. EZO-DO range (0–100 mg/L) exceeds Water DO (8 bits, 0–25.5 mg/L) —
most environmental dissolved oxygen is 0–15 mg/L so this is acceptable for
target deployments. Turbidity sensors confirm the log-scale need (0.1–10,000
NTU dynamic range across devices).

#### 4.1.9 Wind

| Sensor                           | Manufacturer      | Interface    | Range & Accuracy                                                | Notes                                               |
| -------------------------------- | ----------------- | ------------ | --------------------------------------------------------------- | --------------------------------------------------- |
| Davis 6410 anemometer            | Davis Instruments | Pulse/Analog | Speed: 0–89 m/s (pulse); Direction: 0–360° (pot)                | Cup anemometer + vane, standard amateur weather     |
| Inspeed Vortex                   | Inspeed           | Pulse/Analog | Speed: 0–80+ m/s; Direction: 0–360°                             | Alternative to Davis                                |
| Modern Device Rev P              | Modern Device     | Analog       | Airspeed: ~0–40 m/s                                             | Hot-wire anemometer, no moving parts, fragile       |
| Ultrasonic (Gill, FT, Lufft)     | Various           | UART/RS485   | Speed: 0–60+ m/s; Dir: 0–360°; Temp                             | No moving parts, 2D or 3D, expensive (£200+)        |
| Calypso ULP                      | Calypso           | BLE/UART     | Speed: 0–30 m/s; Dir: 0–360°                                    | Ultra-low power ultrasonic, battery operated        |
| SEN-15901 (SparkFun weather kit) | SparkFun/Argent   | Pulse/Analog | Speed: 0–55 m/s; Dir: 0–360° (8 pos); Rain: 0.2794mm/tip        | Cheap weather station kit, cups + vane + rain gauge |

**Encoding implications:** Davis 6410 goes to 89 m/s — exceeds existing Wind
Speed (7 bits, 0–63.5 m/s). SPEED_MS_EXPANDED (10 bits, 0–127.5 m/s)
confirms J.2 fix. Direction sensors are all 0–360° — existing ANGLE_360 (8
bits) is adequate.

#### 4.1.10 Rain

| Sensor                   | Manufacturer         | Interface    | Range & Accuracy                               | Notes                                                    |
| ------------------------ | -------------------- | ------------ | ---------------------------------------------- | -------------------------------------------------------- |
| Hydreon RG-15            | Hydreon              | UART/Digital | Accumulation (mm), rain intensity, event flag  | Optical, solid-state, no tipping bucket, low maintenance |
| Hydreon RG-9             | Hydreon              | Digital      | Rain yes/no, 7 intensity levels                | Simpler/cheaper than RG-15, digital output               |
| Tipping bucket (generic) | Davis, Argent, Misol | Pulse        | 0.2–0.5 mm per tip depending on model          | Standard design, reed switch, simple pulse counting      |
| Davis 6463M              | Davis Instruments    | Pulse        | 0.2 mm per tip                                 | AeroCone collector, improved accuracy in wind            |

**Encoding implications:** Tipping bucket produces pulse counts — COUNTER16 is
the natural encoding for accumulation. Hydreon RG-9's 7 intensity levels map
to UINT4 (categorical). Hydreon RG-15 outputs continuous rain rate — existing
Rain Rate (8 bits, 0–255 mm/hr) is adequate.

#### 4.1.11 Solar / Light / UV

| Sensor                  | Manufacturer       | Interface     | Range & Accuracy                                                      | Notes                                                            |
| ----------------------- | ------------------ | ------------- | --------------------------------------------------------------------- | ---------------------------------------------------------------- |
| BH1750                  | Rohm               | I2C           | Lux: 1–65535 lux, 1 lux res                                           | Very common, cheap, spectral response close to human eye         |
| TSL2591                 | AMS/OSRAM          | I2C           | Lux: 188 µlux–88,000 lux; separate visible + IR channels              | Very wide dynamic range (600M:1), high sensitivity               |
| VEML7700                | Vishay             | I2C           | Lux: 0–120,000 lux (with gain adjustment)                             | Good all-round ambient light, auto gain                          |
| MAX44009                | Maxim              | I2C           | Lux: 0.045–188,000 lux                                                | Ultra-wide range, ultra-low power (0.65 µA)                      |
| LTR-390UV               | Lite-On            | I2C           | UV Index + ALS (ambient light) in one                                 | Dual UV + visible, good for outdoor                              |
| GUVA-S12SD              | GenUV              | Analog        | UV-A irradiance: 240–370 nm band, ~0–15 UV index (derived)            | Cheap UV-A photodiode, analog output                             |
| VEML6075                | Vishay             | I2C           | UVA + UVB irradiance, UV Index computed                               | Separate UVA/UVB channels, more accurate UV index                |
| SI1145                  | Silicon Labs       | I2C           | UV index (approx), Visible, IR                                        | Digital proximity/UV/ambient light, UV is estimated not measured |
| ML8511                  | Lapis              | Analog        | UV intensity: ~0–15 mW/cm²                                            | Analog UV sensor, simple                                         |
| Apogee SP-510 / SP-610  | Apogee Instruments | Analog/SDI-12 | Solar irradiance (pyranometer): 0–2000 W/m²                           | Thermopile-based, ISO 9060 compliant, research grade             |
| Davis 6450              | Davis Instruments  | Analog        | Solar irradiance: 0–1800 W/m², ±5%                                    | Silicon photodiode pyranometer, consumer grade                   |
| Apogee SQ-520           | Apogee Instruments | SDI-12/Analog | PAR: 0–4000 µmol/m²/s                                                 | Quantum sensor (PAR), research grade                             |
| LI-COR LI-190R          | LI-COR             | Analog        | PAR: 0–3000+ µmol/m²/s                                                | Gold standard PAR sensor, expensive (~$500+)                     |
| TCS34725                | AMS/OSRAM          | I2C           | RGBC (Red, Green, Blue, Clear): 16-bit per channel; colour temp; lux  | Colour sensor, IR blocking filter, good for colour matching      |
| APDS-9960               | Broadcom           | I2C           | RGBC + proximity + gesture                                            | Multi-function: colour, proximity (0–20 cm), gesture recognition |

**Encoding implications:** VEML7700 and MAX44009 both exceed 65,535 lux —
confirms the need for LUX_LOG (8 bits, logarithmic, up to ~120,000 lux). BH1750
tops out at 65,535 — fits linear LUX (16 bits). UV sensors all output 0–15 UV
Index — existing encoding is adequate. Apogee pyranometers go to 2000 W/m² —
existing Irradiance (10 bits, 0–1023 W/m²) needs J.11 fix (11 bits) or a
variant with wider range. PAR sensors go to 4000 µmol/m²/s — UINT16 with
variant scaling handles this.

#### 4.1.12 Distance / Ranging

| Sensor            | Manufacturer       | Interface       | Range & Accuracy                | Notes                                                   |
| ----------------- | ------------------ | --------------- | ------------------------------- | ------------------------------------------------------- |
| VL53L0X           | STMicroelectronics | I2C             | 30–1200 mm, ±3%                 | Time-of-Flight laser, small, cheap, 940nm               |
| VL53L1X           | STMicroelectronics | I2C             | 40–4000 mm, ±3%                 | Longer range than L0X, ROI selection                    |
| VL53L4CD          | STMicroelectronics | I2C             | 1–1300 mm                       | Close-range ToF, high accuracy                          |
| HC-SR04           | Various            | Trig/Echo       | 2–400 cm, ±3 mm                 | Ultrasonic, ubiquitous, cheap, 40kHz                    |
| JSN-SR04T         | Various            | Trig/Echo/UART  | 20–600 cm                       | Waterproof ultrasonic, good for water level             |
| MB7389 (MaxSonar) | MaxBotix           | Analog/PWM/UART | 30–500 cm, 1 cm res             | Weather-resistant ultrasonic, IP67                      |
| TFmini Plus       | Benewake           | UART/I2C        | 10 cm–12 m, ±1–6 cm             | LiDAR, works outdoors, 100 Hz update                    |
| TFmini-S          | Benewake           | UART            | 10 cm–12 m                      | Smaller, cheaper variant                                |
| TF-Luna           | Benewake           | UART/I2C        | 20 cm–8 m, ±2 cm                | Cheapest LiDAR option (~$10)                            |
| US-100            | Various            | UART/Trig-Echo  | 2–450 cm                        | Ultrasonic with temp compensation                       |
| GP2Y0A21YK0F      | Sharp              | Analog          | 10–80 cm                        | IR proximity, analog, cheap, noisy                      |

**Encoding implications:** Short-range sensors (VL53L0X/L1X) need mm precision
— DISTANCE_MM_SHORT (10 bits, 0–1023 mm) or DISTANCE_MM (16 bits). Medium-range
sensors (HC-SR04, MaxSonar) work in cm — existing DEPTH/DISTANCE_CM (10 bits,
0–1023 cm) covers most. LiDAR up to 12m = 1200 cm — exceeds 1023 cm, needs
DISTANCE_CM_EXPANDED (16 bits).

#### 4.1.13 Weight / Load

| Sensor               | Manufacturer                     | Interface            | Range & Accuracy                                 | Notes                                                                        |
| -------------------- | -------------------------------- | -------------------- | ------------------------------------------------ | ---------------------------------------------------------------------------- |
| HX711                | Avia Semiconductor               | Digital (clock/data) | 24-bit ADC for load cells, 10 or 80 SPS          | The standard load cell ADC, cheap, everywhere                                |
| NAU7802              | Nuvoton                          | I2C                  | 24-bit ADC for load cells, up to 320 SPS         | Better than HX711 — I2C, lower noise, configurable                           |
| Load cell (bar type) | Various (SparkFun, CZL635, etc.) | Analog (via HX711)   | 1 kg, 5 kg, 10 kg, 20 kg, 50 kg, 100 kg, 200 kg  | Wheatstone bridge, needs amplifier ADC. Beehive: typically 4×50 kg           |

**Encoding implications:** Beehive scale (4×50 kg = 200 kg max = 200,000 g) —
exceeds WEIGHT_G (16 bits, 65,535 g). Needs WEIGHT_G_EXPANDED (24 bits,
~16.7 million g). Typical precision is ~50 g for beehive applications, so
1 g resolution is more than adequate.

#### 4.1.14 Acceleration / Motion / Tilt

| Sensor    | Manufacturer       | Interface  | Range & Accuracy                                      | Notes                                            |
| --------- | ------------------ | ---------- | ----------------------------------------------------- | ------------------------------------------------ |
| ADXL345   | Analog Devices     | I2C/SPI    | 3-axis: ±2/4/8/16g, 13-bit, up to 3200 Hz             | Classic, cheap, tap/freefall/activity detection  |
| MPU6050   | InvenSense/TDK     | I2C        | 3-axis: ±2/4/8/16g; 3-axis gyro: ±250–2000°/s         | 6-DOF IMU, very common, DMP for orientation      |
| MPU9250   | InvenSense/TDK     | I2C/SPI    | As MPU6050 + 3-axis magnetometer (AK8963)             | 9-DOF IMU (discontinued but still available)     |
| LIS3DH    | STMicroelectronics | I2C/SPI    | 3-axis: ±2/4/8/16g, 10/12-bit, up to 5.3 kHz          | Low power (2 µA @ 1 Hz), FIFO buffer             |
| BMI270    | Bosch              | I2C/SPI    | 3-axis: ±2/4/8/16g; 3-axis gyro: ±125–2000°/s         | Ultra-low power IMU, wearable-grade              |
| ICM-20948 | InvenSense/TDK     | I2C/SPI    | 9-DOF: accel ±2–16g, gyro ±250–2000°/s, mag ±4900 µT  | MPU9250 successor, recommended for new designs   |
| ADXL355   | Analog Devices     | I2C/SPI    | 3-axis: ±2/4/8g, 20-bit, very low noise               | High precision, structural health monitoring     |
| SCA100T   | Murata             | SPI/Analog | Tilt: ±90° (single/dual axis), 0.001° res             | Dedicated inclinometer, industrial grade         |
| SCL3300   | Murata             | SPI        | Tilt: ±90° 3-axis, 0.001° res                         | High precision tilt, MEMS                        |

**Encoding implications:** All accelerometers support ±16g — ACCEL_G (10 bits,
±16g, ~0.03g) is a good match. ADXL355 (20-bit, ±8g) exceeds the encoding
precision — ACCEL_G_EXPANDED (16 bits, ~0.001g) serves this. SCA100T and
SCL3300 (0.001° tilt resolution) far exceed TILT (8 bits, ~0.7°) — for
structural monitoring, an expanded TILT type or ANGLE_SIGNED may be needed.

#### 4.1.15 Magnetometer

| Sensor   | Manufacturer       | Interface | Range & Accuracy                                  | Notes                                                       |
| -------- | ------------------ | --------- | ------------------------------------------------- | ----------------------------------------------------------- |
| QMC5883L | QST Corporation    | I2C       | 3-axis: ±8 or ±2 gauss (±800 / ±200 µT), 16-bit   | HMC5883L replacement, very common, cheap                    |
| HMC5883L | Honeywell          | I2C       | 3-axis: ±1.3–8.1 gauss, 12-bit                    | Classic, now hard to source, many counterfeits are QMC5883L |
| LIS3MDL  | STMicroelectronics | I2C/SPI   | 3-axis: ±4/8/12/16 gauss, 16-bit                  | Good performance, pairs well with LIS3DH                    |
| MMC5603  | MEMSIC             | I2C       | 3-axis: ±30 gauss, 0.0625 mG res                  | Set/reset for offset, high accuracy                         |
| BMM150   | Bosch              | I2C/SPI   | 3-axis: ±1300 µT (XY), ±2500 µT (Z)               | Often paired with BMI270 for 9-DOF                          |

**Encoding implications:** MAGNETIC_UT (10 bits, -512 to +511 µT) covers most
sensors. LIS3MDL at ±16 gauss (±1600 µT) exceeds the range — would need an
expanded variant or UINT16 with variant scaling. For typical environmental
applications (vehicle detection, compass), ±512 µT is adequate.

#### 4.1.16 Lightning

| Sensor | Manufacturer        | Interface | Range & Accuracy                                                                 | Notes                                                                                            |
| ------ | ------------------- | --------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| AS3935 | ScioSense (was AMS) | I2C/SPI   | Distance: 1–40 km (5 levels); Lightning/disturber/noise event; Estimated energy  | Only real option for IC-level lightning detection. Breakouts from SparkFun, DFRobot, CJMCU       |

**Encoding implications:** AS3935 distance is quantised to ~5 levels (not
continuous) — Lightning Distance (8 bits, 0–63 km, ~0.25 km) is generous.
Strike count accumulates between reads — COUNTER8 is natural.

#### 4.1.17 Sound / Noise

| Sensor                     | Manufacturer   | Interface  | Range & Accuracy                             | Notes                                             |
| -------------------------- | -------------- | ---------- | -------------------------------------------- | ------------------------------------------------- |
| ICS-43434                  | InvenSense/TDK | I2S        | Raw audio, -26 dBFS sensitivity, SNR 64 dBA  | MEMS digital mic, good for SPL computation        |
| SPH0645LM4H                | Knowles        | I2S        | Raw audio, SNR 65 dBA                        | Popular I2S MEMS mic, Adafruit breakout           |
| INMP441                    | InvenSense/TDK | I2S        | Raw audio, SNR 61 dBA                        | Very cheap I2S MEMS mic, common on ESP32 projects |
| MAX4466                    | Maxim          | Analog     | Amplified analog mic output                  | Electret mic with adjustable gain preamp          |
| MAX9814                    | Maxim          | Analog     | Amplified analog mic output, auto gain       | AGC electret mic amp, better than MAX4466         |
| SEN-15892 (Qwiic Loudness) | SparkFun       | I2C/Analog | Raw ADC value proportional to SPL            | Simple board, not calibrated dBA                  |

**Encoding implications:** These sensors output raw audio or uncalibrated
levels. SPL in dBA is computed in firmware (FFT/weighted RMS). SOUND_DBA (8
bits, 0–140 dBA, ~0.55 dBA) is appropriate — environmental sound rarely needs
finer than 1 dBA precision.

#### 4.1.18 Radiation

| Sensor                             | Manufacturer               | Interface       | Range & Accuracy                                         | Notes                                                                  |
| ---------------------------------- | -------------------------- | --------------- | -------------------------------------------------------- | ---------------------------------------------------------------------- |
| Geiger tube (SBM-20, J305, M4011)  | Various (Russian, Chinese) | Pulse           | CPM, ~1 µSv/h per ~150 CPM (SBM-20)                      | Needs HV supply (400V). Conversion factor varies by tube               |
| RadiationD v1.1                    | RH Electronics             | Pulse (via kit) | CPM via SBM-20 or similar                                | Arduino/ESP32 shield with HV supply                                    |
| BME688 + firmware                  | Bosch                      | I2C             | Some claim radon indication via gas scan                 | Very experimental, not validated for radon                             |

**Encoding implications:** Existing Radiation fields (CPM: 14 bits, 0–16383;
Dose: 14 bits, 0–163.83 µSv/h) are adequate for all listed sensors.

#### 4.1.19 GPS / Position

| Sensor           | Manufacturer   | Interface    | Range & Accuracy                                            | Notes                                                   |
| ---------------- | -------------- | ------------ | ----------------------------------------------------------- | ------------------------------------------------------- |
| NEO-6M           | u-blox         | UART         | Lat/Lon/Alt, ±2.5 m CEP, 5 Hz                               | Classic cheap GPS, still widely used                    |
| NEO-M8N          | u-blox         | UART/I2C     | Lat/Lon/Alt, ±2.5 m CEP, 10 Hz; GLONASS+GPS                 | Multi-constellation, more reliable fix                  |
| NEO-M9N          | u-blox         | UART/I2C/SPI | Lat/Lon/Alt, ±1.5 m CEP, 25 Hz; quad constellation          | Current gen, quad constellation                         |
| SAM-M10Q         | u-blox         | UART/I2C     | As M9N, smaller module                                      | Compact, low power                                      |
| L76K / L80 / L86 | Quectel        | UART         | Lat/Lon/Alt, ±2.5 m; GPS+GLONASS or GPS+BeiDou              | Budget alternative to u-blox                            |
| PA1010D          | CDTop/Adafruit | I2C/UART     | GPS+GLONASS, ±3 m                                           | Antenna integrated, I2C, Adafruit breakout              |
| ZED-F9P          | u-blox         | UART/I2C/SPI | RTK: ±1 cm with corrections; multi-band L1/L2               | High precision RTK, ~$200+, needs base station or NTRIP |
| BN-880           | Beitian        | UART/I2C     | GPS+GLONASS, ±2.5 m + compass (QMC5883L)                    | GPS + magnetometer combo module, popular in drones      |

**Encoding implications:** Existing Position (48 bits, ~1.2 m resolution)
matches consumer GPS accuracy (±2.5 m). RTK (±1 cm) far exceeds the encoding —
ZED-F9P deployments would need a higher-resolution position type, but this is
outside the primary target deployment class.

#### 4.1.20 Power Monitoring

| Sensor    | Manufacturer      | Interface     | Range & Accuracy                                                     | Notes                                                     |
| --------- | ----------------- | ------------- | -------------------------------------------------------------------- | --------------------------------------------------------- |
| INA219    | Texas Instruments | I2C           | Voltage: 0–26V; Current: via shunt, ±3.2A (at 0.1Ω); Power computed  | Bidirectional, programmable gain. Very common             |
| INA260    | Texas Instruments | I2C           | Voltage: 0–36V; Current: ±15A (integrated 2mΩ shunt); Power          | No external shunt needed, easy to use                     |
| INA226    | Texas Instruments | I2C           | Voltage: 0–36V; Current: via shunt, bidirectional; Power             | Higher precision than INA219, alert function              |
| ACS712    | Allegro           | Analog        | Current: ±5A, ±20A, or ±30A variants                                 | Hall effect, analog output, isolated, cheap but noisy     |
| ADS1115   | Texas Instruments | I2C           | 4-ch 16-bit ADC, ±0.256V to ±6.144V FSR                              | Not a current sensor but essential for precise voltage    |
| PZEM-004T | Peacefair         | UART (Modbus) | Voltage: 80–260V AC; Current: 0–100A (CT); Power; Energy; Freq; PF   | Mains monitoring with CT clamp, complete solution         |

**Encoding implications:** INA219 at 26V = 26,000 mV — fits VOLTAGE_MV (16
bits, 65,535 mV). INA260 at 36V = 36,000 mV — also fits. Current at 15A =
15,000 mA — fits CURRENT_MA (16 bits). PZEM-004T for AC mains outputs
voltage/current/power/energy/frequency — could justify a Power Bundle, but
mains monitoring is lower priority.

#### 4.1.21 LoRa / Radio Modules (Link Quality)

| Sensor            | Manufacturer | Interface          | Range & Accuracy                                      | Notes                                                       |
| ----------------- | ------------ | ------------------ | ----------------------------------------------------- | ----------------------------------------------------------- |
| SX1276 / SX1278   | Semtech      | SPI                | RSSI: -127 to 0 dBm; SNR: -20 to +10 dB; Packet RSSI  | LoRa transceiver, 137–525 MHz (SX1278) or 868/915 (SX1276)  |
| SX1262 / SX1268   | Semtech      | SPI                | RSSI, SNR, signal RSSI                                | Next-gen LoRa, lower power, better sensitivity              |
| RFM95W / RFM96W   | HopeRF       | SPI                | SX1276-based module, same outputs                     | Ready-made module, easier than bare IC                      |
| RYLR896 / RYLR998 | REYAX        | UART (AT commands) | RSSI, SNR                                             | Serial LoRa module, simple AT interface                     |
| E220-900T         | Ebyte        | UART               | RSSI                                                  | SX1262-based, various power/antenna options                 |

**Encoding implications:** Existing Link field (6 bits: RSSI -120–-60 dBm,
SNR -20–+10 dB) is a compact encoding for the most useful range. All LoRa
modules output compatible values.

### 4.2 Sensor Products (Integrated Systems)

Complete sensor products or sensor ecosystems — not bare ICs. These produce data
that might be ingested by an iotdata gateway or translated at a bridge.

#### 4.2.1 Weather Station Products

| Product                  | Manufacturer      | Connectivity                          | Measurements                                                                              | Notes                                                                |
| ------------------------ | ----------------- | ------------------------------------- | ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| Ecowitt GW2000 / HP2560  | Ecowitt           | Wi-Fi gateway, 915/868 MHz to sensors | Temp, humidity, wind, rain, solar, UV, pressure (via gateway)                             | Cheap ecosystem, many compatible sensors, custom protocol on 915/868 |
| Ecowitt WS90 (Wittboy)   | Ecowitt           | 915/868 MHz                           | Wind (ultrasonic), temp, humidity, rain (haptic), light, UV                               | All-in-one array, no moving parts, solar powered                     |
| Ecowitt WH51             | Ecowitt           | 915/868 MHz                           | Soil moisture: 0–100%                                                                     | Cheap capacitive soil probe, CR2032 battery                          |
| Ecowitt WN34             | Ecowitt           | 915/868 MHz                           | Temperature (waterproof probe)                                                            | Pool/soil/pipe temp, various probe lengths                           |
| Ecowitt WH45             | Ecowitt           | 915/868 MHz                           | CO₂: 400–5000 ppm; PM2.5; PM10; Temp; Humidity                                            | Indoor air quality 5-in-1                                            |
| Davis Vantage Vue        | Davis Instruments | 915 MHz proprietary                   | Temp, humidity, wind, rain, pressure                                                      | Reliable, professional amateur, all-in-one outdoor sensor suite      |
| Davis Vantage Pro2       | Davis Instruments | 915 MHz proprietary                   | As Vue + solar, UV; optional soil/leaf stations                                           | Modular, add-on sensors, widely used in agriculture                  |
| Davis AirLink            | Davis Instruments | Wi-Fi                                 | PM1, PM2.5, PM10 + AQI                                                                    | Pairs with Vantage ecosystem                                         |
| Tempest (WeatherFlow)    | WeatherFlow       | 915 MHz + Wi-Fi hub                   | Wind (ultrasonic), temp, humidity, pressure, rain (haptic), solar, UV, lightning (AS3935) | All-in-one, solar powered, haptic rain sensor, built-in lightning    |
| Ambient Weather WS-5000  | Ambient Weather   | 915 MHz + Wi-Fi                       | Temp, humidity, wind, rain, solar, UV, pressure                                           | Ecowitt OEM rebrand with different cloud service                     |
| Bresser 7-in-1           | Bresser           | 868 MHz                               | Temp, humidity, wind, rain, solar, UV                                                     | European alternative, 868 MHz                                        |
| MiSol weather sensors    | MiSol             | 433/868/915 MHz                       | Various: temp, humidity, wind, rain, soil                                                 | Very cheap, compatible with Ecowitt gateways in some models          |

#### 4.2.2 Soil / Agriculture Products

| Product                    | Manufacturer | Connectivity    | Measurements                                                        | Notes                                                  |
| -------------------------- | ------------ | --------------- | ------------------------------------------------------------------- | ------------------------------------------------------ |
| Meter Group Teros stations | Meter Group  | SDI-12 → logger | VWC, EC, temp, water potential (sensor dependent)                   | Research grade, ZL6 logger with cellular/Wi-Fi         |
| Meter Group Atmos 41       | Meter Group  | SDI-12          | All-in-one: wind, temp, RH, pressure, solar, rain, lightning, tilt  | All-in-one weather station for ag, SDI-12 output       |
| Dragino LSE01              | Dragino      | LoRaWAN         | Soil moisture + EC + temp (capacitive)                              | LoRaWAN soil sensor, solar powered, cheap (~$40)       |
| SenseCAP S2105             | Seeed Studio | LoRaWAN         | Soil moisture + temp                                                | LoRaWAN, IP66, RS485 sensor input                      |
| SenseCAP S2100             | Seeed Studio | LoRaWAN         | Data logger for any RS485/analog/GPIO sensor                        | Versatile LoRaWAN gateway for third-party sensors      |
| Milesight EM500 series     | Milesight    | LoRaWAN         | Various: soil moisture/temp/EC, CO₂, light, temp/humidity           | Industrial IoT LoRaWAN sensors, IP67, battery powered  |
| RAK10701 Field Tester      | RAKwireless  | LoRaWAN         | Coverage mapping (RSSI, SNR, GPS)                                   | Not a sensor; useful for LoRa field testing            |

#### 4.2.3 Water Quality Products

| Product                         | Manufacturer      | Connectivity            | Measurements                                                        | Notes                                                        |
| ------------------------------- | ----------------- | ----------------------- | ------------------------------------------------------------------- | ------------------------------------------------------------ |
| Atlas Scientific Wi-Fi Pool Kit | Atlas Scientific  | Wi-Fi (ESP32-based)     | pH, ORP, DO, EC, Temp (modular EZO boards)                          | Build-your-own combo, Whitebox carrier + EZO modules         |
| Atlas Scientific Gravity boards | Atlas Scientific  | Analog (Arduino)        | pH, EC, DO, ORP, Temp                                               | Budget line, analog output, less precise than EZO            |
| YSI EXO2                        | Xylem/YSI         | RS485/SDI-12/Bluetooth  | pH, EC, DO, turbidity, chlorophyll, fDOM, temp, depth (modular)     | Professional multi-parameter sonde, very expensive (~$10k+)  |
| In-Situ Aqua TROLL series       | In-Situ           | SDI-12/Modbus/Bluetooth | Various: pH, EC, DO, turbidity, pressure, temp, ORP                 | Professional, submersible, field-swappable sensors           |
| Hanna HI98194                   | Hanna Instruments | Bluetooth               | pH, EC, DO, pressure, temp                                          | Portable multiparameter meter, lab-grade                     |
| LilyGo T-HiGrow                 | LilyGo            | Wi-Fi (ESP32)           | Soil moisture, temp, humidity, light, salt (conductivity), battery  | Very cheap dev board, mediocre sensors, good for prototyping |

#### 4.2.4 Air Quality Products

| Product                     | Manufacturer | Connectivity       | Measurements                                                             | Notes                                                         |
| --------------------------- | ------------ | ------------------ | ------------------------------------------------------------------------ | ------------------------------------------------------------- |
| PurpleAir PA-II             | PurpleAir    | Wi-Fi              | PM1, PM2.5, PM10 (dual PMS5003), temp, humidity, pressure                | Dual laser counters for data validation, community network    |
| Airthings Wave Plus         | Airthings    | BLE + hub          | Radon (Bq/m³), CO₂, TVOC, temp, humidity, pressure                       | Indoor, radon is key differentiator                           |
| Airthings View Plus         | Airthings    | Wi-Fi/BLE          | As Wave Plus + PM2.5                                                     | Adds particulate                                              |
| uRADMonitor A3 / model A    | uRADMonitor  | Wi-Fi/Ethernet     | Radiation (CPM, µSv/h), PM2.5, CO₂, HCHO, VOC, temp, humidity, pressure  | Environmental monitoring, community network                   |
| Air Gradient ONE / Open Air | Air Gradient | Wi-Fi (ESP32)      | PM2.5 (PMS5003T), CO₂ (SenseAir S8), TVOC (SGP41), temp, humidity        | Open-source design, kits available, ESPHome compatible        |
| SenseCAP Indicator          | Seeed Studio | Wi-Fi/BLE          | CO₂ (SCD41), TVOC (SGP41), temp, humidity                                | Desk unit with screen, ESP32-S3 + RP2040                      |
| Aranet4                     | SAF Tehnika  | BLE + optional hub | CO₂ (NDIR), temp, humidity, pressure                                     | Very popular indoor CO₂ monitor, long battery life (2+ years) |
| Awair Element               | Awair        | Wi-Fi              | CO₂, TVOC, PM2.5, temp, humidity                                         | Consumer indoor AQ, good app, local API available             |

#### 4.2.5 Multi-Purpose IoT / LoRaWAN Sensors

| Product                | Manufacturer | Connectivity       | Measurements                                                                     | Notes                                                     |
| ---------------------- | ------------ | ------------------ | -------------------------------------------------------------------------------- | --------------------------------------------------------- |
| Dragino LHT65N         | Dragino      | LoRaWAN            | Temp + humidity (SHT31), external probe input                                    | Cheap LoRaWAN sensor, external DS18B20 option             |
| Dragino LDDS75         | Dragino      | LoRaWAN            | Distance (ultrasonic): 28–350 cm                                                 | Water level / bin level sensor                            |
| Dragino LSPH01         | Dragino      | LoRaWAN            | Soil pH                                                                          | LoRaWAN soil pH sensor                                    |
| Dragino LWL02          | Dragino      | LoRaWAN            | Water leak detection (binary + depth)                                            | Flood detection                                           |
| SenseCAP T1000         | Seeed Studio | LoRaWAN + GNSS     | GPS, temp, light, 3-axis accel                                                   | Tracker/asset tag with environmental sensing              |
| RAK Wisblock ecosystem | RAKwireless  | LoRaWAN            | Modular: base board + sensor boards (accel, env, GPS, etc.)                      | Build-your-own LoRaWAN sensor, very flexible              |
| TTGO/LilyGo T-Beam     | LilyGo       | LoRa + Wi-Fi + BLE | GPS (NEO-6M or M8N), LoRa (SX1276/62), Wi-Fi, BLE                                | Dev board, popular for Meshtastic, needs external sensors |
| Heltec CubeCell series | Heltec       | LoRaWAN            | Various boards: GPS, solar, OLED options                                         | Ultra-low power LoRa dev boards (ASR6501/ASR6502)         |
| Decentlab sensors      | Decentlab    | LoRaWAN            | Various: soil moisture, distance, pressure, CO₂, air temp/humidity, dendrometer  | Swiss-made, research grade, expensive but reliable, IP67  |
| Milesight AM100 series | Milesight    | LoRaWAN            | Temp, humidity, CO₂, TVOC, light, motion, pressure, sound                        | Indoor ambiance monitoring, occupancy sensing             |
| Elsys ERS series       | Elsys        | LoRaWAN            | Temp, humidity, light, motion, CO₂, sound, acceleration                          | Indoor LoRaWAN multi-sensor, Scandinavian quality         |

#### 4.2.6 Beehive / Livestock / Speciality Agriculture

| Product                | Manufacturer        | Connectivity   | Measurements                                        | Notes                           |
| ---------------------- | ------------------- | -------------- | --------------------------------------------------- | ------------------------------- |
| Arnia hive monitor     | Arnia               | Cellular/Wi-Fi | Weight, temp, humidity, acoustics (bee activity)    | Commercial beehive monitoring   |
| HiveMind (DIY)         | Various open-source | LoRa/Wi-Fi     | Weight (HX711 + 4×50kg load cells), temp, humidity  | Common DIY beehive scale design |
| Digitanimal GPS collar | Digitanimal         | Cellular       | GPS, activity (accelerometer)                       | Livestock tracking              |
| MOOvement ear tag      | MOOvement           | Cellular/LPWAN | GPS, activity, temperature                          | Cattle monitoring               |

#### 4.2.7 Indoor / Building Sensors

| Product                            | Manufacturer | Connectivity | Measurements                                                       | Notes                                                              |
| ---------------------------------- | ------------ | ------------ | ------------------------------------------------------------------ | ------------------------------------------------------------------ |
| Aqara sensors (door, motion, temp) | Aqara        | Zigbee       | Various: door open/close, motion, temp/humidity, light, vibration  | Consumer smart home, very cheap, Zigbee ecosystem                  |
| Xiaomi/Mi sensors                  | Xiaomi       | BLE/Zigbee   | Temp/humidity, door, motion, light, plant soil moisture            | BLE advertising, can be captured by ESPHome                        |
| Shelly H&T                         | Shelly       | Wi-Fi/MQTT   | Temp: -40 to +60°C; Humidity: 0–100%                               | Wi-Fi, battery powered, MQTT/CoAP native                           |
| Shelly Flood                       | Shelly       | Wi-Fi/MQTT   | Water detection (binary), temperature                              | Floor-level water detection                                        |
| Ruuvi RuuviTag                     | Ruuvi        | BLE          | Temp: -40 to +85°C; Humidity; Pressure; Acceleration (LIS2DH12)    | BLE beacon with environmental sensing, open firmware, Nordic nRF52 |
| Blue Maestro Tempo Disc            | Blue Maestro | BLE          | Temp, humidity, dew point (±0.1°C)                                 | BLE logger, long battery, data logging on device                   |
| SwitchBot Meter                    | SwitchBot    | BLE + hub    | Temp, humidity                                                     | Consumer, app-based, hub adds cloud connectivity                   |

### 4.3 Integration Notes

- Prices fluctuate; approximate costs noted where they indicate tier
  (budget/mid/research-grade).
- Many sensors have Adafruit, SparkFun, DFRobot, or Seeed Studio breakout
  boards that make prototyping easier — search "[sensor] breakout" for options.
- SDI-12 sensors (Meter, Campbell) need an SDI-12 to UART/I2C adapter for
  microcontroller integration.
- Atlas Scientific EZO modules can be daisy-chained on I2C — good for
  multi-parameter water monitoring rigs.
- For LoRaWAN products, CayenneLPP and SenML are common payload formats —
  iotdata replaces these with a more efficient encoding.

---

## 5. Prioritisation (Cross-Cutting)

Priority is determined by leverage: which encoding work (Section 2) unlocks the
most domain coverage (Section 3) for the most common, affordable, scalable
sensor deployments (Section 4)?

Criteria for high priority:

- The measurement is common across many deployments (environmental, agricultural,
  building)
- The sensors are affordable, widely available, and integrate with scalable
  electronics (I2C/SPI/UART, ESP32/nRF52/STM32)
- The encoding type serves multiple domains (e.g., CONDUCTIVITY serves both soil
  and water)
- The type unblocks a bundle or a frequently requested deployment pattern

### Tier 1 — Before v1.0

These are essential for a credible v1.0. Without them, common deployments cannot
be adequately encoded.

**Layer 1 generics:**

| Type      | Rationale                                                              |
| --------- | ---------------------------------------------------------------------- |
| NULL      | Enables zero-overhead binary flags (motion, door, tamper)              |
| BIT1      | Boolean + dynamic selector for rate/absolute (§2.6)                    |
| UINT8     | Most common generic escape hatch                                       |
| UINT16    | Raw ADC, large measurements, foundation for many Layer 3 types         |
| INT8      | Rate of change, signed small measurements                              |
| INT16     | Signed large measurements                                              |
| COUNTER16 | Wrapping counters (rain tips, flow pulses, energy totaliser)           |
| RAW8      | Opaque byte for proprietary/custom payloads                            |
| RAW16     | Opaque 2-byte for proprietary/custom payloads                          |

**Layer 2 units:**

| Type                 | Rationale                                                             |
| -------------------- | --------------------------------------------------------------------- |
| PERCENTAGE           | Unlocks soil moisture, leaf wetness, DO saturation, fill level, etc.  |
| CONDUCTIVITY         | Unlocks both soil EC and water EC — high-leverage single type         |
| TEMPERATURE_EXPANDED | Covers precision sensors (TMP117), high-altitude, industrial          |
| PRESSURE_EXPANDED    | Fixes J.1 — essential for any deployment above ~1500m                 |
| SPEED_MS_EXPANDED    | Fixes J.2 — essential for severe weather deployments                  |
| DISTANCE_CM_EXPANDED | Covers LiDAR (>1023 cm), deep water level, extended range ultrasonic  |

**Layer 3 specifics:**

| Type                      | Rationale                                                          |
| ------------------------- | ------------------------------------------------------------------ |
| Standalone Irradiance     | Fixes J.15 — many deployments want irradiance without UV           |
| Standalone UV Index       | Fixes J.15 — many deployments want UV without irradiance           |
| Soil Moisture (VWC)       | Core agriculture measurement; Ecowitt WH51, Teros 12, capacitive   |
| Soil EC                   | Core agriculture; Teros 12, Dragino LSE01                          |
| Soil Bundle               | Teros 12 pattern (VWC+EC+Temp); Dragino LSE01 pattern              |
| Water pH                  | Core water quality; Atlas EZO-pH, DFRobot SEN0161                  |
| Water EC                  | Core water quality; alias for CONDUCTIVITY with label              |
| Water Dissolved Oxygen    | Core water quality; Atlas EZO-DO, DFRobot SEN0237                  |
| Water Bundle              | Typical water bundle (pH+EC+DO)                                    |
| Temperature Rate of Change| Fire/frost detection (J.17.9); computed in firmware, no extra HW   |
| Pressure Rate of Change   | Storm prediction; computed in firmware, no extra HW                |

### Tier 2 — v1.x

Important, but either needs further encoding analysis (log scale), serves fewer
deployments, or has less common sensor availability.

**Layer 1/2:**

| Type             | Rationale                                                           |
| ---------------- | ------------------------------------------------------------------- |
| UINT4            | Small enum, category                                                |
| UINT10           | Medium unsigned, matches DEPTH encoding                             |
| RAW24            | Opaque 3-byte                                                       |
| RAW32            | Opaque 4-byte                                                       |
| RATIO8           | NDVI, duty cycle, confidence — useful but niche                     |
| SOUND_DBA        | Environmental noise monitoring                                      |
| LUX / LUX_LOG    | Light sensing; LUX_LOG depends on log-scale formula decision (§2.13)|
| TILT             | Landslide/structural monitoring                                     |
| ACCEL_G          | Vibration, motion, structural health                                |
| PRESSURE_KPA     | Non-atmospheric pressure (pipe, hydraulic, tyre)                    |
| WEIGHT_G         | Beehive scales, silo monitoring, livestock                          |

**Layer 3:**

| Type                     | Rationale                                                         |
| ------------------------ | ----------------------------------------------------------------- |
| Water Turbidity          | Important for water quality; needs log-scale decision (§2.13)     |
| Water ORP                | Redox potential for water treatment, aquaculture                  |
| Water Quality Bundle     | Multi-parameter sonde pattern (Atlas, YSI)                        |
| Soil Water Potential     | Critical for irrigation; Teros 21, Watermark 200SS                |
| Leaf Wetness             | Agricultural disease prediction                                   |
| PAR                      | Photosynthetic radiation for agriculture/ecology                  |
| Lightning Bundle         | Distance + count; AS3935                                          |
| Sound Level              | Environmental noise compliance, wildlife monitoring               |
| Colour RGB               | TCS34725; niche but straightforward (24 bits)                     |
| Non-Atmospheric Pressure | PRESSURE_KPA alias for pipe/hydraulic/tyre applications           |

### Tier 3 — Future / Niche

Add when demand emerges. These are either niche measurements, served by
generic types with variant labels, or dependent on decisions not yet made.

- All remaining gas species (CH₄, NH₃, H₂S, SO₂, O₂, radon — pending gas expansion strategy §2.13)
- Flow rate / volume totaliser (FLOW_LPM, VOLUME_L)
- NDVI (covered by RATIO8 + variant label)
- Chlorophyll-a (PPM or UGM3 + variant label)
- Dendrometer (UINT16 + variant label)
- Vibration frequency (UINT16 + variant label)
- RPM / rotation
- Magnetic field (MAGNETIC_UT)
- Strain (INT16 + variant label)
- Sound frequency (UINT16 + variant label)
- Colour temperature (derivable from RGB)
- CMYK colour (dropped — not a sensor output)
- Infrared band power (UINT16 + variant label)
- RF power / EMF (INT8 + variant label)
- Salinity / TDS (derivable from EC)
- Current / voltage / power / energy fields (VOLTAGE_MV, CURRENT_MA, POWER_W, ENERGY_WH)
- Mains frequency (UINT8 + variant scaling)
- PM2.5 rate of change (INT8 + variant label)
- Generic speed/velocity beyond wind (SPEED_MS + variant label)
- Distance short-range mm (DISTANCE_MM_SHORT, DISTANCE_MM)
- All remaining _EXPANDED variants not in Tier 1
- WEIGHT_G_EXPANDED, FLOW_LPM_EXPANDED, RATIO16, ACCEL_G_EXPANDED,
  ANGLE_360_FINE, ANGLE_SIGNED, COUNTER8, CONDUCTIVITY_COARSE,
  PERCENTAGE_FINE, SOUND_DBA_EXPANDED, FORCE_N

### Leverage Analysis (Tier 1)

The following Tier 1 Layer 2 types each unlock multiple Layer 3 / domain uses:

| Layer 2 Type         | Domains Unlocked                                                  |
| -------------------- | ----------------------------------------------------------------- |
| PERCENTAGE           | Soil moisture, leaf wetness, DO saturation, fill level, generic % |
| CONDUCTIVITY         | Soil EC, water EC, TDS (derived), salinity (derived)              |
| TEMPERATURE_EXPANDED | Industrial, desert, high-altitude, precision deployments          |
| PRESSURE_EXPANDED    | All deployments above ~1500 m (including many agricultural sites) |
| COUNTER16            | Rain tips, flow pulses, lightning, energy, generic event counts   |
| INT8                 | Temperature rate, pressure rate, any signed small measurement     |

---

*Last updated: 2026-04-02*
