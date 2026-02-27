# iotdata tests

Test suite for the iotdata encoder, decoder, and supporting functions. All tests
use a lightweight framework defined in `test_common.h` with `PASS`/`FAIL` output
and process exit code 0 on success, 1 on any failure.

Build and run all tests:

```
make test
```

## Test programs

### test_default

Exercises the built-in default variant map (weather station, variant 0). Covers
all 13 field types with individual round-trip tests, presence byte grouping
(pres0-only vs pres0+pres1), full-station encoding, boundary values (min/max for
every field), quantisation accuracy sweeps (temperature, wind, position,
radiation dose), TLV raw and string round-trips, JSON binary→JSON→binary
round-trips, dump and print output verification, error conditions (out-of-range
values, duplicate fields, invalid variant/station IDs), and edge cases (empty
packets, single pres1 field, packet size reporting).

### test_complete

Defines two custom variants to exercise field types not present in the default
map: `complete` (3 presence bytes, 16 fields including air quality PM/gas
channels, depth, and image) and `standalone` (3 presence bytes, 15 individual
sub-field types). Tests air quality PM partial channel masks, gas slot
boundaries, image format/size/compression/flag combinations, full-variant
encoding with all fields populated, TLV typed helpers (version, status, health,
config, diagnostic, userdata), multiple TLVs in a single packet, JSON
round-trips for both variants including TLV preservation, dump/print output, and
image RLE and heatshrink compress/decompress round-trips.

### test_custom

Defines three custom variants to verify the custom variant map mechanism:
`soil_sensor` (1 presence byte, standalone temperature and humidity),
`wind_mast` (1 presence byte, individual wind speed/direction/gust), and
`radiation_monitor` (2 presence bytes, 11 fields across pres0+pres1). Tests that
custom field ordering works, fields decode to the correct positions, partial
field encoding omits absent fields, JSON output uses custom field labels (e.g.
`soil_temp`, `soil_moist`), JSON round-trips produce identical wire bytes, print
output shows custom variant names, `iotdata_get_variant()` returns correct
definitions, and empty packets work for all variants.

### test_failures

Negative testing and error path coverage using the same two-variant layout as
test_complete. Covers negative value round-trips (temperature, RSSI, SNR,
position), boundary values for every field type, out-of-range error codes for
every encoder function, encoder state errors (null context, null buffer, encode
before begin, encode after end, duplicate fields, invalid variants, buffer too
small), decoder error paths (null inputs, zero length, truncated packets,
reserved variants), image edge cases (1-byte and 254-byte payloads, all pixel
formats), TLV edge cases (max entries, max data length, empty strings, type
boundaries, KV mismatch, invalid 6-bit characters), buffer overflow scenarios,
JSON parse errors and missing fields, dump/print with short or empty inputs, and
full-packet round-trips at all-minimum and all-maximum values.

### test_version

Build-variant smoke test. A single source file compiled multiple times with
different `#define` combinations to verify that every compile-time configuration
compiles, links, and runs correctly. Each build encodes a packet (or uses a
prebuilt one for `NO_ENCODE`), then exercises whichever API surface is
available. The Makefile builds and runs all variants automatically.

| Build                 | Configuration                               |
| --------------------- | ------------------------------------------- |
| `FULL`                | All features enabled                        |
| `NO_PRINT`            | Exclude print functions                     |
| `NO_DUMP`             | Exclude dump functions                      |
| `NO_JSON`             | Exclude JSON support (no cJSON dependency)  |
| `NO_DECODE`           | Encoder only                                |
| `NO_ENCODE`           | Decoder only                                |
| `NO_FLOATING`         | Integer-only mode (`int32_t` scaled values) |
| `NO_FLOATING_NO_JSON` | Integer-only + no JSON                      |
| `NO_ERROR_STRINGS`    | Exclude `iotdata_strerror`                  |
| `NO_FLOATING_DOUBLES` | `float` instead of `double` for position    |
| `SELECTIVE`           | All types via `IOTDATA_ENABLE_SELECTIVE`    |
| `NO_CHECKS`           | No runtime state or type checks             |

### test_example

Not a test — a live weather station simulator that runs until Ctrl-C. Generates
random-walk sensor data using the default variant, encodes a packet every 30
seconds (with position/datetime added every 5 minutes), and displays the raw
values, hex dump, diagnostic dump, decoded output, and JSON for each packet.
Useful for visually inspecting encoder output and verifying the full
encode→decode→print→JSON pipeline interactively.

## Shared framework

`test_common.h` provides the test macros (`TEST`, `PASS`, `FAIL`, `ASSERT_EQ`,
`ASSERT_NEAR`, `ASSERT_OK`, `ASSERT_ERR`, `ASSERT_TRUE`) and shared
encoder/decoder state. Each test function calls `begin()` to start encoding,
`finish()` to finalise, and `decode_pkt()` to decode the result into a shared
`iotdata_decoded_t`. Tests return early on first assertion failure within each
test function.
