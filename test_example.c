/*
 * test_example.c — Weather station simulator
 *
 * Generates random-walk weather data using the built-in weather station
 * variant map.  Every 30 seconds it encodes a packet and displays:
 *   1. The sensor values before encoding
 *   2. A hex dump of the encoded binary
 *   3. The iotdata diagnostic dump
 *
 * Every 5 minutes (10th packet), position and datetime are also included,
 * triggering the extension byte.
 *
 * Runs until terminated with Ctrl-C.
 *
 * Compile:
 *   cc -DIOTDATA_VARIANT_MAPS_DEFAULT test_example.c iotdata.c -lm -lcjson -o test_example
 */

#include "iotdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Signal handling
 * -------------------------------------------------------------------------*/

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

/* ---------------------------------------------------------------------------
 * Random walk helpers
 * -------------------------------------------------------------------------*/

/* Uniform random in [lo, hi] */
static double rand_range(double lo, double hi) {
    return lo + (hi - lo) * ((double)rand() / (double)RAND_MAX);
}

/* Drift a value by up to ±step, clamped to [lo, hi] */
static double drift(double val, double step, double lo, double hi) {
    val += rand_range(-step, step);
    if (val < lo)
        val = lo;
    if (val > hi)
        val = hi;
    return val;
}

/* Drift an integer value */
static int drifti(int val, int step, int lo, int hi) {
    val += (int)rand_range((double)-step, (double)(step + 1));
    if (val < lo)
        val = lo;
    if (val > hi)
        val = hi;
    return val;
}

/* Wrap an angle to [0, 359] */
static int wrap_deg(int deg) {
    deg %= 360;
    if (deg < 0)
        deg += 360;
    return deg;
}

/* ---------------------------------------------------------------------------
 * Simulated sensor state
 * -------------------------------------------------------------------------*/

typedef struct {
    /* pres0 fields */
    double battery; /* 0-100 % */
    bool charging;
    double link_snr;    /* -20 to +10 dB */
    int link_rssi;      /* -120 to -60 dBm */
    double temperature; /* -40 to +80 °C */
    int pressure;       /* 850-1105 hPa */
    int humidity;       /* 0-100 % */
    double wind_speed;  /* 0-63.5 m/s */
    int wind_dir;       /* 0-359 degrees */
    double wind_gust;   /* 0-63.5 m/s */
    int rain_rate;      /* 0-255 mm/hr */
    double rain_size;   /* 0-6.3 mm/d */
    int solar_irrad;    /* 0-1023 W/m² */
    int solar_uv;       /* 0-15 */

    /* pres1 fields (only sent every 5 min) */
    int clouds;      /* 0-8 okta */
    int air_quality; /* 0-500 AQI */
    int rad_cpm;     /* 0-16383 CPM */
    double rad_dose; /* 0-163.83 µSv/h */
    double pos_lat;
    double pos_lon;
    uint8_t flags;

    /* bookkeeping */
    uint16_t sequence;
} sensor_state_t;

static void state_init(sensor_state_t *s) {
    s->battery = 85.0;
    s->charging = false;
    s->link_rssi = -85;
    s->link_snr = 5.0;
    s->temperature = 15.0;
    s->pressure = 1013;
    s->humidity = 55;
    s->wind_speed = 5.0;
    s->wind_dir = 180;
    s->wind_gust = 8.0;
    s->rain_rate = 2;
    s->rain_size = 0.5;
    s->solar_irrad = 400;
    s->solar_uv = 5;

    s->clouds = 4;
    s->air_quality = 45;
    s->rad_cpm = 20;
    s->rad_dose = 0.12;
    s->pos_lat = 59.334591; /* Stockholm */
    s->pos_lon = 18.063240;
    s->flags = 0x00;

    s->sequence = 0;
}

static void state_step(sensor_state_t *s) {
    /* Battery drains slowly, occasionally charges */
    s->battery = drift(s->battery, 0.3, 0.0, 100.0);
    if (s->battery < 20.0)
        s->charging = true;
    else if (s->battery > 90.0)
        s->charging = false;

    /* Link quality */
    s->link_rssi = drifti(s->link_rssi, 2, -120, -60);
    s->link_snr = drift(s->link_snr, 1.0, -20.0, 10.0);

    /* Atmospheric */
    s->temperature = drift(s->temperature, 0.5, -40.0, 80.0);
    s->pressure = drifti(s->pressure, 1, 850, 1105);
    s->humidity = drifti(s->humidity, 2, 0, 100);

    /* Wind */
    s->wind_speed = drift(s->wind_speed, 1.0, 0.0, 63.0);
    s->wind_dir = wrap_deg(s->wind_dir + (int)rand_range(-15.0, 16.0));
    s->wind_gust = s->wind_speed + rand_range(0.0, 10.0);
    if (s->wind_gust > 63.5)
        s->wind_gust = 63.5;

    /* Rain: mostly 0, occasionally rains */
    if (s->rain_rate == 0) {
        if (rand_range(0, 1) < 0.05) {
            s->rain_rate = drifti(s->rain_rate, 5, 1, 20);
            s->rain_size = drift(s->rain_size, 1.0, 1.0, 60.0);
        }
    } else {
        s->rain_rate = drifti(s->rain_rate, 3, 0, 255);
        s->rain_size = drift(s->rain_size, 1.0, 0.0, 60.0);
    }

    /* Solar: correlated with a rough sine wave plus noise */
    s->solar_irrad = drifti(s->solar_irrad, 30, 0, 1023);
    s->solar_uv = s->solar_irrad / 100;
    if (s->solar_uv > 15)
        s->solar_uv = 15;

    /* Slow-changing pres1 fields */
    s->clouds = drifti(s->clouds, 1, 0, 8);
    s->air_quality = drifti(s->air_quality, 5, 0, 500);
    s->rad_cpm = drifti(s->rad_cpm, 3, 0, 16383);
    s->rad_dose = drift(s->rad_dose, 0.02, 0.0, 163.0);

    /* GPS drift */
    s->pos_lat += rand_range(-0.000005, 0.000005);
    s->pos_lon += rand_range(-0.000005, 0.000005);
    if (s->pos_lat < -90.0)
        s->pos_lat = -90.0;
    if (s->pos_lat > 90.0)
        s->pos_lat = 90.0;
    if (s->pos_lon < -180.0)
        s->pos_lon = -180.0;
    if (s->pos_lon > 180.0)
        s->pos_lon = 180.0;

    s->flags = (uint8_t)(s->flags ^ (uint8_t)(rand() & 0x01));

    s->sequence++;
}

/* ---------------------------------------------------------------------------
 * Seconds from start of current year (for datetime field)
 * -------------------------------------------------------------------------*/

static uint32_t seconds_from_year_start(void) {
    const time_t now = time(NULL);
    struct tm jan1 = *gmtime(&now);
    jan1.tm_mon = 0;
    jan1.tm_mday = 1;
    jan1.tm_hour = 0;
    jan1.tm_min = 0;
    jan1.tm_sec = 0;
    const double diff = difftime(now, mktime(&jan1));
    return (diff > 0) ? (uint32_t)diff : 0;
}

/* ---------------------------------------------------------------------------
 * Display helpers
 * -------------------------------------------------------------------------*/

static void print_separator(void) {
    printf("────────────────────────────────────────────────────────────────────────────────\n");
}

static void print_pre_encode(const sensor_state_t *s, int full) {
    printf("\n** Sensor values:\n\n");
    printf("    battery:     %5.1f%%  %s\n", s->battery, s->charging ? "(charging)" : "");
    printf("    link:        %5d dBm   SNR %.1f dB\n", s->link_rssi, s->link_snr);
    printf("    temperature: %+6.2f °C\n", s->temperature);
    printf("    pressure:    %5d hPa\n", s->pressure);
    printf("    humidity:    %5d %%\n", s->humidity);
    printf("    wind:        %5.1f m/s @ %03d°  (gust %.1f m/s)\n", s->wind_speed, s->wind_dir, s->wind_gust);
    printf("    rain:        %5d mm/hr, %.1f mm/d\n", s->rain_rate, s->rain_size);
    printf("    solar:       %5d W/m²  UV %d\n", s->solar_irrad, s->solar_uv);
    if (full) {
        printf("    clouds:      %5d okta\n", s->clouds);
        printf("    air quality: %5d AQI\n", s->air_quality);
        printf("    radation:    %5d CPM, %8.2f µSv/h\n", s->rad_cpm, s->rad_dose);
        printf("    position:    %.6f, %.6f\n", s->pos_lat, s->pos_lon);
        printf("    datetime:    %u s from year start\n", seconds_from_year_start());
        printf("    flags:       0x%02X\n", s->flags);
    }
}

static void print_hex_dump(const uint8_t *buf, size_t len) {
    printf("\n** Binary (%zu bytes):\n\n", len);
    if (len > 0)
        printf("    ");
    for (size_t i = 0; i < len; i++)
        printf("%02X %s", buf[i], ((i + 1) % 16 == 0 && i + 1 < len) ? "\n    " : "");
    printf("\n");
}

static void print_diagnostic_dump(const uint8_t *buf, size_t len) {
    iotdata_status_t rc;
    char dump_str[8192];
    if ((rc = iotdata_dump_to_string(buf, len, dump_str, sizeof(dump_str), true)) == IOTDATA_OK) {
        printf("\n** Diagnostic dump:\n\n%s", dump_str);
        if ((rc = iotdata_dump_to_string(buf, len, dump_str, sizeof(dump_str), false)) == IOTDATA_OK)
            printf("\n%s", dump_str);
    } else
        fprintf(stderr, "\n  dump: %s", iotdata_strerror(rc));
}

static void print_to_string(const uint8_t *buf, size_t len) {
    iotdata_status_t rc;
    char print_str[4096];
    if ((rc = iotdata_print_to_string(buf, len, print_str, sizeof(print_str))) == IOTDATA_OK)
        printf("\n** Decoded:\n\n%s", print_str);
    else
        fprintf(stderr, "\n  decoded: %s", iotdata_strerror(rc));
}

static void print_to_json(const uint8_t *buf, size_t len) {
    iotdata_status_t rc;
    char *json_out;
    if ((rc = iotdata_decode_to_json(buf, len, &json_out)) == IOTDATA_OK) {
        printf("\n** JSON:\n\n%s", json_out);
        free(json_out);
    } else
        fprintf(stderr, "\n  json: %s", iotdata_strerror(rc));
}

/* ---------------------------------------------------------------------------
 * Encode and display one packet
 * -------------------------------------------------------------------------*/

static void encode_and_display(sensor_state_t *s, int full) {
    uint8_t buf[256];
    size_t len;

    const time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));

    print_separator();
    printf("** Packet #%u  [%s]  %s\n", s->sequence, ts, full ? "*** 5-minute report (with position/datetime) ***" : "30-second report");
    print_separator();

    print_pre_encode(s, full);

    iotdata_status_t rc;
    iotdata_encoder_t enc;
    if ((rc = iotdata_encode_begin(&enc, buf, sizeof(buf), 0, 42, s->sequence)) != IOTDATA_OK) {
        fprintf(stderr, "\n  encode_begin: %s\n", iotdata_strerror(rc));
        return;
    }
    /* pres0: always */
    iotdata_encode_battery(&enc, (uint8_t)lround(s->battery), s->charging);
    iotdata_encode_link(&enc, (int16_t)s->link_rssi, (float)s->link_snr);
    iotdata_encode_environment(&enc, (float)s->temperature, (uint16_t)s->pressure, (uint8_t)s->humidity);
    iotdata_encode_wind(&enc, (float)s->wind_speed, (uint16_t)s->wind_dir, (float)s->wind_gust);
    iotdata_encode_rain(&enc, (uint8_t)s->rain_rate, (uint8_t)(s->rain_size * 10.0));
    iotdata_encode_solar(&enc, (uint16_t)s->solar_irrad, (uint8_t)s->solar_uv);
    /* pres1: every 5 minutes */
    if (full) {
        iotdata_encode_air_quality(&enc, (uint16_t)s->air_quality);
        iotdata_encode_clouds(&enc, (uint8_t)s->clouds);
        iotdata_encode_radiation(&enc, (uint16_t)s->rad_cpm, (float)s->rad_dose);
        iotdata_encode_position(&enc, s->pos_lat, s->pos_lon);
        iotdata_encode_datetime(&enc, seconds_from_year_start());
        iotdata_encode_flags(&enc, s->flags);
    }
    if ((rc = iotdata_encode_end(&enc, &len)) != IOTDATA_OK) {
        fprintf(stderr, "\n  encode_end: %s\n", iotdata_strerror(rc));
        return;
    }

    print_hex_dump(buf, len);
    print_diagnostic_dump(buf, len);
    print_to_string(buf, len);
    print_to_json(buf, len);
    printf("\n\n");
}

/* ---------------------------------------------------------------------------
 * Main loop
 * -------------------------------------------------------------------------*/

int main(void) {

    struct sigaction sa = { .sa_handler = sigint_handler, .sa_flags = 0 };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    srand((unsigned)time(NULL));

    sensor_state_t state;
    state_init(&state);

    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  iotdata weather station simulator               ║\n");
    printf("║  Station 42 — variant 0 (weather_station)        ║\n");
    printf("║  30s reports / 5min full reports with position   ║\n");
    printf("║  Press Ctrl-C to stop                            ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    int tick = 0;
    while (running) {
        state_step(&state);
        /* Every 10th tick (5 minutes) is a full report */
        int full_report = (tick % 10 == 0);
        encode_and_display(&state, full_report);
        /* Sleep 30 seconds, but check running each second for fast exit */
        for (int i = 0; i < 30 && running; i++)
            sleep(1);
        tick++;
    }

    printf("\n  Stopped after %d packets.\n\n", tick);
    return 0;
}
