/*
 * IoT Sensor Telemetry Protocol
 * Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
 *
 * iotdata_variant_simulator.h - multi-sensor simulator
 *
 * Simulates multiple sensors across the variant suite, each
 * producingrealistic readings with random walk, diurnal
 * patterns, and battery drain.  Poll-based: call
 * iotsim_poll() in a loop.
 *
 * Usage:
 *   iotsim_t sim;
 *   iotsim_init(&sim, seed);
 *   while (...) {
 *       iotsim_packet_t pkt;
 *       if (iotsim_poll(&sim, now_ms, &pkt))
 *           send(pkt.buf, pkt.len);
 *   }
 */

#ifndef IOTDATA_SIMULATOR_H
#define IOTDATA_SIMULATOR_H

#include "iotdata_variant_suite.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------*/

#define IOTSIM_NUM_SENSORS 16
#ifndef IOTSIM_TX_MIN_MS
#define IOTSIM_TX_MIN_MS 5000 /* 5s  minimum interval  */
#endif
#ifndef IOTSIM_TX_MAX_MS
#define IOTSIM_TX_MAX_MS 15000 /* 15s maximum interval  */
#endif
#define IOTSIM_EXTRA_FIELDS_EVERY 10 /* every ~10th TX, add extras */
#define IOTSIM_MAX_PACKET         128

/* ---------------------------------------------------------------------------
 * Per-sensor simulated state
 * -------------------------------------------------------------------------*/

typedef struct {
    /* Identity */
    uint8_t variant;     /* IOTDATA_VSUITE_* index          */
    uint16_t station_id; /* unique station ID (1-based)     */
    uint16_t sequence;   /* rolling sequence counter        */

    /* Timing */
    uint32_t next_tx_ms;     /* next scheduled transmission     */
    uint32_t tx_interval_ms; /* current interval                */
    uint32_t tx_count;       /* transmissions so far            */

    /* Simulated readings (physical units, pre-quantisation) */
    int16_t temperature;    /* centi-degrees: 2150 = 21.50°C  */
    uint16_t pressure;      /* hPa                             */
    uint8_t humidity;       /* percent                         */
    uint16_t wind_speed;    /* centi-m/s: 350 = 3.50 m/s      */
    uint16_t wind_dir;      /* degrees 0-359                   */
    uint16_t wind_gust;     /* centi-m/s                       */
    uint8_t rain_rate;      /* mm/hr                           */
    uint8_t rain_size;      /* 0.25mm units                    */
    uint16_t solar_irr;     /* W/m²                            */
    uint8_t solar_uv;       /* UV index                        */
    uint8_t clouds;         /* okta 0-8                        */
    uint16_t aq_index;      /* AQI 0-500                       */
    uint16_t aq_pm[4];      /* PM µg/m³                        */
    uint8_t aq_pm_present;  /* which PM channels               */
    uint16_t aq_gas[8];     /* gas values in native units      */
    uint8_t aq_gas_present; /* which gas channels              */
    uint16_t rad_cpm;       /* counts per minute               */
    uint16_t rad_dose;      /* centi-µSv/h: 10 = 0.10 µSv/h   */
    uint16_t depth;         /* cm                              */
    uint8_t battery;        /* percent 0-100                   */
    uint8_t flags;          /* 1-bit flags                     */
} iotsim_sensor_t;

/* ---------------------------------------------------------------------------
 * Simulator top-level state
 * -------------------------------------------------------------------------*/

typedef struct {
    iotsim_sensor_t sensors[IOTSIM_NUM_SENSORS];
    uint32_t rng_state; /* xorshift32 state */
    uint32_t time_base; /* sim start time for diurnal */
    int poll_next;      /* round-robin start index for iotsim_poll */
} iotsim_t;

/* ---------------------------------------------------------------------------
 * Output packet
 * -------------------------------------------------------------------------*/

typedef struct {
    uint8_t buf[IOTSIM_MAX_PACKET];
    size_t len;
    uint8_t sensor_index; /* which sensor [0..15]  */
    uint8_t variant;      /* variant type          */
    uint16_t station_id;  /* station ID            */
    uint16_t sequence;    /* sequence number       */
} iotsim_packet_t;

/* ---------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------*/

/* Initialise simulator with RNG seed.  Randomises sensor allocation
 * and initial readings.  time_now_ms is the starting wallclock. */
void iotsim_init(iotsim_t *sim, uint32_t seed, uint32_t time_now_ms);

/* Poll for next ready packet.  Returns true if a packet was generated.
 * Call in a loop at your desired granularity (e.g. every 100ms).
 * Only returns one packet per call — call repeatedly until false
 * to drain all due sensors. */
bool iotsim_poll(iotsim_t *sim, uint32_t time_now_ms, iotsim_packet_t *out);

/* Get sensor info (for debug/display) */
const iotsim_sensor_t *iotsim_sensor(const iotsim_t *sim, int index);

/* ---------------------------------------------------------------------------
 * Persistence — reboot-safe sequence numbers
 *
 * Only the per-station sequence counters are persisted; everything else in
 * the simulator (readings, RNG, timing) is regenerated fresh on each boot.
 * The bundle is a portable, NUL-terminated ASCII string ("station:sequence"
 * pairs) so the storage backend is decoupled from the simulator — the target
 * (e.g. ESP32 NVS/flash) just stores and returns the opaque string.
 *
 * Usage (after iotsim_init):
 *   char buf[IOTSIM_SAVE_SIZE];
 *   if (flash_load(buf, sizeof(buf)))   // returns the stored string
 *       iotsim_save_import(&sim, buf);  // restore sequence numbers
 *   ...
 *   // periodically, and/or before reboot:
 *   if (iotsim_save_export(&sim, buf, sizeof(buf)))
 *       flash_save(buf);
 * -------------------------------------------------------------------------*/

/* Magic/version prefix.  Bump the trailing digit if the format changes;
 * iotsim_save_import() rejects any string that does not start with it, so
 * old or absent/garbage data simply falls back to fresh sequence numbers. */
#define IOTSIM_SAVE_MAGIC "IOTSIMSEQ1"

/* Worst-case bundle size in bytes, including the NUL terminator.  Each entry
 * is " 65535:65535" (12 chars); sizeof(magic) already accounts for the NUL.
 * Suitable for sizing a static buffer with no malloc. */
#define IOTSIM_SAVE_SIZE  (sizeof(IOTSIM_SAVE_MAGIC) + IOTSIM_NUM_SENSORS * 12)

/* Maximum buffer size (including NUL) needed for export, and the size the
 * import buffer should be able to hold.  Returns IOTSIM_SAVE_SIZE; provided
 * for callers that prefer to query the size at runtime. */
size_t iotsim_save_size(void);

/* Export the persistable sequence numbers as a portable NUL-terminated
 * string.  bufsize must be >= iotsim_save_size().  Returns false on bad
 * arguments or if the buffer is too small. */
bool iotsim_save_export(const iotsim_t *sim, char *buf, size_t bufsize);

/* Import sequence numbers previously produced by iotsim_save_export, matching
 * by station_id (sensors absent from the string keep their current sequence).
 * Call AFTER iotsim_init.  Returns true if the string was a valid bundle,
 * false if it was malformed or foreign — in which case sim is left unchanged.
 * Backwards compatible: only sequence numbers are touched. */
bool iotsim_save_import(iotsim_t *sim, const char *buf);

#endif /* IOTDATA_SIMULATOR_H */
