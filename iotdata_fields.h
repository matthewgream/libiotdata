
/* ---------------------------------------------------------------------------
 * Field BATTERY
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_BATTERY)
#define IOTDATA_FIELDS_DAT_BATTERY \
    uint8_t battery_level; \
    bool battery_charging;
#define IOTDATA_BATTERY_LEVEL_MAX   100
#define IOTDATA_BATTERY_LEVEL_BITS  5
#define IOTDATA_BATTERY_CHARGE_BITS 1
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_battery(struct iotdata_encoder_t_ *enc, uint8_t level_percent, bool charging);
#endif
#define IOTDATA_FIELDS_DEF_BATTERY(F)         F(BATTERY)
#define IOTDATA_FIELDS_OPS_BATTERY(E)         E(BATTERY, _iotdata_field_def_battery)
#define IOTDATA_FIELDS_ERR_IDENTS_BATTERY(S)  S(IOTDATA_ERR_BATTERY_LEVEL_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_BATTERY(D) D(IOTDATA_ERR_BATTERY_LEVEL_HIGH, "Battery level above 100%")
#else
#define IOTDATA_FIELDS_DAT_BATTERY
#define IOTDATA_FIELDS_DEF_BATTERY(F)
#define IOTDATA_FIELDS_OPS_BATTERY(E)
#define IOTDATA_FIELDS_ERR_IDENTS_BATTERY(S)
#define IOTDATA_FIELDS_ERR_STRINGS_BATTERY(D)
#endif

/* ---------------------------------------------------------------------------
 * Field LINK
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_LINK)
#define IOTDATA_FIELDS_DAT_LINK \
    int8_t link_rssi; \
    iotdata_float_t link_snr;
#define IOTDATA_LINK_RSSI_MIN  (-120)
#define IOTDATA_LINK_RSSI_MAX  (-60)
#define IOTDATA_LINK_RSSI_STEP (4)
#define IOTDATA_LINK_RSSI_BITS (4)
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
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_link(struct iotdata_encoder_t_ *enc, int8_t rssi_dbm, iotdata_float_t snr_db);
#endif
#define IOTDATA_FIELDS_DEF_LINK(F) F(LINK)
#define IOTDATA_FIELDS_OPS_LINK(E) E(LINK, _iotdata_field_def_link)
#define IOTDATA_FIELDS_ERR_IDENTS_LINK(S) \
    S(IOTDATA_ERR_LINK_RSSI_LOW) \
    S(IOTDATA_ERR_LINK_RSSI_HIGH) \
    S(IOTDATA_ERR_LINK_SNR_LOW) \
    S(IOTDATA_ERR_LINK_SNR_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_LINK(D) \
    D(IOTDATA_ERR_LINK_RSSI_LOW, "RSSI below -120 dBm") \
    D(IOTDATA_ERR_LINK_RSSI_HIGH, "RSSI above -60 dBm") \
    D(IOTDATA_ERR_LINK_SNR_LOW, "SNR below -20 dB") \
    D(IOTDATA_ERR_LINK_SNR_HIGH, "SNR above +10 dB")
#else
#define IOTDATA_FIELDS_DAT_LINK
#define IOTDATA_FIELDS_DEF_LINK(F)
#define IOTDATA_FIELDS_OPS_LINK(E)
#define IOTDATA_FIELDS_ERR_IDENTS_LINK(S)
#define IOTDATA_FIELDS_ERR_STRINGS_LINK(D)
#endif

/* ---------------------------------------------------------------------------
 * Field ENVIRONMENT, TEMPERATURE, PRESSURE, HUMIDITY
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_TEMPERATURE)
#define IOTDATA_FIELDS_DAT_TEMPERATURE iotdata_float_t temperature;
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
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_temperature(struct iotdata_encoder_t_ *enc, iotdata_float_t temperature_c);
#endif
#define IOTDATA_FIELDS_DEF_TEMPERATURE(F) F(TEMPERATURE)
#define IOTDATA_FIELDS_ERR_IDENTS_TEMPERATURE(S) \
    S(IOTDATA_ERR_TEMPERATURE_LOW) \
    S(IOTDATA_ERR_TEMPERATURE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_TEMPERATURE(D) \
    D(IOTDATA_ERR_TEMPERATURE_LOW, "Temperature below -40C") \
    D(IOTDATA_ERR_TEMPERATURE_HIGH, "Temperature above +80C")
#else
#define IOTDATA_FIELDS_DAT_TEMPERATURE
#define IOTDATA_FIELDS_DEF_TEMPERATURE(F)
#define IOTDATA_FIELDS_ERR_IDENTS_TEMPERATURE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_TEMPERATURE(D)
#endif
#if defined(IOTDATA_ENABLE_TEMPERATURE)
#define IOTDATA_FIELDS_OPS_TEMPERATURE(E) E(TEMPERATURE, _iotdata_field_def_temperature)
#else
#define IOTDATA_FIELDS_OPS_TEMPERATURE(E)
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_PRESSURE)
#define IOTDATA_FIELDS_DAT_PRESSURE uint16_t pressure;
#define IOTDATA_PRESSURE_MIN        850
#define IOTDATA_PRESSURE_MAX        1105
#define IOTDATA_PRESSURE_BITS       8
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_pressure(struct iotdata_encoder_t_ *enc, uint16_t pressure_hpa);
#endif
#define IOTDATA_FIELDS_DEF_PRESSURE(F) F(PRESSURE)
#define IOTDATA_FIELDS_ERR_IDENTS_PRESSURE(S) \
    S(IOTDATA_ERR_PRESSURE_LOW) \
    S(IOTDATA_ERR_PRESSURE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_PRESSURE(D) \
    D(IOTDATA_ERR_PRESSURE_LOW, "Pressure below 850 hPa") \
    D(IOTDATA_ERR_PRESSURE_HIGH, "Pressure above 1105 hPa")
#else
#define IOTDATA_FIELDS_DAT_PRESSURE
#define IOTDATA_FIELDS_DEF_PRESSURE(F)
#define IOTDATA_FIELDS_ERR_IDENTS_PRESSURE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_PRESSURE(D)
#endif
#if defined(IOTDATA_ENABLE_PRESSURE)
#define IOTDATA_FIELDS_OPS_PRESSURE(E) E(PRESSURE, _iotdata_field_def_pressure)
#else
#define IOTDATA_FIELDS_OPS_PRESSURE(E)
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT) || defined(IOTDATA_ENABLE_HUMIDITY)
#define IOTDATA_FIELDS_DAT_HUMIDITY uint8_t humidity;
#define IOTDATA_HUMIDITY_MAX        100
#define IOTDATA_HUMIDITY_BITS       7
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_humidity(struct iotdata_encoder_t_ *enc, uint8_t humidity_pct);
#endif
#define IOTDATA_FIELDS_DEF_HUMIDITY(F)         F(HUMIDITY)
#define IOTDATA_FIELDS_ERR_IDENTS_HUMIDITY(S)  S(IOTDATA_ERR_HUMIDITY_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_HUMIDITY(D) D(IOTDATA_ERR_HUMIDITY_HIGH, "Humidity above 100%")
#else
#define IOTDATA_FIELDS_DAT_HUMIDITY
#define IOTDATA_FIELDS_DEF_HUMIDITY(F)
#define IOTDATA_FIELDS_ERR_IDENTS_HUMIDITY(S)
#define IOTDATA_FIELDS_ERR_STRINGS_HUMIDITY(D)
#endif
#if defined(IOTDATA_ENABLE_HUMIDITY)
#define IOTDATA_FIELDS_OPS_HUMIDITY(E) E(HUMIDITY, _iotdata_field_def_humidity)
#else
#define IOTDATA_FIELDS_OPS_HUMIDITY(E)
#endif
#if defined(IOTDATA_ENABLE_ENVIRONMENT)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_environment(struct iotdata_encoder_t_ *enc, iotdata_float_t temp_c, uint16_t pressure_hpa, uint8_t humidity_pct);
#endif
#define IOTDATA_FIELDS_DEF_ENVIRONMENT(F) F(ENVIRONMENT)
#define IOTDATA_FIELDS_OPS_ENVIRONMENT(E) E(ENVIRONMENT, _iotdata_field_def_environment)
#else
#define IOTDATA_FIELDS_DEF_ENVIRONMENT(F)
#define IOTDATA_FIELDS_OPS_ENVIRONMENT(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_ENVIRONMENT(S)
#define IOTDATA_FIELDS_ERR_STRINGS_ENVIRONMENT(D)
#define IOTDATA_FIELDS_DAT_ENVIRONMENT IOTDATA_FIELDS_DAT_TEMPERATURE IOTDATA_FIELDS_DAT_PRESSURE IOTDATA_FIELDS_DAT_HUMIDITY

/* ---------------------------------------------------------------------------
 * Field WIND, WIND_SPEED, WIND_DIRECTION, WIND_GUST
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_SPEED)
#define IOTDATA_FIELDS_DAT_WIND_SPEED iotdata_float_t wind_speed;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_WIND_SPEED_RES (0.5f)
#define IOTDATA_WIND_SPEED_MAX (63.5f)
#else
#define IOTDATA_WIND_SPEED_RES (50)
#define IOTDATA_WIND_SPEED_MAX (6350)
#endif
#define IOTDATA_WIND_SPEED_BITS 7
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_wind_speed(struct iotdata_encoder_t_ *enc, iotdata_float_t speed_ms);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_SPEED(S)  S(IOTDATA_ERR_WIND_SPEED_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_SPEED(D) D(IOTDATA_ERR_WIND_SPEED_HIGH, "Wind speed above 63.5 m/s")
#else
#define IOTDATA_FIELDS_DAT_WIND_SPEED
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_SPEED(S)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_SPEED(D)
#endif
#if defined(IOTDATA_ENABLE_WIND_SPEED)
#define IOTDATA_FIELDS_DEF_WIND_SPEED(F) F(WIND_SPEED)
#define IOTDATA_FIELDS_OPS_WIND_SPEED(E) E(WIND_SPEED, _iotdata_field_def_wind_speed)
#else
#define IOTDATA_FIELDS_DEF_WIND_SPEED(F)
#define IOTDATA_FIELDS_OPS_WIND_SPEED(E)
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_DIRECTION)
#define IOTDATA_DAT_WIND_DIRECTION  uint16_t wind_direction;
#define IOTDATA_WIND_DIRECTION_MAX  359
#define IOTDATA_WIND_DIRECTION_BITS 8
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_wind_direction(struct iotdata_encoder_t_ *enc, uint16_t direction_deg);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_DIRECTION(S)  S(IOTDATA_ERR_WIND_DIRECTION_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_DIRECTION(D) D(IOTDATA_ERR_WIND_DIRECTION_HIGH, "Wind direction above 359 degrees")
#else
#define IOTDATA_DAT_WIND_DIRECTION
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_DIRECTION(S)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_DIRECTION(D)
#endif
#if defined(IOTDATA_ENABLE_WIND_DIRECTION)
#define IOTDATA_FIELDS_DEF_WIND_DIRECTION(F) F(WIND_DIRECTION)
#define IOTDATA_FIELDS_OPS_WIND_DIRECTION(E) E(WIND_DIRECTION, _iotdata_field_def_wind_direction)
#else
#define IOTDATA_FIELDS_DEF_WIND_DIRECTION(F)
#define IOTDATA_FIELDS_OPS_WIND_DIRECTION(E)
#endif
#if defined(IOTDATA_ENABLE_WIND) || defined(IOTDATA_ENABLE_WIND_GUST)
#define IOTDATA_FIELDS_DAT_WIND_GUST iotdata_float_t wind_gust;
#define IOTDATA_WIND_GUST_BITS       7
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_wind_gust(struct iotdata_encoder_t_ *enc, iotdata_float_t gust_ms);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_GUST(S)  S(IOTDATA_ERR_WIND_GUST_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_GUST(D) D(IOTDATA_ERR_WIND_GUST_HIGH, "Wind gust above 63.5 m/s")
#else
#define IOTDATA_FIELDS_DAT_WIND_GUST
#define IOTDATA_FIELDS_ERR_IDENTS_WIND_GUST(S)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND_GUST(D)
#endif
#if defined(IOTDATA_ENABLE_WIND_GUST)
#define IOTDATA_FIELDS_DEF_WIND_GUST(F) F(WIND_GUST)
#define IOTDATA_FIELDS_OPS_WIND_GUST(E) E(WIND_GUST, _iotdata_field_def_wind_gust)
#else
#define IOTDATA_FIELDS_DEF_WIND_GUST(F)
#define IOTDATA_FIELDS_OPS_WIND_GUST(E)
#endif
#if defined(IOTDATA_ENABLE_WIND)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_wind(struct iotdata_encoder_t_ *enc, iotdata_float_t speed_ms, uint16_t direction_deg, iotdata_float_t gust_ms);
#endif
#define IOTDATA_FIELDS_DEF_WIND(F) F(WIND)
#define IOTDATA_FIELDS_OPS_WIND(E) E(WIND, _iotdata_field_def_wind)
#else
#define IOTDATA_FIELDS_DEF_WIND(F)
#define IOTDATA_FIELDS_OPS_WIND(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_WIND(S)
#define IOTDATA_FIELDS_ERR_STRINGS_WIND(D)
#define IOTDATA_FIELDS_DAT_WIND IOTDATA_FIELDS_DAT_WIND_SPEED IOTDATA_DAT_WIND_DIRECTION IOTDATA_FIELDS_DAT_WIND_GUST

/* ---------------------------------------------------------------------------
 * Field RAIN, RAIN_RATE, RAIN_SIZE
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_RATE)
#define IOTDATA_FIELDS_DAT_RAIN_RATE uint8_t rain_rate;
#define IOTDATA_RAIN_RATE_MAX        255
#define IOTDATA_RAIN_RATE_BITS       8
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_rain_rate(struct iotdata_encoder_t_ *enc, uint8_t rate_mmhr);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RAIN_RATE(S)  S(IOTDATA_ERR_RAIN_RATE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_RAIN_RATE(D) D(IOTDATA_ERR_RAIN_RATE_HIGH, "Rain rate above 255 mm/hr")
#else
#define IOTDATA_FIELDS_DAT_RAIN_RATE
#define IOTDATA_FIELDS_ERR_IDENTS_RAIN_RATE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RAIN_RATE(D)
#endif
#if defined(IOTDATA_ENABLE_RAIN_RATE)
#define IOTDATA_FIELDS_DEF_RAIN_RATE(F) F(RAIN_RATE)
#define IOTDATA_FIELDS_OPS_RAIN_RATE(E) E(RAIN_RATE, _iotdata_field_def_rain_rate)
#else
#define IOTDATA_FIELDS_DEF_RAIN_RATE(F)
#define IOTDATA_FIELDS_OPS_RAIN_RATE(E)
#endif
#if defined(IOTDATA_ENABLE_RAIN) || defined(IOTDATA_ENABLE_RAIN_SIZE)
#define IOTDATA_FIELDS_DAT_RAIN_SIZE uint8_t rain_size10;
#define IOTDATA_RAIN_SIZE_MAX        15
#define IOTDATA_RAIN_SIZE_BITS       4
#define IOTDATA_RAIN_SIZE_SCALE      4
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_rain_size(struct iotdata_encoder_t_ *enc, uint8_t size_mmd);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RAIN_SIZE(S)  S(IOTDATA_ERR_RAIN_SIZE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_RAIN_SIZE(D) D(IOTDATA_ERR_RAIN_SIZE_HIGH, "Rain size above 6.0 mm/d")
#else
#define IOTDATA_FIELDS_DAT_RAIN_SIZE
#define IOTDATA_FIELDS_ERR_IDENTS_RAIN_SIZE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RAIN_SIZE(D)
#endif
#if defined(IOTDATA_ENABLE_RAIN_SIZE)
#define IOTDATA_FIELDS_DEF_RAIN_SIZE(F) F(RAIN_SIZE)
#define IOTDATA_FIELDS_OPS_RAIN_SIZE(E) E(RAIN_SIZE, _iotdata_field_def_rain_size)
#else
#define IOTDATA_FIELDS_DEF_RAIN_SIZE(F)
#define IOTDATA_FIELDS_OPS_RAIN_SIZE(E)
#endif
#if defined(IOTDATA_ENABLE_RAIN)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_rain(struct iotdata_encoder_t_ *enc, uint8_t rate_mmhr, uint8_t size_mmd);
#endif
#define IOTDATA_FIELDS_DEF_RAIN(F) F(RAIN)
#define IOTDATA_FIELDS_OPS_RAIN(E) E(RAIN, _iotdata_field_def_rain)
#else
#define IOTDATA_FIELDS_DEF_RAIN(F)
#define IOTDATA_FIELDS_OPS_RAIN(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RAIN(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RAIN(D)
#define IOTDATA_FIELDS_DAT_RAIN IOTDATA_FIELDS_DAT_RAIN_RATE IOTDATA_FIELDS_DAT_RAIN_SIZE

/* ---------------------------------------------------------------------------
 * Field SOLAR, SOLAR_IRRADIATION, SOLAR_ULTRAVIOLET
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_SOLAR)
#define IOTDATA_FIELDS_DAT_SOLAR \
    uint16_t solar_irradiance; \
    uint8_t solar_ultraviolet;
#define IOTDATA_SOLAR_IRRADIATION_MAX  1023
#define IOTDATA_SOLAR_IRRADIATION_BITS 10
#define IOTDATA_SOLAR_ULTRAVIOLET_MAX  15
#define IOTDATA_SOLAR_ULTRAVIOLET_BITS 4
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_solar(struct iotdata_encoder_t_ *enc, uint16_t irradiance_wm2, uint8_t ultraviolet_index);
#endif
#define IOTDATA_FIELDS_DEF_SOLAR(F) F(SOLAR)
#define IOTDATA_FIELDS_OPS_SOLAR(E) E(SOLAR, _iotdata_field_def_solar)
#define IOTDATA_FIELDS_ERR_IDENTS_SOLAR(S) \
    S(IOTDATA_ERR_SOLAR_IRRADIATION_HIGH) \
    S(IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_SOLAR(D) \
    D(IOTDATA_ERR_SOLAR_IRRADIATION_HIGH, "Solar irradiance above 1023 W/m2") \
    D(IOTDATA_ERR_SOLAR_ULTRAVIOLET_HIGH, "Solar ultraviolet index above 15")
#else
#define IOTDATA_FIELDS_DAT_SOLAR
#define IOTDATA_FIELDS_DEF_SOLAR(F)
#define IOTDATA_FIELDS_OPS_SOLAR(E)
#define IOTDATA_FIELDS_ERR_IDENTS_SOLAR(S)
#define IOTDATA_FIELDS_ERR_STRINGS_SOLAR(D)
#endif

/* ---------------------------------------------------------------------------
 * Field CLOUDS
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_CLOUDS)
#define IOTDATA_FIELDS_DAT_CLOUDS uint8_t clouds;
#define IOTDATA_CLOUDS_MAX        8
#define IOTDATA_CLOUDS_BITS       4
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_clouds(struct iotdata_encoder_t_ *enc, uint8_t okta);
#endif
#define IOTDATA_FIELDS_DEF_CLOUDS(F)         F(CLOUDS)
#define IOTDATA_FIELDS_OPS_CLOUDS(E)         E(CLOUDS, _iotdata_field_def_clouds)
#define IOTDATA_FIELDS_ERR_IDENTS_CLOUDS(S)  S(IOTDATA_ERR_CLOUDS_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_CLOUDS(D) D(IOTDATA_ERR_CLOUDS_HIGH, "Cloud cover above 8 okta")
#else
#define IOTDATA_FIELDS_DAT_CLOUDS
#define IOTDATA_FIELDS_DEF_CLOUDS(F)
#define IOTDATA_FIELDS_OPS_CLOUDS(E)
#define IOTDATA_FIELDS_ERR_IDENTS_CLOUDS(S)
#define IOTDATA_FIELDS_ERR_STRINGS_CLOUDS(D)
#endif

/* ---------------------------------------------------------------------------
 * Field AIR_QUALITY, AIR_QUALITY_INDEX, AIR_QUALITY_PM, AIR_QUALITY_GAS
 * -------------------------------------------------------------------------*/

/* --- Sub-field: AQ_INDEX (AQI 0-500, 9 bits) --- */
#if defined(IOTDATA_ENABLE_AIR_QUALITY) || defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX)
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_INDEX uint16_t aq_index;
#define IOTDATA_AIR_QUALITY_INDEX_MAX        500
#define IOTDATA_AIR_QUALITY_INDEX_BITS       9
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_air_quality_index(struct iotdata_encoder_t_ *enc, uint16_t aq_index);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_INDEX(S)  S(IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_INDEX(D) D(IOTDATA_ERR_AIR_QUALITY_INDEX_HIGH, "AQ index above 500 AQI")
#else
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_INDEX
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_INDEX(S)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_INDEX(D)
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_INDEX)
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_INDEX(F) F(AIR_QUALITY_INDEX)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_INDEX(E) E(AIR_QUALITY_INDEX, _iotdata_field_def_aq_index)
#else
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_INDEX(F)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_INDEX(E)
#endif
/* --- Sub-field: AQ_PM (4 channels, presence + 8 bits each, res 5 ug/m3) --- */
#if defined(IOTDATA_ENABLE_AIR_QUALITY) || defined(IOTDATA_ENABLE_AIR_QUALITY_PM)
#define IOTDATA_AIR_QUALITY_PM_COUNT 4
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_PM \
    uint8_t aq_pm_present; \
    uint16_t aq_pm[IOTDATA_AIR_QUALITY_PM_COUNT];
#define IOTDATA_AIR_QUALITY_PM_PRESENT_BITS 4
#define IOTDATA_AIR_QUALITY_PM_VALUE_BITS   8
#define IOTDATA_AIR_QUALITY_PM_VALUE_RES    5
#define IOTDATA_AIR_QUALITY_PM_VALUE_MAX    1275 /* 255 * 5 */

#define IOTDATA_AIR_QUALITY_PM_INDEX_PM1    0
#define IOTDATA_AIR_QUALITY_PM_INDEX_PM25   1
#define IOTDATA_AIR_QUALITY_PM_INDEX_PM4    2
#define IOTDATA_AIR_QUALITY_PM_INDEX_PM10   3
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_air_quality_pm(struct iotdata_encoder_t_ *enc, uint8_t pm_present, const uint16_t pm[4]);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_PM(S)  S(IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_PM(D) D(IOTDATA_ERR_AIR_QUALITY_PM_VALUE_HIGH, "AQ PM value above 1275 ug/m3")
#else
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_PM
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_PM(S)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_PM(D)
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_PM)
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_PM(F) F(AIR_QUALITY_PM)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_PM(E) E(AIR_QUALITY_PM, _iotdata_field_def_aq_pm)
#else
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_PM(F)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_PM(E)
#endif
/* --- Sub-field: AQ_GAS (8 slots, presence + variable bits per slot) --- */
#if defined(IOTDATA_ENABLE_AIR_QUALITY) || defined(IOTDATA_ENABLE_AIR_QUALITY_GAS)
#define IOTDATA_AIR_QUALITY_GAS_COUNT 8
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_GAS \
    uint8_t aq_gas_present; \
    uint16_t aq_gas[IOTDATA_AIR_QUALITY_GAS_COUNT];
#define IOTDATA_AIR_QUALITY_GAS_PRESENT_BITS 8
#define IOTDATA_AIR_QUALITY_GAS_INDEX_VOC    0
#define IOTDATA_AIR_QUALITY_GAS_INDEX_NOX    1
#define IOTDATA_AIR_QUALITY_GAS_INDEX_CO2    2
#define IOTDATA_AIR_QUALITY_GAS_INDEX_CO     3
#define IOTDATA_AIR_QUALITY_GAS_INDEX_HCHO   4
#define IOTDATA_AIR_QUALITY_GAS_INDEX_O3     5
#define IOTDATA_AIR_QUALITY_GAS_INDEX_RSVD6  6
#define IOTDATA_AIR_QUALITY_GAS_INDEX_RSVD7  7
/* Bit width per slot: indices=8, concentrations=10 */
#define IOTDATA_AIR_QUALITY_GAS_BITS_VOC     8  /* 0-510, res 2 index pts   */
#define IOTDATA_AIR_QUALITY_GAS_BITS_NOX     8  /* 0-510, res 2 index pts   */
#define IOTDATA_AIR_QUALITY_GAS_BITS_CO2     10 /* 0-51150, res 50 ppm      */
#define IOTDATA_AIR_QUALITY_GAS_BITS_CO      10 /* 0-1023, res 1 ppm        */
#define IOTDATA_AIR_QUALITY_GAS_BITS_HCHO    10 /* 0-5115, res 5 ppb        */
#define IOTDATA_AIR_QUALITY_GAS_BITS_O3      10 /* 0-1023, res 1 ppb        */
#define IOTDATA_AIR_QUALITY_GAS_BITS_RSVD6   10
#define IOTDATA_AIR_QUALITY_GAS_BITS_RSVD7   10
/* Resolution per slot */
#define IOTDATA_AIR_QUALITY_GAS_RES_VOC      2
#define IOTDATA_AIR_QUALITY_GAS_RES_NOX      2
#define IOTDATA_AIR_QUALITY_GAS_RES_CO2      50
#define IOTDATA_AIR_QUALITY_GAS_RES_CO       1
#define IOTDATA_AIR_QUALITY_GAS_RES_HCHO     5
#define IOTDATA_AIR_QUALITY_GAS_RES_O3       1
#define IOTDATA_AIR_QUALITY_GAS_RES_RSVD6    1
#define IOTDATA_AIR_QUALITY_GAS_RES_RSVD7    1
/* Max physical value per slot */
#define IOTDATA_AIR_QUALITY_GAS_MAX_VOC      510
#define IOTDATA_AIR_QUALITY_GAS_MAX_NOX      510
#define IOTDATA_AIR_QUALITY_GAS_MAX_CO2      51150
#define IOTDATA_AIR_QUALITY_GAS_MAX_CO       1023
#define IOTDATA_AIR_QUALITY_GAS_MAX_HCHO     5115
#define IOTDATA_AIR_QUALITY_GAS_MAX_O3       1023
#define IOTDATA_AIR_QUALITY_GAS_MAX_RSVD6    1023
#define IOTDATA_AIR_QUALITY_GAS_MAX_RSVD7    1023
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_air_quality_gas(struct iotdata_encoder_t_ *enc, uint8_t gas_present, const uint16_t gas[8]);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_GAS(S)  S(IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_GAS(D) D(IOTDATA_ERR_AIR_QUALITY_GAS_VALUE_HIGH, "AQ gas value above slot maximum")
#else
#define IOTDATA_FIELDS_DAT_AIR_QUALITY_GAS
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_GAS(S)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_GAS(D)
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY_GAS)
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_GAS(F) F(AIR_QUALITY_GAS)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_GAS(E) E(AIR_QUALITY_GAS, _iotdata_field_def_aq_gas)
#else
#define IOTDATA_FIELDS_DEF_AIR_QUALITY_GAS(F)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY_GAS(E)
#endif
#if defined(IOTDATA_ENABLE_AIR_QUALITY)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_air_quality(struct iotdata_encoder_t_ *enc, uint16_t aq_index, uint8_t pm_present, const uint16_t pm[4], uint8_t gas_present, const uint16_t gas[8]);
#endif
#define IOTDATA_FIELDS_DEF_AIR_QUALITY(F) F(AIR_QUALITY)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY(E) E(AIR_QUALITY, _iotdata_field_def_air_quality)
#else
#define IOTDATA_FIELDS_DEF_AIR_QUALITY(F)
#define IOTDATA_FIELDS_OPS_AIR_QUALITY(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY(S)
#define IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY(D)
#define IOTDATA_FIELDS_DAT_AIR_QUALITY IOTDATA_FIELDS_DAT_AIR_QUALITY_INDEX IOTDATA_FIELDS_DAT_AIR_QUALITY_PM IOTDATA_FIELDS_DAT_AIR_QUALITY_GAS

/* ---------------------------------------------------------------------------
 * Field RADIATION, RADIATION_CPM, RADIATION_DOSE
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_CPM)
#define IOTDATA_FIELDS_DAT_RADIATION_CPM uint16_t radiation_cpm;
#define IOTDATA_RADIATION_CPM_MAX        16383
#define IOTDATA_RADIATION_CPM_BITS       14
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_radiation_cpm(struct iotdata_encoder_t_ *enc, uint16_t cpm);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RADIATION_CPM(S)  S(IOTDATA_ERR_RADIATION_CPM_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_RADIATION_CPM(D) D(IOTDATA_ERR_RADIATION_CPM_HIGH, "Radiation CPM above 65535")
#else
#define IOTDATA_FIELDS_DAT_RADIATION_CPM
#define IOTDATA_FIELDS_ERR_IDENTS_RADIATION_CPM(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RADIATION_CPM(D)
#endif
#if defined(IOTDATA_ENABLE_RADIATION_CPM)
#define IOTDATA_FIELDS_DEF_RADIATION_CPM(F) F(RADIATION_CPM)
#define IOTDATA_FIELDS_OPS_RADIATION_CPM(E) E(RADIATION_CPM, _iotdata_field_def_radiation_cpm)
#else
#define IOTDATA_FIELDS_DEF_RADIATION_CPM(F)
#define IOTDATA_FIELDS_OPS_RADIATION_CPM(E)
#endif
#if defined(IOTDATA_ENABLE_RADIATION) || defined(IOTDATA_ENABLE_RADIATION_DOSE)
#define IOTDATA_FIELDS_DAT_RADIATION_DOSE iotdata_float_t radiation_dose;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_RADIATION_DOSE_MAX_RAW 16383
#define IOTDATA_RADIATION_DOSE_RES     (0.01f)
#define IOTDATA_RADIATION_DOSE_MAX     (163.83f)
#else
#define IOTDATA_RADIATION_DOSE_MAX 16383
#endif
#define IOTDATA_RADIATION_DOSE_BITS 14
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_radiation_dose(struct iotdata_encoder_t_ *enc, iotdata_float_t usvh);
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RADIATION_DOSE(S)  S(IOTDATA_ERR_RADIATION_DOSE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_RADIATION_DOSE(D) D(IOTDATA_ERR_RADIATION_DOSE_HIGH, "Radiation dose above 163.83 uSv/h")
#else
#define IOTDATA_FIELDS_DAT_RADIATION_DOSE
#define IOTDATA_FIELDS_ERR_IDENTS_RADIATION_DOSE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RADIATION_DOSE(D)
#endif
#if defined(IOTDATA_ENABLE_RADIATION_DOSE)
#define IOTDATA_FIELDS_DEF_RADIATION_DOSE(F) F(RADIATION_DOSE)
#define IOTDATA_FIELDS_OPS_RADIATION_DOSE(E) E(RADIATION_DOSE, _iotdata_field_def_radiation_dose)
#else
#define IOTDATA_FIELDS_DEF_RADIATION_DOSE(F)
#define IOTDATA_FIELDS_OPS_RADIATION_DOSE(E)
#endif
#if defined(IOTDATA_ENABLE_RADIATION)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_radiation(struct iotdata_encoder_t_ *enc, uint16_t cpm, iotdata_float_t usvh);
#endif
#define IOTDATA_FIELDS_DEF_RADIATION(F) F(RADIATION)
#define IOTDATA_FIELDS_OPS_RADIATION(E) E(RADIATION, _iotdata_field_def_radiation)
#else
#define IOTDATA_FIELDS_DEF_RADIATION(F)
#define IOTDATA_FIELDS_OPS_RADIATION(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_RADIATION(S)
#define IOTDATA_FIELDS_ERR_STRINGS_RADIATION(D)
#define IOTDATA_FIELDS_DAT_RADIATION IOTDATA_FIELDS_DAT_RADIATION_CPM IOTDATA_FIELDS_DAT_RADIATION_DOSE

/* ---------------------------------------------------------------------------
 * Field DEPTH
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_DEPTH)
#define IOTDATA_FIELDS_DAT_DEPTH uint16_t depth;
#define IOTDATA_DEPTH_MAX        1023
#define IOTDATA_DEPTH_BITS       10
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_depth(struct iotdata_encoder_t_ *enc, uint16_t depth_cm);
#endif
#define IOTDATA_FIELDS_DEF_DEPTH(F)         F(DEPTH)
#define IOTDATA_FIELDS_OPS_DEPTH(E)         E(DEPTH, _iotdata_field_def_depth)
#define IOTDATA_FIELDS_ERR_IDENTS_DEPTH(S)  S(IOTDATA_ERR_DEPTH_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_DEPTH(D) D(IOTDATA_ERR_DEPTH_HIGH, "Depth above 1023 cm")
#else
#define IOTDATA_FIELDS_DAT_DEPTH
#define IOTDATA_FIELDS_DEF_DEPTH(F)
#define IOTDATA_FIELDS_OPS_DEPTH(E)
#define IOTDATA_FIELDS_ERR_IDENTS_DEPTH(S)
#define IOTDATA_FIELDS_ERR_STRINGS_DEPTH(D)
#endif

/* ---------------------------------------------------------------------------
 * Field POSITION
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_POSITION)
#define IOTDATA_POS_LAT_OFFSET   90.0
#define IOTDATA_POS_LAT_RANGE    180.0
#define IOTDATA_POS_LON_OFFSET   180.0
#define IOTDATA_POS_LON_RANGE    360.0
#define IOTDATA_POS_LAT_OFFSET_I 900000000LL
#define IOTDATA_POS_LAT_RANGE_I  1800000000LL
#define IOTDATA_POS_LON_OFFSET_I 1800000000LL
#define IOTDATA_POS_LON_RANGE_I  3600000000LL
#define IOTDATA_FIELDS_DAT_POSITION \
    iotdata_double_t position_lat; \
    iotdata_double_t position_lon;
#if !defined(IOTDATA_NO_FLOATING)
#define IOTDATA_POS_LAT_LOW  (-90.0)
#define IOTDATA_POS_LAT_HIGH (90.0)
#define IOTDATA_POS_LON_LOW  (-180.0)
#define IOTDATA_POS_LON_HIGH (180.0)
#else
#define IOTDATA_POS_LAT_LOW  (-900000000)
#define IOTDATA_POS_LAT_HIGH (900000000)
#define IOTDATA_POS_LON_LOW  (-1800000000)
#define IOTDATA_POS_LON_HIGH (1800000000)
#endif
#define IOTDATA_POS_LAT_BITS 24
#define IOTDATA_POS_LON_BITS 24
#define IOTDATA_POS_SCALE    16777215 /* (1 << 24) - 1 */
#define IOTDATA_POS_BITS     (IOTDATA_POS_LAT_BITS + IOTDATA_POS_LON_BITS)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_position(struct iotdata_encoder_t_ *enc, iotdata_double_t latitude, iotdata_double_t longitude);
#endif
#define IOTDATA_FIELDS_DEF_POSITION(F) F(POSITION)
#define IOTDATA_FIELDS_OPS_POSITION(E) E(POSITION, _iotdata_field_def_position)
#define IOTDATA_FIELDS_ERR_IDENTS_POSITION(S) \
    S(IOTDATA_ERR_POSITION_LAT_LOW) \
    S(IOTDATA_ERR_POSITION_LAT_HIGH) \
    S(IOTDATA_ERR_POSITION_LON_LOW) \
    S(IOTDATA_ERR_POSITION_LON_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_POSITION(D) \
    D(IOTDATA_ERR_POSITION_LAT_LOW, "Latitude below -90") \
    D(IOTDATA_ERR_POSITION_LAT_HIGH, "Latitude above +90") \
    D(IOTDATA_ERR_POSITION_LON_LOW, "Longitude below -180") \
    D(IOTDATA_ERR_POSITION_LON_HIGH, "Longitude above +180")
#else
#define IOTDATA_FIELDS_DAT_POSITION
#define IOTDATA_FIELDS_DEF_POSITION(F)
#define IOTDATA_FIELDS_OPS_POSITION(E)
#define IOTDATA_FIELDS_ERR_IDENTS_POSITION(S)
#define IOTDATA_FIELDS_ERR_STRINGS_POSITION(D)
#endif

/* ---------------------------------------------------------------------------
 * Field DATETIME
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_DATETIME)
#define IOTDATA_FIELDS_DAT_DATETIME uint32_t datetime_secs;
#define IOTDATA_DATETIME_BITS       24
#define IOTDATA_DATETIME_RES        5
#define IOTDATA_DATETIME_MAX        ((1 << IOTDATA_DATETIME_BITS) - 1)
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_datetime(struct iotdata_encoder_t_ *enc, uint32_t seconds_from_year_start);
#endif
#define IOTDATA_FIELDS_DEF_DATETIME(F)         F(DATETIME)
#define IOTDATA_FIELDS_OPS_DATETIME(E)         E(DATETIME, _iotdata_field_def_datetime)
#define IOTDATA_FIELDS_ERR_IDENTS_DATETIME(S)  S(IOTDATA_ERR_DATETIME_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_DATETIME(D) D(IOTDATA_ERR_DATETIME_HIGH, "Datetime ticks above maximum")
#else
#define IOTDATA_FIELDS_DAT_DATETIME
#define IOTDATA_FIELDS_DEF_DATETIME(F)
#define IOTDATA_FIELDS_OPS_DATETIME(E)
#define IOTDATA_FIELDS_ERR_IDENTS_DATETIME(S)
#define IOTDATA_FIELDS_ERR_STRINGS_DATETIME(D)
#endif

static inline uint32_t iotdata_datetime_convert_utc_to_seconds_from_year_start(int64_t utc) {
    if (utc <= 0)
        return 0;
    // Days and seconds since Unix epoch
    int d = (int)(utc / 86400), rem = (int)(utc % 86400);
    // Leap days from 1970 to approximate year (every 4y, minus 100y, plus 400y)
    // Shift to March-based year so leap day falls at end, then shift back
    // Using: days since epoch -> civil date (Howard Hinnant algorithm, simplified)
    int y = (int)((10000LL * (long long)d + 14780) / 3652425) + 1970;
    // Day-of-year for Jan 1 of year y (relative to epoch)
    int j = y * 365 + y / 4 - y / 100 + y / 400 - 719527;
    if (j > d) {
        y--;
        j = y * 365 + y / 4 - y / 100 + y / 400 - 719527;
    }
    return (uint32_t)((d - j) * 86400 + rem); // 0-based day of year
}

/* ---------------------------------------------------------------------------
 * Field IMAGE
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_IMAGE)
#define IOTDATA_IMAGE_DATA_MAX         254 /* max pixel data after control byte */
/* Control byte: format(2) | size(2) | compression(2) | flags(2) */
#define IOTDATA_IMAGE_FMT_BILEVEL      0 /* 1 bpp */
#define IOTDATA_IMAGE_FMT_GREY4        1 /* 2 bpp */
#define IOTDATA_IMAGE_FMT_GREY16       2 /* 4 bpp */
#define IOTDATA_IMAGE_SIZE_24x18       0
#define IOTDATA_IMAGE_SIZE_32x24       1
#define IOTDATA_IMAGE_SIZE_48x36       2
#define IOTDATA_IMAGE_SIZE_64x48       3
#define IOTDATA_IMAGE_COMP_RAW         0
#define IOTDATA_IMAGE_COMP_RLE         1
#define IOTDATA_IMAGE_COMP_HEATSHRINK  2
#define IOTDATA_IMAGE_FLAG_FRAGMENT    (1U << 1)
#define IOTDATA_IMAGE_FLAG_INVERT      (1U << 0)
/* Heatshrink fixed parameters */
#define IOTDATA_IMAGE_HS_WINDOW_SZ2    8 /* 256-byte window */
#define IOTDATA_IMAGE_HS_LOOKAHEAD_SZ2 4 /* 16-byte lookahead */
#if !defined(IOTDATA_NO_ENCODE)
#define IOTDATA_FIELDS_DAT_IMAGE_ENCODE \
    uint8_t image_pixel_format; \
    uint8_t image_size_tier; \
    uint8_t image_compression; \
    uint8_t image_flags; \
    const uint8_t *image_data; \
    uint8_t image_data_len;
typedef struct {
    union {
        uint8_t data[IOTDATA_IMAGE_DATA_MAX];
    };
} iotdata_encode_to_json_scratch_image_t;
size_t iotdata_image_pixel_count(uint8_t size_tier);
uint8_t iotdata_image_bpp(uint8_t pixel_format);
size_t iotdata_image_bytes(uint8_t pixel_format, uint8_t size_tier);
/* RLE: returns output bytes written, 0 on error */
size_t iotdata_image_rle_compress(const uint8_t *pixels, size_t pixel_count, uint8_t bpp, uint8_t *out, size_t out_max);
size_t iotdata_image_rle_decompress(const uint8_t *compressed, size_t comp_len, uint8_t bpp, uint8_t *pixels, size_t pixel_buf_bytes);
/* Heatshrink LZSS (w=8, l=4): returns output bytes written, 0 on error */
size_t iotdata_image_hs_compress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max);
size_t iotdata_image_hs_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max);
#else
#define IOTDATA_FIELDS_DAT_IMAGE_ENCODE
#endif
#if !defined(IOTDATA_NO_DECODE)
#define IOTDATA_FIELDS_DAT_IMAGE_DECODE \
    uint8_t image_pixel_format; \
    uint8_t image_size_tier; \
    uint8_t image_compression; \
    uint8_t image_flags; \
    uint8_t image_data[IOTDATA_IMAGE_DATA_MAX]; \
    uint8_t image_data_len;
typedef struct {
    union {
        char b64[((IOTDATA_IMAGE_DATA_MAX + 2) / 3) * 4 + 1];
    };
} iotdata_decode_to_json_scratch_image_t;
#else
#define IOTDATA_FIELDS_DAT_IMAGE_DECODE
#endif
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_image(struct iotdata_encoder_t_ *enc, uint8_t pixel_format, uint8_t size_tier, uint8_t compression, uint8_t flags, const uint8_t *data, uint8_t data_len);
#endif
#define IOTDATA_FIELDS_DEF_IMAGE(F) F(IMAGE)
#define IOTDATA_FIELDS_OPS_IMAGE(E) E(IMAGE, _iotdata_field_def_image)
#define IOTDATA_FIELDS_ERR_IDENTS_IMAGE(S) \
    S(IOTDATA_ERR_IMAGE_FORMAT_HIGH) \
    S(IOTDATA_ERR_IMAGE_SIZE_HIGH) \
    S(IOTDATA_ERR_IMAGE_COMPRESSION_HIGH) \
    S(IOTDATA_ERR_IMAGE_DATA_NULL) \
    S(IOTDATA_ERR_IMAGE_DATA_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_IMAGE(D) \
    D(IOTDATA_ERR_IMAGE_FORMAT_HIGH, "Image pixel format above 2") \
    D(IOTDATA_ERR_IMAGE_SIZE_HIGH, "Image size tier above 3") \
    D(IOTDATA_ERR_IMAGE_COMPRESSION_HIGH, "Image compression above 2") \
    D(IOTDATA_ERR_IMAGE_DATA_NULL, "Image data pointer is NULL") \
    D(IOTDATA_ERR_IMAGE_DATA_HIGH, "Image data exceeds 254 bytes")
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE) && !defined(_IOTDATA_NEED_BASE64_DECODE)
#define _IOTDATA_NEED_BASE64_DECODE
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE) && !defined(_IOTDATA_NEED_BASE64_ENCODE)
#define _IOTDATA_NEED_BASE64_ENCODE
#endif
#if !defined(IOTDATA_NO_ENCODE)
#define IOTDATA_IMAGE_ENCODE_FROM_JSON_SCRATCH iotdata_encode_to_json_scratch_image_t image;
#else
#define IOTDATA_IMAGE_ENCODE_FROM_JSON_SCRATCH
#endif
#if !defined(IOTDATA_NO_DECODE)
#define IOTDATA_IMAGE_DECODE_TO_JSON_SCRATCH iotdata_decode_to_json_scratch_image_t image;
#else
#define IOTDATA_IMAGE_DECODE_TO_JSON_SCRATCH
#endif
#else
#define IOTDATA_FIELDS_DAT_IMAGE_ENCODE
#define IOTDATA_FIELDS_DAT_IMAGE_DECODE
#define IOTDATA_FIELDS_DEF_IMAGE(F)
#define IOTDATA_FIELDS_OPS_IMAGE(E)
#define IOTDATA_FIELDS_ERR_IDENTS_IMAGE(S)
#define IOTDATA_FIELDS_ERR_STRINGS_IMAGE(D)
#define IOTDATA_IMAGE_ENCODE_FROM_JSON_SCRATCH
#define IOTDATA_IMAGE_DECODE_TO_JSON_SCRATCH
#endif

/* ---------------------------------------------------------------------------
 * Field FLAGS
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_FLAGS)
#define IOTDATA_FIELDS_DAT_FLAGS uint8_t flags;
#define IOTDATA_FLAGS_BITS       8
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_flags(struct iotdata_encoder_t_ *enc, uint8_t flags);
#endif
#define IOTDATA_FIELDS_DEF_FLAGS(F) F(FLAGS)
#define IOTDATA_FIELDS_OPS_FLAGS(E) E(FLAGS, _iotdata_field_def_flags)
#else
#define IOTDATA_FIELDS_DAT_FLAGS
#define IOTDATA_FIELDS_DEF_FLAGS(F)
#define IOTDATA_FIELDS_OPS_FLAGS(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_FLAGS(S)
#define IOTDATA_FIELDS_ERR_STRINGS_FLAGS(D)

/* ---------------------------------------------------------------------------
 * Field BITS32
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_BITS32)
#define IOTDATA_FIELDS_DAT_BITS32 uint32_t bits32;
#define IOTDATA_BITS32_BITS       32
#if !defined(IOTDATA_NO_ENCODE)
enum iotdata_status_t_ iotdata_encode_bits32(struct iotdata_encoder_t_ *enc, uint32_t bits32);
#endif
#define IOTDATA_FIELDS_DEF_BITS32(F) F(BITS32)
#define IOTDATA_FIELDS_OPS_BITS32(E) E(BITS32, _iotdata_field_def_bits32)
#else
#define IOTDATA_FIELDS_DAT_BITS32
#define IOTDATA_FIELDS_DEF_BITS32(F)
#define IOTDATA_FIELDS_OPS_BITS32(E)
#endif
#define IOTDATA_FIELDS_ERR_IDENTS_BITS32(S)
#define IOTDATA_FIELDS_ERR_STRINGS_BITS32(D)

/* ---------------------------------------------------------------------------
 * Field TLV
 * -------------------------------------------------------------------------*/

#if defined(IOTDATA_ENABLE_TLV)
#define IOTDATA_TLV_MAX         8
#define IOTDATA_TLV_DATA_MAX    255
#define IOTDATA_TLV_STR_LEN_MAX 255
#if !defined(IOTDATA_NO_ENCODE)
typedef struct {
    uint8_t format;
    uint8_t type;
    uint8_t length;
    const uint8_t *data;
    const char *str;
} iotdata_encoder_tlv_t;
typedef struct {
    union {
        uint8_t raw[IOTDATA_TLV_MAX][IOTDATA_TLV_DATA_MAX];
        char str[IOTDATA_TLV_MAX][IOTDATA_TLV_STR_LEN_MAX + 1];
    };
} iotdata_encode_from_json_scratch_tlv_t;
#define IOTDATA_FIELDS_DAT_TLV_ENCODE \
    uint8_t tlv_count; \
    iotdata_encoder_tlv_t tlv[IOTDATA_TLV_MAX];
enum iotdata_status_t_ iotdata_encode_tlv(struct iotdata_encoder_t_ *enc, uint8_t type, const uint8_t *data, uint8_t length);
enum iotdata_status_t_ iotdata_encode_tlv_string(struct iotdata_encoder_t_ *enc, uint8_t type, const char *str);
#else
#define IOTDATA_FIELDS_DAT_TLV_ENCODE
#endif
#if !defined(IOTDATA_NO_DECODE)
typedef struct {
    uint8_t format;
    uint8_t type;
    uint8_t length;
    union {
        uint8_t raw[IOTDATA_TLV_DATA_MAX];
        char str[IOTDATA_TLV_STR_LEN_MAX + 1];
    };
} iotdata_decoder_tlv_t;
typedef struct {
    union {
        char b64[((IOTDATA_TLV_DATA_MAX + 2) / 3) * 4 + 1];
        char str[IOTDATA_TLV_STR_LEN_MAX + 1];
    };
} iotdata_decode_to_json_scratch_tlv_t;
#define IOTDATA_FIELDS_DAT_TLV_DECODE \
    uint8_t tlv_count; \
    iotdata_decoder_tlv_t tlv[IOTDATA_TLV_MAX];
#else
#define IOTDATA_FIELDS_DAT_TLV_DECODE
#endif
#define IOTDATA_TLV_FMT_BITS    1
#define IOTDATA_TLV_TYPE_BITS   6
#define IOTDATA_TLV_TYPE_MAX    63
#define IOTDATA_TLV_MORE_BITS   1
#define IOTDATA_TLV_LENGTH_BITS 8
#define IOTDATA_TLV_HEADER_BITS (IOTDATA_TLV_FMT_BITS + IOTDATA_TLV_TYPE_BITS + IOTDATA_TLV_MORE_BITS + IOTDATA_TLV_LENGTH_BITS)
#define IOTDATA_TLV_FMT_RAW     0
#define IOTDATA_TLV_FMT_STRING  1
#define IOTDATA_TLV_CHAR_BITS   6
static inline bool iotdata_issixbit(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == ' ');
}
#if !defined(IOTDATA_NO_ENCODE)
#define IOTDATA_TLV_ENCODE_FROM_JSON_SCRATCH iotdata_encode_from_json_scratch_tlv_t tlv;
#else
#define IOTDATA_TLV_ENCODE_FROM_JSON_SCRATCH
#endif
#if !defined(IOTDATA_NO_DECODE)
#define IOTDATA_TLV_DECODE_TO_JSON_SCRATCH iotdata_decode_to_json_scratch_tlv_t tlv;
#else
#define IOTDATA_TLV_DECODE_TO_JSON_SCRATCH
#endif
#define IOTDATA_FIELDS_DEF_TLV(F) F(TLV, 31)
#define IOTDATA_FIELDS_ERR_IDENTS_TLV(S) \
    S(IOTDATA_ERR_TLV_TYPE_HIGH) \
    S(IOTDATA_ERR_TLV_DATA_NULL) \
    S(IOTDATA_ERR_TLV_LEN_HIGH) \
    S(IOTDATA_ERR_TLV_FULL) \
    S(IOTDATA_ERR_TLV_STR_NULL) \
    S(IOTDATA_ERR_TLV_STR_LEN_HIGH) \
    S(IOTDATA_ERR_TLV_STR_CHAR_INVALID) \
    S(IOTDATA_ERR_TLV_UNMATCHED) \
    S(IOTDATA_ERR_TLV_KV_MISMATCH) \
    S(IOTDATA_ERR_TLV_VALUE_HIGH)
#define IOTDATA_FIELDS_ERR_STRINGS_TLV(D) \
    D(IOTDATA_ERR_TLV_TYPE_HIGH, "TLV type above maximum (63)") \
    D(IOTDATA_ERR_TLV_DATA_NULL, "TLV data pointer is NULL") \
    D(IOTDATA_ERR_TLV_LEN_HIGH, "TLV length above maximum (255)") \
    D(IOTDATA_ERR_TLV_FULL, "TLV fields exhausted (max 8)") \
    D(IOTDATA_ERR_TLV_STR_NULL, "TLV string pointer is NULL") \
    D(IOTDATA_ERR_TLV_STR_LEN_HIGH, "TLV string too long (max 255 chars)") \
    D(IOTDATA_ERR_TLV_STR_CHAR_INVALID, "TLV string contains unencodable character") \
    D(IOTDATA_ERR_TLV_UNMATCHED, "TLV global type was unmatched") \
    D(IOTDATA_ERR_TLV_KV_MISMATCH, "TLV global key-value type missing one pair") \
    D(IOTDATA_ERR_TLV_VALUE_HIGH, "TLV value above range")
#define IOTDATA_FIELD_RESERVED(id) ((id) == IOTDATA_FIELD_TLV)
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_ENCODE) && !defined(_IOTDATA_NEED_BASE64_DECODE)
#define _IOTDATA_NEED_BASE64_DECODE
#endif
#if !defined(IOTDATA_NO_JSON) && !defined(IOTDATA_NO_DECODE) && !defined(_IOTDATA_NEED_BASE64_ENCODE)
#define _IOTDATA_NEED_BASE64_ENCODE
#endif
#if !defined(IOTDATA_NO_ENCODE)
#define _IOTDATA_NEED_SIXBIT_ENCODE
#endif
#if !defined(IOTDATA_NO_DECODE)
#define _IOTDATA_NEED_SIXBIT_DECODE
#endif
#else
#define IOTDATA_FIELDS_DAT_TLV_ENCODE
#define IOTDATA_FIELDS_DAT_TLV_DECODE
#define IOTDATA_FIELDS_DEF_TLV(F)
#define IOTDATA_FIELDS_ERR_IDENTS_TLV(S)
#define IOTDATA_FIELDS_ERR_STRINGS_TLV(D)
#define IOTDATA_FIELD_RESERVED(id) 0
#define IOTDATA_TLV_ENCODE_FROM_JSON_SCRATCH
#define IOTDATA_TLV_DECODE_TO_JSON_SCRATCH
#endif

#if defined(IOTDATA_ENABLE_TLV) && !defined(IOTDATA_NO_TLV_SPECIFIC)
#define IOTDATA_TLV_TYPE_GLOBAL_MAX  0x0F
#define IOTDATA_TLV_TYPE_QUALITY_MAX 0x1F

#define IOTDATA_TLV_VERSION          0x01
#define IOTDATA_TLV_STATUS           0x02
#define IOTDATA_TLV_STATUS_LENGTH    9
#define IOTDATA_TLV_STATUS_TICKS_RES 5
#define IOTDATA_TLV_STATUS_TICKS_MAX 0xFFFFFF
#define IOTDATA_TLV_HEALTH           0x03
#define IOTDATA_TLV_HEALTH_LENGTH    7
#define IOTDATA_TLV_HEALTH_TICKS_RES 5
#define IOTDATA_TLV_HEALTH_TICKS_MAX 0xFFFF
#define IOTDATA_TLV_CONFIG           0x04
#define IOTDATA_TLV_DIAGNOSTIC       0x05
#define IOTDATA_TLV_USERDATA         0x06

#define IOTDATA_TLV_REASON_UNKNOWN   0x00
#define IOTDATA_TLV_REASON_POWER_ON  0x01
#define IOTDATA_TLV_REASON_SOFTWARE  0x02
#define IOTDATA_TLV_REASON_WATCHDOG  0x03
#define IOTDATA_TLV_REASON_BROWNOUT  0x04
#define IOTDATA_TLV_REASON_PANIC     0x05
#define IOTDATA_TLV_REASON_DEEPSLEEP 0x06
#define IOTDATA_TLV_REASON_EXTERNAL  0x07
#define IOTDATA_TLV_REASON_OTA       0x08

#define IOTDATA_TLV_REASON_NA        0xFF
#define IOTDATA_TLV_HEALTH_TEMP_NA   127
#define IOTDATA_TLV_HEALTH_HEAP_NA   0xFFFF

#if !defined(IOTDATA_NO_ENCODE)
/* 0x01 VERSION — key-value pairs, space-delimited on wire: kv[0]="FW", kv[1]="142", kv[2]="HW", kv[3]="3", ... count must be even (every key has a value). */
enum iotdata_status_t_ iotdata_encode_tlv_type_version(struct iotdata_encoder_t_ *enc, const char *const *kv, size_t count, bool raw, char *buf, size_t buf_size);
/* 0x02 STATUS — 9 bytes raw format: uptimes in seconds (converted to 5-second ticks internally), pass lifetime_uptime_secs=0 for "not tracked". */
enum iotdata_status_t_ iotdata_encode_tlv_type_status(struct iotdata_encoder_t_ *enc, uint32_t session_uptime_secs, uint32_t lifetime_uptime_secs, uint16_t restarts, uint8_t reason, uint8_t buf[9]);
/* 0x03 HEALTH — 7 bytes raw format: session_active_secs converted to 5-second ticks internally, pass cpu_temp=127 for "not available", free_heap=0xFFFF for "not tracked". */
enum iotdata_status_t_ iotdata_encode_tlv_type_health(struct iotdata_encoder_t_ *enc, int8_t cpu_temp, uint16_t supply_mv, uint16_t free_heap, uint32_t session_active_secs, uint8_t buf[7]);
/* 0x04 CONFIG — string format, space-delimited key-value pairs */
enum iotdata_status_t_ iotdata_encode_tlv_type_config(struct iotdata_encoder_t_ *enc, const char *const *kv, size_t count, bool raw, char *buf, size_t buf_size);
/* 0x05 DIAGNOSTIC — string format, free-form message */
enum iotdata_status_t_ iotdata_encode_tlv_type_diagnostic(struct iotdata_encoder_t_ *enc, const char *str, bool raw);
/* 0x06 USERDATA — string format, free-form event */
enum iotdata_status_t_ iotdata_encode_tlv_type_userdata(struct iotdata_encoder_t_ *enc, const char *str, bool raw);
#endif
#endif

/* ---------------------------------------------------------------------------
 * Master
 * -------------------------------------------------------------------------*/

#define IOTDATA_FIELDS_SELECTED(F) \
    IOTDATA_FIELDS_DEF_BATTERY(F) \
    IOTDATA_FIELDS_DEF_LINK(F) \
    IOTDATA_FIELDS_DEF_TEMPERATURE(F) \
    IOTDATA_FIELDS_DEF_PRESSURE(F) \
    IOTDATA_FIELDS_DEF_HUMIDITY(F) \
    IOTDATA_FIELDS_DEF_ENVIRONMENT(F) \
    IOTDATA_FIELDS_DEF_WIND(F) \
    IOTDATA_FIELDS_DEF_WIND_SPEED(F) \
    IOTDATA_FIELDS_DEF_WIND_DIRECTION(F) \
    IOTDATA_FIELDS_DEF_WIND_GUST(F) \
    IOTDATA_FIELDS_DEF_RAIN(F) \
    IOTDATA_FIELDS_DEF_RAIN_RATE(F) \
    IOTDATA_FIELDS_DEF_RAIN_SIZE(F) \
    IOTDATA_FIELDS_DEF_SOLAR(F) \
    IOTDATA_FIELDS_DEF_CLOUDS(F) \
    IOTDATA_FIELDS_DEF_AIR_QUALITY(F) \
    IOTDATA_FIELDS_DEF_AIR_QUALITY_INDEX(F) \
    IOTDATA_FIELDS_DEF_AIR_QUALITY_PM(F) \
    IOTDATA_FIELDS_DEF_AIR_QUALITY_GAS(F) \
    IOTDATA_FIELDS_DEF_RADIATION(F) \
    IOTDATA_FIELDS_DEF_RADIATION_CPM(F) \
    IOTDATA_FIELDS_DEF_RADIATION_DOSE(F) \
    IOTDATA_FIELDS_DEF_DEPTH(F) \
    IOTDATA_FIELDS_DEF_POSITION(F) \
    IOTDATA_FIELDS_DEF_DATETIME(F) \
    IOTDATA_FIELDS_DEF_IMAGE(F) \
    IOTDATA_FIELDS_DEF_FLAGS(F) \
    IOTDATA_FIELDS_DEF_BITS32(F)

#define IOTDATA_FIELDS_RESERVED(F) IOTDATA_FIELDS_DEF_TLV(F)

#define IOTDATA_FIELDS_DAT_ENCODER \
    IOTDATA_FIELDS_DAT_BATTERY \
    IOTDATA_FIELDS_DAT_LINK \
    IOTDATA_FIELDS_DAT_ENVIRONMENT \
    IOTDATA_FIELDS_DAT_WIND \
    IOTDATA_FIELDS_DAT_RAIN \
    IOTDATA_FIELDS_DAT_SOLAR \
    IOTDATA_FIELDS_DAT_CLOUDS \
    IOTDATA_FIELDS_DAT_AIR_QUALITY \
    IOTDATA_FIELDS_DAT_RADIATION \
    IOTDATA_FIELDS_DAT_DEPTH \
    IOTDATA_FIELDS_DAT_POSITION \
    IOTDATA_FIELDS_DAT_DATETIME \
    IOTDATA_FIELDS_DAT_IMAGE_ENCODE \
    IOTDATA_FIELDS_DAT_FLAGS \
    IOTDATA_FIELDS_DAT_BITS32 \
    IOTDATA_FIELDS_DAT_TLV_ENCODE

#define IOTDATA_FIELDS_DAT_DECODER \
    IOTDATA_FIELDS_DAT_BATTERY \
    IOTDATA_FIELDS_DAT_LINK \
    IOTDATA_FIELDS_DAT_ENVIRONMENT \
    IOTDATA_FIELDS_DAT_WIND \
    IOTDATA_FIELDS_DAT_RAIN \
    IOTDATA_FIELDS_DAT_SOLAR \
    IOTDATA_FIELDS_DAT_CLOUDS \
    IOTDATA_FIELDS_DAT_AIR_QUALITY \
    IOTDATA_FIELDS_DAT_RADIATION \
    IOTDATA_FIELDS_DAT_DEPTH \
    IOTDATA_FIELDS_DAT_POSITION \
    IOTDATA_FIELDS_DAT_DATETIME \
    IOTDATA_FIELDS_DAT_IMAGE_DECODE \
    IOTDATA_FIELDS_DAT_FLAGS \
    IOTDATA_FIELDS_DAT_BITS32 \
    IOTDATA_FIELDS_DAT_TLV_DECODE

#define IOTDATA_FIELDS_OPS(E) \
    IOTDATA_FIELDS_OPS_BATTERY(E) \
    IOTDATA_FIELDS_OPS_LINK(E) \
    IOTDATA_FIELDS_OPS_TEMPERATURE(E) \
    IOTDATA_FIELDS_OPS_PRESSURE(E) \
    IOTDATA_FIELDS_OPS_HUMIDITY(E) \
    IOTDATA_FIELDS_OPS_ENVIRONMENT(E) \
    IOTDATA_FIELDS_OPS_WIND(E) \
    IOTDATA_FIELDS_OPS_WIND_SPEED(E) \
    IOTDATA_FIELDS_OPS_WIND_DIRECTION(E) \
    IOTDATA_FIELDS_OPS_WIND_GUST(E) \
    IOTDATA_FIELDS_OPS_RAIN(E) \
    IOTDATA_FIELDS_OPS_RAIN_RATE(E) \
    IOTDATA_FIELDS_OPS_RAIN_SIZE(E) \
    IOTDATA_FIELDS_OPS_SOLAR(E) \
    IOTDATA_FIELDS_OPS_CLOUDS(E) \
    IOTDATA_FIELDS_OPS_AIR_QUALITY(E) \
    IOTDATA_FIELDS_OPS_AIR_QUALITY_INDEX(E) \
    IOTDATA_FIELDS_OPS_AIR_QUALITY_PM(E) \
    IOTDATA_FIELDS_OPS_AIR_QUALITY_GAS(E) \
    IOTDATA_FIELDS_OPS_RADIATION(E) \
    IOTDATA_FIELDS_OPS_RADIATION_CPM(E) \
    IOTDATA_FIELDS_OPS_RADIATION_DOSE(E) \
    IOTDATA_FIELDS_OPS_DEPTH(E) \
    IOTDATA_FIELDS_OPS_POSITION(E) \
    IOTDATA_FIELDS_OPS_DATETIME(E) \
    IOTDATA_FIELDS_OPS_IMAGE(E) \
    IOTDATA_FIELDS_OPS_FLAGS(E) \
    IOTDATA_FIELDS_OPS_BITS32(E)

#define IOTDATA_FIELDS_ERR_IDENTS(S) \
    IOTDATA_FIELDS_ERR_IDENTS_TLV(S) \
    IOTDATA_FIELDS_ERR_IDENTS_BATTERY(S) \
    IOTDATA_FIELDS_ERR_IDENTS_LINK(S) \
    IOTDATA_FIELDS_ERR_IDENTS_TEMPERATURE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_PRESSURE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_HUMIDITY(S) \
    IOTDATA_FIELDS_ERR_IDENTS_ENVIRONMENT(S) \
    IOTDATA_FIELDS_ERR_IDENTS_WIND(S) \
    IOTDATA_FIELDS_ERR_IDENTS_WIND_SPEED(S) \
    IOTDATA_FIELDS_ERR_IDENTS_WIND_DIRECTION(S) \
    IOTDATA_FIELDS_ERR_IDENTS_WIND_GUST(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RAIN(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RAIN_RATE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RAIN_SIZE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_SOLAR(S) \
    IOTDATA_FIELDS_ERR_IDENTS_CLOUDS(S) \
    IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY(S) \
    IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_INDEX(S) \
    IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_PM(S) \
    IOTDATA_FIELDS_ERR_IDENTS_AIR_QUALITY_GAS(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RADIATION(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RADIATION_CPM(S) \
    IOTDATA_FIELDS_ERR_IDENTS_RADIATION_DOSE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_DEPTH(S) \
    IOTDATA_FIELDS_ERR_IDENTS_POSITION(S) \
    IOTDATA_FIELDS_ERR_IDENTS_DATETIME(S) \
    IOTDATA_FIELDS_ERR_IDENTS_IMAGE(S) \
    IOTDATA_FIELDS_ERR_IDENTS_FLAGS(S) \
    IOTDATA_FIELDS_ERR_IDENTS_BITS32(S)

#define IOTDATA_FIELDS_ERR_STRINGS(D) \
    IOTDATA_FIELDS_ERR_STRINGS_TLV(D) \
    IOTDATA_FIELDS_ERR_STRINGS_BATTERY(D) \
    IOTDATA_FIELDS_ERR_STRINGS_LINK(D) \
    IOTDATA_FIELDS_ERR_STRINGS_TEMPERATURE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_PRESSURE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_HUMIDITY(D) \
    IOTDATA_FIELDS_ERR_STRINGS_ENVIRONMENT(D) \
    IOTDATA_FIELDS_ERR_STRINGS_WIND(D) \
    IOTDATA_FIELDS_ERR_STRINGS_WIND_SPEED(D) \
    IOTDATA_FIELDS_ERR_STRINGS_WIND_DIRECTION(D) \
    IOTDATA_FIELDS_ERR_STRINGS_WIND_GUST(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RAIN(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RAIN_RATE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RAIN_SIZE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_SOLAR(D) \
    IOTDATA_FIELDS_ERR_STRINGS_CLOUDS(D) \
    IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY(D) \
    IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_INDEX(D) \
    IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_PM(D) \
    IOTDATA_FIELDS_ERR_STRINGS_AIR_QUALITY_GAS(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RADIATION(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RADIATION_CPM(D) \
    IOTDATA_FIELDS_ERR_STRINGS_RADIATION_DOSE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_DEPTH(D) \
    IOTDATA_FIELDS_ERR_STRINGS_POSITION(D) \
    IOTDATA_FIELDS_ERR_STRINGS_DATETIME(D) \
    IOTDATA_FIELDS_ERR_STRINGS_IMAGE(D) \
    IOTDATA_FIELDS_ERR_STRINGS_FLAGS(D) \
    IOTDATA_FIELDS_ERR_STRINGS_BITS32(D)

#define IOTDATA_FIELDS_ENCODE_FROM_JSON_SCRATCH \
    IOTDATA_IMAGE_ENCODE_FROM_JSON_SCRATCH \
    IOTDATA_TLV_ENCODE_FROM_JSON_SCRATCH

#define IOTDATA_FIELDS_DECODE_TO_JSON_SCRATCH \
    IOTDATA_IMAGE_DECODE_TO_JSON_SCRATCH \
    IOTDATA_TLV_DECODE_TO_JSON_SCRATCH

/* ---------------------------------------------------------------------------
 * End
 * -------------------------------------------------------------------------*/
