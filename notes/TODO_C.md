# iotdata — Off-the-Shelf Sensor Reference

Research reference for sensor chips, modules, breakout boards, and sensor
products commonly used in environmental monitoring, agriculture, smart
buildings, weather stations, and general IoT deployments.

Not covered: factory/heavy-industrial sensors, extreme environment sensors.

---

## Sensor Chips & Modules

These are ICs or small modules typically soldered or connected to a
microcontroller (ESP32, STM32, PIC, etc.) via I2C, SPI, UART, SDI-12, or analog
output.

### Temperature

| Sensor               | Manufacturer                  | Interface | Outputs & Ranges                                         | Notes                                                                  |
| -------------------- | ----------------------------- | --------- | -------------------------------------------------------- | ---------------------------------------------------------------------- |
| DS18B20              | Maxim/Analog Devices          | 1-Wire    | Temp: -55 to +125°C, ±0.5°C (0.0625°C res)               | Ubiquitous, waterproof probe versions, parasitic power, multi-drop bus |
| TMP117               | Texas Instruments             | I2C       | Temp: -55 to +150°C, ±0.1°C (0.0078°C res)               | High accuracy, NIST-traceable, low power                               |
| TMP36                | Analog Devices                | Analog    | Temp: -40 to +125°C, ±2°C                                | Simple analog output, cheap, no calibration needed                     |
| MAX31865             | Maxim/Analog Devices          | SPI       | PT100/PT1000 RTD interface, -200 to +850°C               | RTD-to-digital converter, very high precision                          |
| MAX31855             | Maxim/Analog Devices          | SPI       | K-type thermocouple, -200 to +1350°C, ±2°C               | Cold junction compensated, also J/N/T variants                         |
| MCP9808              | Microchip                     | I2C       | Temp: -40 to +125°C, ±0.25°C (0.0625°C res)              | Programmable alert thresholds                                          |
| NTC thermistor (10k) | Various (Murata, TDK, Vishay) | Analog    | Typically -40 to +125°C, accuracy depends on calibration | Cheapest option, needs ADC + Steinhart-Hart                            |
| Si7051               | Silicon Labs                  | I2C       | Temp: -40 to +125°C, ±0.1°C (0.01°C res)                 | Very high precision, tiny package                                      |

### Humidity (typically includes temperature)

| Sensor                | Manufacturer      | Interface   | Outputs & Ranges                                                      | Notes                                              |
| --------------------- | ----------------- | ----------- | --------------------------------------------------------------------- | -------------------------------------------------- |
| SHT40 / SHT41 / SHT45 | Sensirion         | I2C         | RH: 0–100%, ±1.8% (SHT40) to ±1% (SHT45); Temp: -40 to +125°C, ±0.2°C | Current generation, replaces SHT3x. Very low power |
| SHT31                 | Sensirion         | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.3°C                          | Previous gen, still widely used                    |
| HDC1080               | Texas Instruments | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.2°C                          | Low power, cheap, integrated heater                |
| DHT22 / AM2302        | Aosong            | 1-Wire-like | RH: 0–100%, ±2–5%; Temp: -40 to +80°C, ±0.5°C                         | Very cheap, slow (2s sample), unreliable long-term |
| DHT11                 | Aosong            | 1-Wire-like | RH: 20–80%, ±5%; Temp: 0–50°C, ±2°C                                   | Even cheaper, limited range, integer resolution    |
| AHT20                 | Aosong            | I2C         | RH: 0–100%, ±2%; Temp: -40 to +85°C, ±0.3°C                           | Cheap I2C upgrade from DHT series                  |
| SHTC3                 | Sensirion         | I2C         | RH: 0–100%, ±2%; Temp: -40 to +125°C, ±0.2°C                          | Ultra-low power, wearable/battery focus            |
| HIH6130               | Honeywell         | I2C         | RH: 10–90%, ±4%; Temp: -25 to +85°C                                   | Industrial grade, condensation resistant           |
| BME280                | Bosch             | I2C/SPI     | See multi-sensor section below                                        |                                                    |

### Barometric Pressure (typically includes temperature)

| Sensor    | Manufacturer       | Interface | Outputs & Ranges                                                      | Notes                                       |
| --------- | ------------------ | --------- | --------------------------------------------------------------------- | ------------------------------------------- |
| BME280    | Bosch              | I2C/SPI   | See multi-sensor section below                                        |                                             |
| BMP280    | Bosch              | I2C/SPI   | Press: 300–1100 hPa, ±1 hPa (0.16 Pa res); Temp: -40 to +85°C         | Like BME280 minus humidity                  |
| BMP390    | Bosch              | I2C/SPI   | Press: 300–1250 hPa, ±0.5 hPa (±0.03 hPa rel); Temp: -40 to +85°C     | Higher accuracy than BMP280, lower noise    |
| MS5611    | TE Connectivity    | I2C/SPI   | Press: 10–1200 mbar, ±1.5 mbar (0.012 mbar res); Temp: -40 to +85°C   | Popular in altimeters, very high resolution |
| LPS22HB   | STMicroelectronics | I2C/SPI   | Press: 260–1260 hPa, ±0.1 hPa (abs); Temp included                    | Low power, high accuracy                    |
| DPS310    | Infineon           | I2C/SPI   | Press: 300–1200 hPa, ±1 hPa (0.002 hPa precision); Temp: -40 to +85°C | Very high precision, good for indoor nav    |
| SPL06-007 | Goertek            | I2C/SPI   | Press: 300–1100 hPa, ±1 hPa; Temp: -40 to +85°C                       | Budget alternative to BMP280                |
| HP206C    | HopeRF             | I2C       | Press: 300–1200 hPa; Temp; Altitude computed                          | Integrated altimeter function               |

### Multi-Sensor Environment (temp + humidity + pressure)

| Sensor         | Manufacturer       | Interface | Outputs & Ranges                                                       | Notes                                                             |
| -------------- | ------------------ | --------- | ---------------------------------------------------------------------- | ----------------------------------------------------------------- |
| BME280         | Bosch              | I2C/SPI   | Temp: -40 to +85°C, ±1°C; RH: 0–100%, ±3%; Press: 300–1100 hPa, ±1 hPa | The classic IoT weather sensor. Ubiquitous, cheap, well-supported |
| BME680         | Bosch              | I2C/SPI   | As BME280 + Gas resistance (VOC proxy), IAQ index                      | Adds MOX gas sensor for indoor air quality                        |
| BME688         | Bosch              | I2C/SPI   | As BME680 + AI gas scanning mode                                       | Can identify gas signatures with Bosch AI library                 |
| ENS160 + AHT21 | ScioSense + Aosong | I2C       | AQI, TVOC (0–65000 ppb), eCO₂ (400–65000 ppm) + RH + Temp              | Common combo board (e.g. SparkFun)                                |

### Air Quality — Particulate Matter

| Sensor              | Manufacturer | Interface | Outputs & Ranges                                           | Notes                                                         |
| ------------------- | ------------ | --------- | ---------------------------------------------------------- | ------------------------------------------------------------- |
| PMS5003 / PMS7003   | Plantower    | UART      | PM1.0, PM2.5, PM10: 0–500 µg/m³; particle counts per 0.1L  | Most common low-cost PM sensor. Fan-based, ~100mA active      |
| PMS5003T            | Plantower    | UART      | As PMS5003 + Temp + Humidity                               | Integrated T/H, saves external sensor                         |
| SPS30               | Sensirion    | I2C/UART  | PM1.0, PM2.5, PM4, PM10: 0–1000 µg/m³; mass + number conc. | Higher quality than Plantower, auto-clean, longer life (~8yr) |
| SEN5x (SEN54/SEN55) | Sensirion    | I2C       | PM1–10 + VOC index + NOx index + Temp + RH (SEN55)         | All-in-one environmental module                               |
| PMSA003I            | Plantower    | I2C       | As PMS5003 but I2C interface                               | Easier integration than UART versions                         |
| SDS011              | Nova Fitness | UART      | PM2.5, PM10: 0–999.9 µg/m³                                 | Older design, larger, cheaper, noisier                        |
| HM3301              | Seeed/Grove  | I2C       | PM1.0, PM2.5, PM10: 1–500 µg/m³                            | Grove ecosystem compatible                                    |

### Air Quality — Gas Sensors

| Sensor                                       | Manufacturer        | Interface   | Outputs & Ranges                                    | Notes                                                                                                                                          |
| -------------------------------------------- | ------------------- | ----------- | --------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| SCD41 / SCD40                                | Sensirion           | I2C         | CO₂: 400–5000 ppm, ±50 ppm; Temp; RH                | True NDIR CO₂ (photoacoustic), gold standard for indoor CO₂                                                                                    |
| SCD30                                        | Sensirion           | I2C/UART    | CO₂: 400–10,000 ppm, ±30 ppm; Temp; RH              | Older NDIR module, larger, higher power                                                                                                        |
| MH-Z19B / MH-Z19C                            | Winsen              | UART/PWM    | CO₂: 400–5000 ppm (up to 10,000), ±50 ppm           | Cheap NDIR CO₂, widely used in DIY projects                                                                                                    |
| SGP41                                        | Sensirion           | I2C         | VOC index: 0–500; NOx index: 0–500                  | MOX sensor, outputs processed index values, needs algorithm library                                                                            |
| SGP30                                        | Sensirion           | I2C         | TVOC: 0–60,000 ppb; eCO₂: 400–60,000 ppm            | Older than SGP41, direct ppb/ppm output                                                                                                        |
| CCS811                                       | ScioSense (was AMS) | I2C         | eCO₂: 400–8192 ppm; TVOC: 0–1187 ppb                | MOX, estimated CO₂ (not true NDIR), being discontinued                                                                                         |
| ENS160                                       | ScioSense           | I2C         | AQI (1–5); TVOC: 0–65,000 ppb; eCO₂: 400–65,000 ppm | CCS811 successor, multi-element MOX                                                                                                            |
| MQ series (MQ-2, MQ-4, MQ-7, MQ-135, MQ-137) | Winsen/Hanwei       | Analog      | Various gases, ranges depend on model               | Very cheap, analog output, high power (~150mA heater), poor selectivity. MQ-4: CH₄ 200–10,000 ppm; MQ-7: CO 20–2000 ppm; MQ-137: NH₃ 5–500 ppm |
| MICS-6814                                    | SGX Sensortech      | Analog      | CO: 1–1000 ppm; NO₂: 0.05–10 ppm; NH₃: 1–500 ppm    | Three-channel MOX, better than MQ series                                                                                                       |
| ZE07-CO                                      | Winsen              | UART/Analog | CO: 0–500 ppm, ±3 ppm                               | Electrochemical, much better selectivity than MOX                                                                                              |
| ZE25-O3                                      | Winsen              | UART/Analog | O₃: 0–10 ppm                                        | Electrochemical ozone sensor                                                                                                                   |
| ZMOD4410                                     | Renesas             | I2C         | TVOC, IAQ, eCO₂                                     | Low power MOX, firmware-based gas detection                                                                                                    |

### Soil Sensors

| Sensor                             | Manufacturer                      | Interface  | Outputs & Ranges                                           | Notes                                                               |
| ---------------------------------- | --------------------------------- | ---------- | ---------------------------------------------------------- | ------------------------------------------------------------------- |
| Teros 12                           | Meter Group                       | SDI-12/DDI | VWC: 0–100%; EC: 0–20,000 µS/cm (bulk); Temp: -40 to +60°C | Research grade, capacitive FDR. Expensive (~$150+)                  |
| Teros 21                           | Meter Group                       | SDI-12/DDI | Water potential: -100,000 to -9 kPa; Temp: -40 to +60°C    | Matric potential sensor (MPS-6 successor)                           |
| 5TE                                | Meter/Decagon                     | SDI-12     | VWC, EC, Temp                                              | Older Decagon design, still common in field                         |
| Capacitive soil moisture (generic) | Various (DFRobot, Adafruit, etc.) | Analog     | VWC: ~0–100% (uncalibrated)                                | Very cheap (£2–5), no calibration, corrosion-resistant vs resistive |
| Watermark 200SS                    | Irrometer                         | Resistance | Water potential: 0–239 kPa (centibars)                     | Granular matrix sensor, widely used in irrigation                   |
| SoilWatch 10                       | Pino-Tech                         | Analog/I2C | VWC: 0–60% calibrated                                      | Open-source-friendly, well-documented                               |
| Chirp!                             | Catnip Electronics                | I2C        | Capacitive moisture (raw), Temp, Light                     | Open-source, good for hobby use                                     |

### Water Quality Sensors

| Sensor                   | Manufacturer     | Interface | Outputs & Ranges                                 | Notes                                                             |
| ------------------------ | ---------------- | --------- | ------------------------------------------------ | ----------------------------------------------------------------- |
| EZO-pH                   | Atlas Scientific | I2C/UART  | pH: 0.001–14.000, ±0.002                         | Lab-grade, isolated, calibratable. Board (~$40) + probe (~$30–80) |
| EZO-EC                   | Atlas Scientific | I2C/UART  | EC: 0.07–500,000 µS/cm; TDS; Salinity; SG        | K1.0 or K10 probe depending on range                              |
| EZO-DO                   | Atlas Scientific | I2C/UART  | DO: 0–100 mg/L, ±0.05 mg/L                       | Galvanic probe, needs periodic membrane replacement               |
| EZO-ORP                  | Atlas Scientific | I2C/UART  | ORP: -1019.9 to +1019.9 mV                       | Platinum electrode                                                |
| EZO-RTD                  | Atlas Scientific | I2C/UART  | Temp: -200 to +850°C (PT100/1000)                | For temperature compensation of other EZO sensors                 |
| EZO-HUM                  | Atlas Scientific | I2C/UART  | RH + Temp + Dew point                            | Humidity module in same ecosystem                                 |
| SEN0161                  | DFRobot          | Analog    | pH: 0–14, ±0.1                                   | Budget pH, analog output, less precise than Atlas                 |
| SEN0244                  | DFRobot          | Analog    | EC: 1–20,000 µS/cm (K=1) or up to 200,000 (K=10) | Budget EC, analog output                                          |
| SEN0189                  | DFRobot          | Analog    | Turbidity: 0–3000 NTU (approx)                   | IR-based, analog, rough readings                                  |
| SEN0237                  | DFRobot          | Analog    | DO: 0–20 mg/L                                    | Budget dissolved oxygen                                           |
| Gravity TDS Meter        | DFRobot          | Analog    | TDS: 0–1000 ppm                                  | Very cheap, derived from conductivity                             |
| TSS/turbidity (Seapoint) | Seapoint Sensors | Analog    | Turbidity: 0–750 FTU (up to 1500)                | Research-grade, underwater, expensive                             |

### Wind

| Sensor                           | Manufacturer      | Interface    | Outputs & Ranges                                               | Notes                                               |
| -------------------------------- | ----------------- | ------------ | -------------------------------------------------------------- | --------------------------------------------------- |
| Davis 6410 anemometer            | Davis Instruments | Pulse/Analog | Speed: 0–89 m/s (pulse); Direction: 0–360° (pot)               | Cup anemometer + vane, standard amateur weather     |
| Inspeed Vortex                   | Inspeed           | Pulse/Analog | Speed: 0–80+ m/s; Direction: 0–360°                            | Alternative to Davis                                |
| Modern Device Rev P              | Modern Device     | Analog       | Airspeed: ~0–40 m/s                                            | Hot-wire anemometer, no moving parts, fragile       |
| Ultrasonic (Gill, FT, Lufft)     | Various           | UART/RS485   | Speed: 0–60+ m/s; Dir: 0–360°; Temp                            | No moving parts, 2D or 3D, expensive (£200+)        |
| Calypso ULP                      | Calypso           | BLE/UART     | Speed: 0–30 m/s; Dir: 0–360°                                   | Ultra-low power ultrasonic, battery operated        |
| SEN-15901 (SparkFun weather kit) | SparkFun/Argent   | Pulse/Analog | Speed: 0–55 m/s; Dir: 0–360° (8 positions); Rain: 0.2794mm/tip | Cheap weather station kit, cups + vane + rain gauge |

### Rain

| Sensor                   | Manufacturer         | Interface    | Outputs & Ranges                              | Notes                                                    |
| ------------------------ | -------------------- | ------------ | --------------------------------------------- | -------------------------------------------------------- |
| Hydreon RG-15            | Hydreon              | UART/Digital | Accumulation (mm), rain intensity, event flag | Optical, solid-state, no tipping bucket, low maintenance |
| Hydreon RG-9             | Hydreon              | Digital      | Rain yes/no, 7 intensity levels               | Simpler/cheaper than RG-15, digital output               |
| Tipping bucket (generic) | Davis, Argent, Misol | Pulse        | 0.2–0.5 mm per tip depending on model         | Standard design, reed switch, simple pulse counting      |
| Davis 6463M              | Davis Instruments    | Pulse        | 0.2 mm per tip                                | AeroCone collector, improved accuracy in wind            |

### Solar / Light / UV

| Sensor                 | Manufacturer       | Interface     | Outputs & Ranges                                                     | Notes                                                            |
| ---------------------- | ------------------ | ------------- | -------------------------------------------------------------------- | ---------------------------------------------------------------- |
| BH1750                 | Rohm               | I2C           | Lux: 1–65535 lux, 1 lux res                                          | Very common, cheap, spectral response close to human eye         |
| TSL2591                | AMS/OSRAM          | I2C           | Lux: 188 µlux–88,000 lux; separate visible + IR channels             | Very wide dynamic range (600M:1), high sensitivity               |
| VEML7700               | Vishay             | I2C           | Lux: 0–120,000 lux (with gain adjustment)                            | Good all-round ambient light, auto gain                          |
| MAX44009               | Maxim              | I2C           | Lux: 0.045–188,000 lux                                               | Ultra-wide range, ultra-low power (0.65 µA)                      |
| LTR-390UV              | Lite-On            | I2C           | UV Index + ALS (ambient light) in one                                | Dual UV + visible, good for outdoor                              |
| GUVA-S12SD             | GenUV              | Analog        | UV-A irradiance: 240–370 nm band, ~0–15 UV index (derived)           | Cheap UV-A photodiode, analog output                             |
| VEML6075               | Vishay             | I2C           | UVA + UVB irradiance, UV Index computed                              | Separate UVA/UVB channels, more accurate UV index                |
| SI1145                 | Silicon Labs       | I2C           | UV index (approx), Visible, IR                                       | Digital proximity/UV/ambient light, UV is estimated not measured |
| ML8511                 | Lapis              | Analog        | UV intensity: ~0–15 mW/cm²                                           | Analog UV sensor, simple                                         |
| Apogee SP-510 / SP-610 | Apogee Instruments | Analog/SDI-12 | Solar irradiance (pyranometer): 0–2000 W/m²                          | Thermopile-based, ISO 9060 compliant, research grade             |
| Davis 6450             | Davis Instruments  | Analog        | Solar irradiance: 0–1800 W/m², ±5%                                   | Silicon photodiode pyranometer, consumer grade                   |
| Apogee SQ-520          | Apogee Instruments | SDI-12/Analog | PAR: 0–4000 µmol/m²/s                                                | Quantum sensor (PAR), research grade                             |
| LI-COR LI-190R         | LI-COR             | Analog        | PAR: 0–3000+ µmol/m²/s                                               | Gold standard PAR sensor, expensive (~$500+)                     |
| TCS34725               | AMS/OSRAM          | I2C           | RGBC (Red, Green, Blue, Clear): 16-bit per channel; colour temp; lux | Colour sensor, IR blocking filter, good for colour matching      |
| APDS-9960              | Broadcom           | I2C           | RGBC + proximity + gesture                                           | Multi-function: colour, proximity (0–20 cm), gesture recognition |

### Distance / Ranging

| Sensor            | Manufacturer       | Interface       | Outputs & Ranges              | Notes                                                   |
| ----------------- | ------------------ | --------------- | ----------------------------- | ------------------------------------------------------- |
| VL53L0X           | STMicroelectronics | I2C             | Distance: 30–1200 mm, ±3%     | Time-of-Flight laser, small, cheap, 940nm               |
| VL53L1X           | STMicroelectronics | I2C             | Distance: 40–4000 mm, ±3%     | Longer range than L0X, ROI selection                    |
| VL53L4CD          | STMicroelectronics | I2C             | Distance: 1–1300 mm           | Close-range ToF, high accuracy                          |
| HC-SR04           | Various            | Trig/Echo       | Distance: 2–400 cm, ±3 mm     | Ultrasonic, ubiquitous, cheap, 40kHz                    |
| JSN-SR04T         | Various            | Trig/Echo/UART  | Distance: 20–600 cm           | Waterproof ultrasonic, good for water level             |
| MB7389 (MaxSonar) | MaxBotix           | Analog/PWM/UART | Distance: 30–500 cm, 1 cm res | Weather-resistant ultrasonic, IP67, various beam widths |
| TFmini Plus       | Benewake           | UART/I2C        | Distance: 10 cm–12 m, ±1–6 cm | LiDAR, works outdoors, 100 Hz update                    |
| TFmini-S          | Benewake           | UART            | Distance: 10 cm–12 m          | Smaller, cheaper variant                                |
| TF-Luna           | Benewake           | UART/I2C        | Distance: 20 cm–8 m, ±2 cm    | Cheapest LiDAR option (~$10)                            |
| US-100            | Various            | UART/Trig-Echo  | Distance: 2–450 cm            | Ultrasonic with temp compensation, UART or HC-SR04 mode |
| GP2Y0A21YK0F      | Sharp              | Analog          | Distance: 10–80 cm            | IR proximity, analog, cheap, noisy                      |

### Weight / Load

| Sensor               | Manufacturer                     | Interface            | Outputs & Ranges                                | Notes                                                                        |
| -------------------- | -------------------------------- | -------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------- |
| HX711                | Avia Semiconductor               | Digital (clock/data) | 24-bit ADC for load cells, 10 or 80 SPS         | The standard load cell ADC, cheap, everywhere                                |
| NAU7802              | Nuvoton                          | I2C                  | 24-bit ADC for load cells, up to 320 SPS        | Better than HX711 — I2C, lower noise, configurable                           |
| Load cell (bar type) | Various (SparkFun, CZL635, etc.) | Analog (via HX711)   | 1 kg, 5 kg, 10 kg, 20 kg, 50 kg, 100 kg, 200 kg | Wheatstone bridge, needs amplifier ADC. Beehive scales typically use 4×50 kg |

### Acceleration / Motion / Tilt

| Sensor    | Manufacturer       | Interface  | Outputs & Ranges                                     | Notes                                           |
| --------- | ------------------ | ---------- | ---------------------------------------------------- | ----------------------------------------------- |
| ADXL345   | Analog Devices     | I2C/SPI    | 3-axis accel: ±2/4/8/16g, 13-bit, up to 3200 Hz      | Classic, cheap, tap/freefall/activity detection |
| MPU6050   | InvenSense/TDK     | I2C        | 3-axis accel: ±2/4/8/16g; 3-axis gyro: ±250–2000°/s  | 6-DOF IMU, very common, DMP for orientation     |
| MPU9250   | InvenSense/TDK     | I2C/SPI    | As MPU6050 + 3-axis magnetometer (AK8963)            | 9-DOF IMU (discontinued but still available)    |
| LIS3DH    | STMicroelectronics | I2C/SPI    | 3-axis accel: ±2/4/8/16g, 10/12-bit, up to 5.3 kHz   | Low power (2 µA @ 1 Hz), FIFO buffer            |
| BMI270    | Bosch              | I2C/SPI    | 3-axis accel: ±2/4/8/16g; 3-axis gyro: ±125–2000°/s  | Ultra-low power IMU, wearable-grade             |
| ICM-20948 | InvenSense/TDK     | I2C/SPI    | 9-DOF: accel ±2–16g, gyro ±250–2000°/s, mag ±4900 µT | MPU9250 successor, recommended for new designs  |
| ADXL355   | Analog Devices     | I2C/SPI    | 3-axis accel: ±2/4/8g, 20-bit, very low noise        | High precision, structural health monitoring    |
| SCA100T   | Murata             | SPI/Analog | Tilt: ±90° (single or dual axis), 0.001° res         | Dedicated inclinometer, industrial grade        |
| SCL3300   | Murata             | SPI        | Tilt: ±90° 3-axis, 0.001° res                        | High precision tilt, MEMS                       |

### Magnetometer

| Sensor   | Manufacturer       | Interface | Outputs & Ranges                                | Notes                                                       |
| -------- | ------------------ | --------- | ----------------------------------------------- | ----------------------------------------------------------- |
| QMC5883L | QST Corporation    | I2C       | 3-axis: ±8 or ±2 gauss (±800 / ±200 µT), 16-bit | HMC5883L replacement, very common, cheap                    |
| HMC5883L | Honeywell          | I2C       | 3-axis: ±1.3–8.1 gauss, 12-bit                  | Classic, now hard to source, many counterfeits are QMC5883L |
| LIS3MDL  | STMicroelectronics | I2C/SPI   | 3-axis: ±4/8/12/16 gauss, 16-bit                | Good performance, pairs well with LIS3DH                    |
| MMC5603  | MEMSIC             | I2C       | 3-axis: ±30 gauss, 0.0625 mG res                | Set/reset for offset, high accuracy                         |
| BMM150   | Bosch              | I2C/SPI   | 3-axis: ±1300 µT (XY), ±2500 µT (Z)             | Often paired with BMI270 for 9-DOF                          |

### Lightning

| Sensor | Manufacturer        | Interface | Outputs & Ranges                                                                | Notes                                                                                            |
| ------ | ------------------- | --------- | ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| AS3935 | ScioSense (was AMS) | I2C/SPI   | Distance: 1–40 km (5 levels); Lightning/disturber/noise event; Estimated energy | Only real option for IC-level lightning detection. Breakout boards from SparkFun, DFRobot, CJMCU |

### Sound / Noise

| Sensor                     | Manufacturer   | Interface  | Outputs & Ranges                            | Notes                                             |
| -------------------------- | -------------- | ---------- | ------------------------------------------- | ------------------------------------------------- |
| ICS-43434                  | InvenSense/TDK | I2S        | Raw audio, -26 dBFS sensitivity, SNR 64 dBA | MEMS digital mic, good for SPL computation        |
| SPH0645LM4H                | Knowles        | I2S        | Raw audio, SNR 65 dBA                       | Popular I2S MEMS mic, Adafruit breakout           |
| INMP441                    | InvenSense/TDK | I2S        | Raw audio, SNR 61 dBA                       | Very cheap I2S MEMS mic, common on ESP32 projects |
| MAX4466                    | Maxim          | Analog     | Amplified analog mic output                 | Electret mic with adjustable gain preamp          |
| MAX9814                    | Maxim          | Analog     | Amplified analog mic output, auto gain      | AGC electret mic amp, better than MAX4466         |
| SEN-15892 (Qwiic Loudness) | SparkFun       | I2C/Analog | Raw ADC value proportional to SPL           | Simple board, not calibrated dBA                  |

### Radiation

| Sensor                            | Manufacturer               | Interface       | Outputs & Ranges                                        | Notes                                                                                         |
| --------------------------------- | -------------------------- | --------------- | ------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| Geiger tube (SBM-20, J305, M4011) | Various (Russian, Chinese) | Pulse           | CPM (counts per minute), ~1 µSv/h per ~150 CPM (SBM-20) | Needs HV supply (400V). SBM-20 is beta+gamma, J305 is gamma. Conversion factor varies by tube |
| RadiationD v1.1                   | RH Electronics             | Pulse (via kit) | CPM via SBM-20 or similar                               | Arduino/ESP32 shield with HV supply                                                           |
| BME688 + firmware                 | Bosch                      | I2C             | Some claim radon indication via gas scan                | Very experimental, not validated for radon                                                    |

### GPS / Position

| Sensor           | Manufacturer   | Interface    | Outputs & Ranges                                           | Notes                                                   |
| ---------------- | -------------- | ------------ | ---------------------------------------------------------- | ------------------------------------------------------- |
| NEO-6M           | u-blox         | UART         | Lat/Lon/Alt, ±2.5 m CEP, 5 Hz                              | Classic cheap GPS, still widely used                    |
| NEO-M8N          | u-blox         | UART/I2C     | Lat/Lon/Alt, ±2.5 m CEP, 10 Hz; GLONASS+GPS                | Multi-constellation, more reliable fix                  |
| NEO-M9N          | u-blox         | UART/I2C/SPI | Lat/Lon/Alt, ±1.5 m CEP, 25 Hz; GPS+GLONASS+Galileo+BeiDou | Current gen, quad constellation                         |
| SAM-M10Q         | u-blox         | UART/I2C     | As M9N, smaller module                                     | Compact, low power                                      |
| L76K / L80 / L86 | Quectel        | UART         | Lat/Lon/Alt, ±2.5 m; GPS+GLONASS or GPS+BeiDou             | Budget alternative to u-blox                            |
| PA1010D          | CDTop/Adafruit | I2C/UART     | GPS+GLONASS, ±3 m                                          | Antenna integrated, I2C, Adafruit breakout              |
| ZED-F9P          | u-blox         | UART/I2C/SPI | RTK: ±1 cm with corrections; multi-band L1/L2              | High precision RTK, ~$200+, needs base station or NTRIP |
| BN-880           | Beitian        | UART/I2C     | GPS+GLONASS, ±2.5 m + compass (QMC5883L)                   | GPS + magnetometer combo module, popular in drones      |

### Power Monitoring

| Sensor    | Manufacturer      | Interface     | Outputs & Ranges                                                    | Notes                                                                                     |
| --------- | ----------------- | ------------- | ------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| INA219    | Texas Instruments | I2C           | Voltage: 0–26V; Current: via shunt, ±3.2A (at 0.1Ω); Power computed | Bidirectional, programmable gain. Very common                                             |
| INA260    | Texas Instruments | I2C           | Voltage: 0–36V; Current: ±15A (integrated 2mΩ shunt); Power         | No external shunt needed, easy to use                                                     |
| INA226    | Texas Instruments | I2C           | Voltage: 0–36V; Current: via shunt, bidirectional; Power            | Higher precision than INA219, alert function                                              |
| ACS712    | Allegro           | Analog        | Current: ±5A, ±20A, or ±30A variants                                | Hall effect, analog output, isolated, cheap but noisy                                     |
| ADS1115   | Texas Instruments | I2C           | 4-ch 16-bit ADC, ±0.256V to ±6.144V FSR                             | Not a current sensor but essential for precise voltage measurement from any analog sensor |
| PZEM-004T | Peacefair         | UART (Modbus) | Voltage: 80–260V AC; Current: 0–100A (CT); Power; Energy; Freq; PF  | Mains monitoring module with CT clamp, complete solution                                  |

### LoRa / Radio Modules (relevant for link quality fields)

| Sensor            | Manufacturer | Interface          | Outputs & Ranges                                     | Notes                                                       |
| ----------------- | ------------ | ------------------ | ---------------------------------------------------- | ----------------------------------------------------------- |
| SX1276 / SX1278   | Semtech      | SPI                | RSSI: -127 to 0 dBm; SNR: -20 to +10 dB; Packet RSSI | LoRa transceiver, 137–525 MHz (SX1278) or 868/915 (SX1276)  |
| SX1262 / SX1268   | Semtech      | SPI                | RSSI, SNR, signal RSSI                               | Next-gen LoRa, lower power, better sensitivity              |
| RFM95W / RFM96W   | HopeRF       | SPI                | SX1276-based module, same outputs                    | Ready-made module, easier than bare IC                      |
| RYLR896 / RYLR998 | REYAX        | UART (AT commands) | RSSI, SNR                                            | Serial LoRa module, simple AT interface, good for beginners |
| E220-900T         | Ebyte        | UART               | RSSI                                                 | SX1262-based, various power/antenna options                 |

---

## Sensor Products (Integrated Systems)

Complete sensor products or sensor ecosystems — not bare ICs. These produce data
that might be ingested by an iotdata gateway or translated at a bridge.

### Weather Station Products

| Product                 | Manufacturer      | Connectivity                          | Measurements                                                                              | Notes                                                                |
| ----------------------- | ----------------- | ------------------------------------- | ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| Ecowitt GW2000 / HP2560 | Ecowitt           | Wi-Fi gateway, 915/868 MHz to sensors | Temp, humidity, wind, rain, solar, UV, pressure (via gateway)                             | Cheap ecosystem, many compatible sensors, custom protocol on 915/868 |
| Ecowitt WS90 (Wittboy)  | Ecowitt           | 915/868 MHz                           | Wind (ultrasonic), temp, humidity, rain (haptic), light, UV                               | All-in-one array, no moving parts, solar powered                     |
| Ecowitt WH51            | Ecowitt           | 915/868 MHz                           | Soil moisture: 0–100%                                                                     | Cheap capacitive soil probe, CR2032 battery                          |
| Ecowitt WN34            | Ecowitt           | 915/868 MHz                           | Temperature (waterproof probe)                                                            | Pool/soil/pipe temp, various probe lengths                           |
| Ecowitt WH45            | Ecowitt           | 915/868 MHz                           | CO₂: 400–5000 ppm; PM2.5; PM10; Temp; Humidity                                            | Indoor air quality 5-in-1                                            |
| Davis Vantage Vue       | Davis Instruments | 915 MHz proprietary                   | Temp, humidity, wind, rain, pressure                                                      | Reliable, professional amateur, all-in-one outdoor sensor suite      |
| Davis Vantage Pro2      | Davis Instruments | 915 MHz proprietary                   | As Vue + solar, UV; optional soil/leaf stations                                           | Modular, add-on sensors, widely used in agriculture                  |
| Davis AirLink           | Davis Instruments | Wi-Fi                                 | PM1, PM2.5, PM10 + AQI                                                                    | Pairs with Vantage ecosystem                                         |
| Tempest (WeatherFlow)   | WeatherFlow       | 915 MHz + Wi-Fi hub                   | Wind (ultrasonic), temp, humidity, pressure, rain (haptic), solar, UV, lightning (AS3935) | All-in-one, solar powered, haptic rain sensor, built-in lightning    |
| Ambient Weather WS-5000 | Ambient Weather   | 915 MHz + Wi-Fi                       | Temp, humidity, wind, rain, solar, UV, pressure                                           | Ecowitt OEM rebrand with different cloud service                     |
| Bresser 7-in-1          | Bresser           | 868 MHz                               | Temp, humidity, wind, rain, solar, UV                                                     | European alternative, 868 MHz                                        |
| MiSol weather sensors   | MiSol             | 433/868/915 MHz                       | Various: temp, humidity, wind, rain, soil                                                 | Very cheap, compatible with Ecowitt gateways in some models          |

### Soil / Agriculture Products

| Product                    | Manufacturer | Connectivity    | Measurements                                                       | Notes                                                  |
| -------------------------- | ------------ | --------------- | ------------------------------------------------------------------ | ------------------------------------------------------ |
| Meter Group Teros stations | Meter Group  | SDI-12 → logger | VWC, EC, temp, water potential (sensor dependent)                  | Research grade, ZL6 logger with cellular/Wi-Fi         |
| Meter Group Atmos 41       | Meter Group  | SDI-12          | All-in-one: wind, temp, RH, pressure, solar, rain, lightning, tilt | All-in-one weather station for ag, SDI-12 output       |
| Dragino LSE01              | Dragino      | LoRaWAN         | Soil moisture + EC + temp (capacitive)                             | LoRaWAN soil sensor, solar powered, cheap (~$40)       |
| SenseCAP S2105             | Seeed Studio | LoRaWAN         | Soil moisture + temp                                               | LoRaWAN, IP66, RS485 sensor input                      |
| SenseCAP S2100             | Seeed Studio | LoRaWAN         | Data logger for any RS485/analog/GPIO sensor                       | Versatile LoRaWAN gateway for third-party sensors      |
| Milesight EM500 series     | Milesight    | LoRaWAN         | Various: soil moisture/temp/EC, CO₂, light, temp/humidity          | Industrial IoT LoRaWAN sensors, IP67, battery powered  |
| RAK10701 Field Tester      | RAKwireless  | LoRaWAN         | Coverage mapping (RSSI, SNR, GPS)                                  | Not a sensor per se, but useful for LoRa field testing |

### Water Quality Products

| Product                         | Manufacturer      | Connectivity            | Measurements                                                       | Notes                                                        |
| ------------------------------- | ----------------- | ----------------------- | ------------------------------------------------------------------ | ------------------------------------------------------------ |
| Atlas Scientific Wi-Fi Pool Kit | Atlas Scientific  | Wi-Fi (ESP32-based)     | pH, ORP, DO, EC, Temp (modular EZO boards)                         | Build-your-own combo, Whitebox carrier + EZO modules         |
| Atlas Scientific Gravity boards | Atlas Scientific  | Analog (Arduino)        | pH, EC, DO, ORP, Temp                                              | Budget line, analog output, less precise than EZO            |
| YSI EXO2                        | Xylem/YSI         | RS485/SDI-12/Bluetooth  | pH, EC, DO, turbidity, chlorophyll, fDOM, temp, depth (modular)    | Professional multi-parameter sonde, very expensive (~$10k+)  |
| In-Situ Aqua TROLL series       | In-Situ           | SDI-12/Modbus/Bluetooth | Various: pH, EC, DO, turbidity, pressure, temp, ORP                | Professional, submersible, field-swappable sensors           |
| Hanna HI98194                   | Hanna Instruments | Bluetooth               | pH, EC, DO, pressure, temp                                         | Portable multiparameter meter, lab-grade                     |
| LilyGo T-HiGrow                 | LilyGo            | Wi-Fi (ESP32)           | Soil moisture, temp, humidity, light, salt (conductivity), battery | Very cheap dev board, mediocre sensors, good for prototyping |

### Air Quality Products

| Product                     | Manufacturer | Connectivity       | Measurements                                                            | Notes                                                         |
| --------------------------- | ------------ | ------------------ | ----------------------------------------------------------------------- | ------------------------------------------------------------- |
| PurpleAir PA-II             | PurpleAir    | Wi-Fi              | PM1, PM2.5, PM10 (dual PMS5003), temp, humidity, pressure               | Dual laser counters for data validation, community network    |
| Airthings Wave Plus         | Airthings    | BLE + hub          | Radon (Bq/m³), CO₂, TVOC, temp, humidity, pressure                      | Indoor, radon is key differentiator                           |
| Airthings View Plus         | Airthings    | Wi-Fi/BLE          | As Wave Plus + PM2.5                                                    | Adds particulate                                              |
| uRADMonitor A3 / model A    | uRADMonitor  | Wi-Fi/Ethernet     | Radiation (CPM, µSv/h), PM2.5, CO₂, HCHO, VOC, temp, humidity, pressure | Environmental monitoring, community network                   |
| Air Gradient ONE / Open Air | Air Gradient | Wi-Fi (ESP32)      | PM2.5 (PMS5003T), CO₂ (SenseAir S8), TVOC (SGP41), temp, humidity       | Open-source design, kits available, ESPHome compatible        |
| SenseCAP Indicator          | Seeed Studio | Wi-Fi/BLE          | CO₂ (SCD41), TVOC (SGP41), temp, humidity                               | Desk unit with screen, ESP32-S3 + RP2040                      |
| Aranet4                     | SAF Tehnika  | BLE + optional hub | CO₂ (NDIR), temp, humidity, pressure                                    | Very popular indoor CO₂ monitor, long battery life (2+ years) |
| Awair Element               | Awair        | Wi-Fi              | CO₂, TVOC, PM2.5, temp, humidity                                        | Consumer indoor AQ, good app, local API available             |

### Multi-Purpose IoT / LoRaWAN Sensors

| Product                | Manufacturer | Connectivity       | Measurements                                                                    | Notes                                                     |
| ---------------------- | ------------ | ------------------ | ------------------------------------------------------------------------------- | --------------------------------------------------------- |
| Dragino LHT65N         | Dragino      | LoRaWAN            | Temp + humidity (SHT31), external probe input                                   | Cheap LoRaWAN sensor, external DS18B20 option             |
| Dragino LDDS75         | Dragino      | LoRaWAN            | Distance (ultrasonic): 28–350 cm                                                | Water level / bin level sensor                            |
| Dragino LSPH01         | Dragino      | LoRaWAN            | Soil pH                                                                         | LoRaWAN soil pH sensor                                    |
| Dragino LWL02          | Dragino      | LoRaWAN            | Water leak detection (binary + depth)                                           | Flood detection                                           |
| SenseCAP T1000         | Seeed Studio | LoRaWAN + GNSS     | GPS, temp, light, 3-axis accel                                                  | Tracker/asset tag with environmental sensing              |
| RAK Wisblock ecosystem | RAKwireless  | LoRaWAN            | Modular: base board + sensor boards (accel, env, GPS, etc.)                     | Build-your-own LoRaWAN sensor, very flexible              |
| TTGO/LilyGo T-Beam     | LilyGo       | LoRa + Wi-Fi + BLE | GPS (NEO-6M or M8N), LoRa (SX1276/62), Wi-Fi, BLE                               | Dev board, popular for Meshtastic, needs external sensors |
| Heltec CubeCell series | Heltec       | LoRaWAN            | Various boards: GPS, solar, OLED options                                        | Ultra-low power LoRa dev boards (ASR6501/ASR6502)         |
| Decentlab sensors      | Decentlab    | LoRaWAN            | Various: soil moisture, distance, pressure, CO₂, air temp/humidity, dendrometer | Swiss-made, research grade, expensive but reliable, IP67  |
| Milesight AM100 series | Milesight    | LoRaWAN            | Temp, humidity, CO₂, TVOC, light, motion, pressure, sound                       | Indoor ambiance monitoring, occupancy sensing             |
| Elsys ERS series       | Elsys        | LoRaWAN            | Temp, humidity, light, motion, CO₂, sound, acceleration                         | Indoor LoRaWAN multi-sensor, Scandinavian quality         |

### Beehive / Livestock / Speciality Agriculture

| Product                | Manufacturer        | Connectivity   | Measurements                                       | Notes                           |
| ---------------------- | ------------------- | -------------- | -------------------------------------------------- | ------------------------------- |
| Arnia hive monitor     | Arnia               | Cellular/Wi-Fi | Weight, temp, humidity, acoustics (bee activity)   | Commercial beehive monitoring   |
| HiveMind (DIY)         | Various open-source | LoRa/Wi-Fi     | Weight (HX711 + 4×50kg load cells), temp, humidity | Common DIY beehive scale design |
| Digitanimal GPS collar | Digitanimal         | Cellular       | GPS, activity (accelerometer)                      | Livestock tracking              |
| MOOvement ear tag      | MOOvement           | Cellular/LPWAN | GPS, activity, temperature                         | Cattle monitoring               |

### Indoor / Building Sensors

| Product                            | Manufacturer | Connectivity | Measurements                                                      | Notes                                                              |
| ---------------------------------- | ------------ | ------------ | ----------------------------------------------------------------- | ------------------------------------------------------------------ |
| Aqara sensors (door, motion, temp) | Aqara        | Zigbee       | Various: door open/close, motion, temp/humidity, light, vibration | Consumer smart home, very cheap, Zigbee ecosystem                  |
| Xiaomi/Mi sensors                  | Xiaomi       | BLE/Zigbee   | Temp/humidity, door, motion, light, plant soil moisture           | BLE advertising, can be captured by ESPHome                        |
| Shelly H&T                         | Shelly       | Wi-Fi/MQTT   | Temp: -40 to +60°C; Humidity: 0–100%                              | Wi-Fi, battery powered, MQTT/CoAP native                           |
| Shelly Flood                       | Shelly       | Wi-Fi/MQTT   | Water detection (binary), temperature                             | Floor-level water detection                                        |
| Ruuvi RuuviTag                     | Ruuvi        | BLE          | Temp: -40 to +85°C; Humidity; Pressure; Acceleration (LIS2DH12)   | BLE beacon with environmental sensing, open firmware, Nordic nRF52 |
| Blue Maestro Tempo Disc            | Blue Maestro | BLE          | Temp, humidity, dew point (±0.1°C)                                | BLE logger, long battery, data logging on device                   |
| SwitchBot Meter                    | SwitchBot    | BLE + hub    | Temp, humidity                                                    | Consumer, app-based, hub adds cloud connectivity                   |

---

## Notes

- Prices fluctuate; approximate costs noted where they indicate tier
  (budget/mid/research-grade)
- Many sensors have Adafruit, SparkFun, DFRobot, or Seeed Studio breakout boards
  that make prototyping easier — search "[sensor] breakout" for options
- SDI-12 sensors (Meter, Campbell) need an SDI-12 to UART/I2C adapter for
  microcontroller integration
- Atlas Scientific EZO modules can be daisy-chained on I2C — good for
  multi-parameter water monitoring rigs
- For LoRaWAN products, CayenneLPP and SenML are common payload formats —
  iotdata would replace these with a more efficient encoding

---

_Last updated: 2026-02-27_
