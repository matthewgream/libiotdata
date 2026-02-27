# iotdata — Sensor Field Types TODO

Working notes for expanding field type coverage. Goal: comprehensive catalogue
of measurement types with ranges, then determine which get dedicated protocol
fields, which use generic typed fields, and which are out of scope.

---

## Existing Protocol Fields (for reference)

Already defined in the spec — no work needed except where noted.

| Field | Bits | Range | Resolution | Notes |
|-------|------|-------|------------|-------|
| Battery | 6 | 0–100%, charging flag | ~3.2% | |
| Link | 6 | RSSI -120–-60 dBm, SNR -20–+10 dB | 4 dBm / 10 dB | |
| Temperature | 9 | -40 to +80°C | 0.25°C | Standalone + in Environment bundle |
| Pressure (baro) | 8 | 850–1105 hPa | 1 hPa | **J.1: range too narrow, needs fix** |
| Humidity | 7 | 0–100% | 1% | Standalone + in Environment bundle |
| Environment | 24 | Temp+Press+Humid bundle | as above | |
| Wind Speed | 7 | 0–63.5 m/s | 0.5 m/s | **J.2: max may be too low** |
| Wind Direction | 8 | 0–355° | ~1.41° | |
| Wind Gust | 7 | 0–63.5 m/s | 0.5 m/s | |
| Wind | 22 | Speed+Dir+Gust bundle | as above | |
| Rain Rate | 8 | 0–255 mm/hr | 1 mm/hr | |
| Rain Size | 4 | 0–6.0 mm | 0.25 mm | **J.7: semantics undefined** |
| Rain | 12 | Rate+Size bundle | as above | |
| Solar Irradiance | 10 | 0–1023 W/m² | 1 W/m² | **J.11: may need 11 bits; J.15: no standalone** |
| UV Index | 4 | 0–15 | 1 | **J.8: max 15 may be low; J.15: no standalone** |
| Solar | 14 | Irradiance+UV bundle | as above | |
| Clouds | 4 | 0–8 okta | 1 okta | |
| AQ Index | 9 | 0–500 AQI | 1 | |
| AQ PM | 4–36 | 0–1275 µg/m³ per channel | 5 µg/m³ | 4 channels: PM1, PM2.5, PM4, PM10 |
| AQ Gas | 8–84 | Variable per gas | Variable | VOC, NOx, CO₂, CO, HCHO, O₃ + 2 reserved |
| Air Quality | 21+ | Index+PM+Gas bundle | as above | |
| Radiation CPM | 14 | 0–16383 CPM | 1 CPM | |
| Radiation Dose | 14 | 0–163.83 µSv/h | 0.01 µSv/h | |
| Radiation | 28 | CPM+Dose bundle | as above | |
| Depth | 10 | 0–1023 cm | 1 cm | Generic: snow, water level, ice, soil depth |
| Position | 48 | Lat/lon ±90°/±180° | ~1.2 m | |
| Datetime | 24 | Year-relative, 5s ticks | 5 s | |
| Flags | 8 | 8-bit bitmask | — | Deployment-defined |
| Image | Variable | Pixel data + control | — | Experimental |

---

## NEW FIELD TYPES — By Category

### 1. Soil

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Soil moisture (VWC) | % | 0–100 | Capacitive: Meter Teros 12, EC-5; TDR: Campbell CS655; resistive cheapies |
| Soil moisture (raw permittivity) | ε | 1–80 | Same sensors, raw output mode |
| Soil electrical conductivity | µS/cm | 0–20,000 (ag), 0–100,000 (saline) | Meter Teros 12, Teros 21, Decagon 5TE |
| Soil water potential (matric) | kPa | -200 to 0 (or -1500 to 0 full range) | Meter Teros 21 (tensiometer), Watermark 200SS |
| Soil temperature | °C | -40 to +80 | Reuse standalone Temperature field via variant label |
| **Soil bundle** | | VWC + EC + temp (+ potential?) | Teros 12 outputs all three |

Notes:
- Soil moisture and humidity share the same 0–100% range — could reuse humidity encoding with variant label, but soil EC needs its own type
- Water potential is critical for irrigation scheduling — distinct from moisture content
- Many soil probes output all values simultaneously (I2C/SDI-12), bundle makes sense


### 2. Water Quality

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| pH | pH | 0–14.00 | Atlas Scientific EZO-pH, Hanna HI1001, DFRobot SEN0161 |
| Electrical conductivity | µS/cm | 0–100,000 (fresh to seawater) | Atlas EZO-EC, DFRobot SEN0244 |
| Dissolved oxygen | mg/L | 0–25 | Atlas EZO-DO, In-Situ RDO Pro, YSI ProODO |
| Dissolved oxygen (saturation) | % | 0–200 (supersaturation possible) | Same sensors, % output mode |
| Turbidity | NTU | 0–10,000 (heavily right-skewed) | DFRobot SEN0189, Seapoint, Campbell OBS |
| ORP / redox potential | mV | -1000 to +1000 | Atlas EZO-ORP, Hanna |
| Total dissolved solids | ppm | 0–50,000 | Often derived from EC, some sensors direct |
| Salinity | ppt | 0–40 (seawater ~35) | Derived from EC+temp, some probes report directly |
| Chlorophyll-a / fluorescence | µg/L | 0–500 | Turner Cyclops, Sea-Bird ECO, YSI EXO |
| Water temperature | °C | -5 to +50 | Reuse standalone Temperature via variant label |
| Water level / stage | cm | 0–1023 | Reuse Depth field via variant label |
| **Water quality bundle** | | pH + EC + DO + temp | Atlas combo pattern, multi-parameter sondes |

Notes:
- Turbidity has massive dynamic range — strong candidate for log-scale encoding
- EC overlaps with soil EC — same physical measurement, consider one field type with variant-defined range
- Chlorophyll is niche but important for aquaculture/eutrophication


### 3. Water Flow & Volume

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Flow rate (small) | L/min | 0–100 | YF-S201 (Hall effect), Badger Meter |
| Flow rate (large) | m³/s | 0–500 | Ultrasonic transit-time, weir gauges |
| Flow volume (totaliser) | L or m³ | 0–65535 (wrapping) | Pulse counters on tipping/flow sensors |

Notes:
- Huge dynamic range problem — a garden hose vs a river
- Maybe variant-defined units with a generic scaled integer?
- Totaliser is a wrapping counter, needs dedicated counter field type
- Many flow measurements are derived from water level via stage-discharge curve — sensor transmits depth, gateway computes flow


### 4. Gas / Air (beyond existing AQ fields)

Existing AQ Gas field has VOC, NOx, CO₂, CO, HCHO, O₃ + 2 reserved slots.
These are additional gas species not covered:

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Methane (CH₄) | ppm | 0–10,000 (LEL ~50,000) | MQ-4, Figaro TGS2611, Sensirion SGP41 |
| Ammonia (NH₃) | ppm | 0–100 (livestock), 0–500 (industrial) | MQ-137, Sensirion SFA30, EC sense NH3/M-100 |
| Hydrogen sulfide (H₂S) | ppm | 0–100 | Alphasense H2S-A4, Membrapor H2S/S-20 |
| Sulfur dioxide (SO₂) | ppb | 0–2000 | Alphasense SO2-A4, Membrapor SO2/S-20 |
| Oxygen concentration | % | 0–25 (ambient ~20.9) | Alphasense O2-A2, SST LOX-02 |
| Radon (Rn) | Bq/m³ | 0–10,000 | Airthings Wave, RadonEye RD200 |
| Gas flow rate | L/min | 0–100 | Mass flow sensors (medical/industrial) |

Notes:
- Could extend AQ Gas mask from 8 to 16 bits (using reserved slots + expansion)
- Or define as standalone fields for variants that only need one gas
- CH₄ and NH₃ are high priority for agricultural deployments
- O₂% is critical for confined space safety (distinct from dissolved oxygen)


### 5. Vegetation & Agriculture

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Leaf wetness | % (or 4-level categorical) | 0–100 or dry/dew/wet/saturated | Decagon LWS, Davis leaf wetness sensor |
| Surface moisture | % | 0–100 | Same encoding as leaf wetness |
| PAR (photosynthetically active radiation) | µmol/m²/s | 0–2500 | Apogee SQ-520, LI-COR LI-190R |
| NDVI / vegetation index | ratio | 0.0–1.0 | Meter SRS-NDVI, multispectral cameras |
| Dendrometer / stem diameter change | µm | 0–10,000 | Ecomatik point dendrometer, Radius-type |

Notes:
- PAR is distinct from solar irradiance — measures photosynthetic wavelengths only
- Leaf wetness could be 2-bit categorical (4 levels) or 7-bit percentage
- NDVI is a ratio 0–1, could use a generic ratio field


### 6. Rate of Change / Derivatives

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Temperature rate | °C/min | -25 to +25 | Computed from consecutive readings |
| Pressure rate | hPa/hr | -10 to +10 | Computed from consecutive readings |
| PM2.5 rate | µg/m³/min | -127 to +128 | Computed from consecutive AQ readings |
| Generic rate | varies | varies | Any field's first derivative |

Notes:
- Temperature rate is highest priority (fire/frost detection, J.17.9)
- Pressure rate is useful for storm prediction
- Open question: dedicated field types vs generic modifier on any field type?
- Computed sensor-side from consecutive readings, no extra hardware needed


### 7. Motion / Vibration / Mechanical

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Acceleration (per-axis or magnitude) | g or m/s² | ±16g typical | ADXL345, MPU6050, LIS3DH, BMI270 |
| Vibration (RMS or peak) | g | 0–10 | Same accel sensors with FFT/RMS processing |
| Vibration frequency (dominant) | Hz | 0–1000 | Computed from accelerometer data |
| Tilt / inclination | degrees | 0–360 or ±90 | ADXL345, SCA100T, inclinometers |
| Rotation / RPM | RPM | 0–10,000 | Hall effect, optical encoders |
| Speed / velocity | m/s | 0–100 (generic) | Derived (GPS, wheel sensor, ultrasonic) |

Notes:
- Acceleration: many sensors output 3-axis, but for telemetry often just magnitude or per-axis max
- Vibration RMS/peak is a processed derivative of raw acceleration
- Tilt is critical for landslide/structural monitoring
- RPM relevant for wind turbines, machinery monitoring


### 8. Electromagnetic

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Magnetic field strength | µT | ±100 (or ±1000 for industrial) | HMC5883L, QMC5883L, LIS3MDL, MMC5603 |
| Lightning distance | km | 0–40 | AS3935 (Franklin Lightning Sensor) |
| Lightning strike count | count | 0–255 per period | AS3935 accumulated |
| RF power / EMF | dBm | -120 to 0 | Spectrum analysers, EMF meters |

Notes:
- AS3935 is the go-to lightning sensor IC, widely available on breakout boards
- Magnetic field useful for vehicle detection (parking), compass heading, geomagnetic monitoring
- Lightning could be a bundle: distance + count + estimated energy


### 9. Distance / Physical

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Distance (short range) | mm | 0–10,000 | VL53L0X/L1X ToF, ultrasonic HC-SR04 |
| Distance (long range) | cm | 0–65535 | TFmini LiDAR, Maxbotix ultrasonic |
| Weight / mass | g | 0–100,000 (100 kg) | Load cells + HX711, beehive scales |
| Force / load | N | 0–10,000 | Load cells, strain gauge bridges |
| Pressure (non-atmospheric) | kPa | 0–10,000 | Honeywell MPRLS, pipe/hydraulic/tyre sensors |
| Strain | µε (microstrain) | ±10,000 | Foil strain gauges, fibre Bragg gratings |

Notes:
- Distance: existing Depth field (10 bits, 0–1023 cm) covers many cases, but short-range mm precision and long-range need separate types
- Weight is key for beehive monitoring, silo levels, livestock scales
- Non-atmospheric pressure is physically different from barometric — pipe, tyre, hydraulic applications
- Strain is structural health monitoring — bridges, buildings, dams


### 10. Acoustic

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Sound level (A-weighted) | dBA | 0–140 | MEMS: ICS-43434, SPH0645; analog electret mics |
| Sound frequency / spectral peak | Hz | 20–20,000 | Computed from mic FFT |

Notes:
- Sound level monitoring: environmental noise compliance, wildlife acoustic triggers
- Frequency is niche — mostly for species ID triggers or machinery fault detection
- Could be a bundle: level + dominant frequency


### 11. Optical / Light (beyond existing Solar)

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Illuminance (lux) | lux | 0–120,000 | BH1750, TSL2591, VEML7700, MAX44009 |
| Colour temperature | K | 2000–10,000 | TCS34725 (RGB sensor, computed) |
| Colour (RGB) | 0–255 per channel | 3×8 bits | TCS34725, APDS-9960 |
| Colour (CMYK) | 0–100% per channel | 4×7 bits | Computed from RGB |
| Infrared (specific band) | W/m² or count | 0–1023 | MLX90614 (thermopile), pyroelectric |
| Standalone Irradiance | W/m² | 0–1500 | **J.15: split from Solar bundle** |
| Standalone UV Index | index | 0–15 | **J.15: split from Solar bundle** |

Notes:
- Lux has enormous range — 0.001 (starlight) to 120,000 (bright sun). Log scale candidate, or limit to useful outdoor range
- RGB colour: 24 bits total for 3 channels — straightforward
- CMYK: 28 bits for 4 channels — less common in sensors, more for display/printing context
- Infrared is relevant for fire detection, presence sensing


### 12. Power / Electrical

| Measurement | Units | Typical Range | Sensors |
|-------------|-------|---------------|---------|
| Current | mA | 0–65535 (or ±32768 signed) | INA219, INA260, ACS712 (Hall effect) |
| Voltage | mV | 0–65535 | ADC, INA219, voltage divider |
| Power | W | 0–65535 | INA219/260 computed, CT clamps |
| Energy (totaliser) | Wh | 0–65535 (wrapping) | Accumulated from power × time |
| Mains frequency | Hz | 45–65 (×10 for 0.1 Hz resolution) | Zero-crossing detector |

Notes:
- Voltage already in Health TLV as supply_mv — but as a data field it's useful for solar panel monitoring, load monitoring
- Current + voltage → power → energy is a natural chain
- Energy is a wrapping totaliser like flow volume
- Already have battery level (percentage) — these are the raw electrical values


### 13. Generic / Flexible Field Types

These are the "escape hatch" for measurements not worth a dedicated protocol field.

| Type | Bits | Range | Use Case |
|------|------|-------|----------|
| BIT1 (boolean/null flag) | 1 | 0/1 | Door open/closed, motion detected, presence, alarm state |
| UINT4 | 4 | 0–15 | Small enum, category, state machine value |
| UINT8 | 8 | 0–255 | Generic byte, small measurement with variant-defined scaling |
| UINT10 | 10 | 0–1023 | Medium measurement (matches existing depth encoding) |
| UINT16 | 16 | 0–65535 | Large measurement, counter, raw ADC value |
| INT8 | 8 | -128 to +127 | Signed small measurement, rate of change |
| INT16 | 16 | -32768 to +32767 | Signed large measurement |
| RAW8 | 8 | opaque | Raw byte, variant label gives meaning |
| RAW16 | 16 | opaque | Raw 2 bytes |
| RAW24 | 24 | opaque | Raw 3 bytes |
| RAW32 | 32 | opaque | Raw 4 bytes |
| COUNTER16 | 16 | 0–65535, wrapping | Event counter: rain tips, pulses, strikes, flow ticks |
| PERCENTAGE | 7 | 0–100% | Generic percentage (reuse humidity encoding) |
| RATIO8 | 8 | 0.0–1.0 (1/255 steps) | Normalised ratio: NDVI, duty cycle, confidence |

Notes:
- Variant label provides the semantic meaning ("door_state", "adc_raw", "pulse_count")
- Variant map could optionally carry scale/offset metadata for generic types
- COUNTER16 vs UINT16: semantically distinct — counters wrap, measurements don't
- BIT1 is the cheapest possible field — useful for binary sensors (PIR, reed switch, tamper)
- RAW types are opaque — decoder passes through, no quantisation, no JSON structure


---

## Open Design Questions

1. **Log-scale fields?** Turbidity, lux, gas concentrations, flow rate all have heavily skewed distributions. Define a LOG_UINT8 / LOG_UINT10 generic type? Or handle per-field?

2. **EC unification:** Soil EC and water EC are the same physical measurement with different typical ranges. One field type with variant-defined range, or two separate types?

3. **Rate-of-change as modifier vs field type:** Generic "delta" mechanism applicable to any field, or dedicated TEMPERATURE_RATE, PRESSURE_RATE types?

4. **Bundle granularity:** How many new bundles? Soil bundle and water quality bundle seem obvious. Lightning bundle (distance + count)? Power bundle (V + I + W)?

5. **Generic field metadata:** If we add UINT8/UINT16 generic types, does the variant map need to carry scale/offset/unit metadata? Or is that purely out-of-band?

6. **Colour encoding:** RGB (24 bits) straightforward. CMYK (28 bits) — is this actually needed in sensor context, or is it purely a display/output concern? Probably just RGB.

7. **Gas field expansion:** Extend existing AQ Gas mask from 8 to 16 bits to accommodate CH₄, NH₃, H₂S, SO₂, O₂, radon? Or separate standalone gas types?

8. **Maximum field type count:** Currently ~25 types. This TODO adds ~40-50 more candidates. The 7-bit global field type ID (J.27) supports 128 types. Comfortable, but should we be more selective?


---

## Priority Tiers

### Tier 1 — Add before v1.0 (common, well-understood, clear encoding)
- Standalone Irradiance (split from Solar, J.15)
- Standalone UV Index (split from Solar, J.15)
- Soil moisture
- Soil EC
- Soil bundle
- Water pH
- Water EC
- Water dissolved oxygen
- Temperature rate of change
- BIT1 (boolean flag)
- UINT8, UINT16, INT8, INT16 generic types
- COUNTER16 (wrapping counter)
- PERCENTAGE

### Tier 2 — Add in v1.x (important, needs range/encoding analysis)
- Water turbidity (log scale question)
- Water ORP
- Water quality bundle
- Soil water potential
- Leaf wetness
- PAR
- Lightning (distance + count)
- Sound level (dBA)
- Illuminance / lux (log scale question)
- Weight / mass
- Non-atmospheric pressure
- Acceleration
- Colour RGB
- RAW8/16/24/32 generic types
- RATIO8

### Tier 3 — Future / niche (add when needed)
- Water flow rate / volume totaliser
- Gas flow rate
- Additional gas species (CH₄, NH₃, H₂S, SO₂, O₂, radon)
- Chlorophyll-a
- NDVI
- Dendrometer
- Vibration frequency
- Tilt / inclination
- RPM / rotation
- Magnetic field
- Strain
- Sound frequency
- Colour temperature
- CMYK colour
- Infrared band power
- RF power / EMF
- Salinity / TDS (derivable from EC)
- Current / voltage / power / energy fields
- Mains frequency
- Pressure rate of change
- PM2.5 rate of change
- Generic speed/velocity
- Distance (mm precision, short range)
- Distance (long range, beyond existing Depth)
- UINT4, UINT10, RAW24, RAW32

---

_Last updated: 2026-02-27_
