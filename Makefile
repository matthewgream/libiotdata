#
# IoT Sensor Telemetry Protocol
# Copyright(C) 2026 Matthew Gream (https://libiotdata.org)
#
# Makefile - iotdata library, test and support Makefile
#
# Targets:
#   all           - Build library and both test suites
#   test          - Build and run all tests
#   test-default  - Build and run default variant tests
#   test-custom   - Build and run custom variant tests
#   test-complete - Build and run comprehensive all-field-type tests
#   test-failures - Build and run failure oriented tests
#   test-example  - Build and run example default variant test
#   test-versions - Build and run all compile-time variant smoke tests
#   minimal       - Build and show full versus minimal build sizes (native)
#   minimal-esp32 - Build and show full versus minimal build sizes (esp32 cross)
#   lib           - Build static library
#   clean         - Remove build artifacts
#   format        - Run clang-format across the code
#
# Compile-time options (pass via EXTRA=):
#   IOTDATA_VARIANT_MAPS_DEFAULT   Default variant maps (weather station)
#   IOTDATA_VARIANT_MAPS <sym>     Custom variant maps array symbol
#   IOTDATA_VARIANT_MAPS_COUNT <n> Number of entries in custom maps
#   IOTDATA_ENABLE_SELECTIVE       Only compile explicitly enabled elements
#   IOTDATA_ENABLE_xxx             Enable individual field types
#   IOTDATA_ENABLE_TLV             Enable TLV
#   IOTDATA_NO_DECODE              Exclude decoder
#   IOTDATA_NO_ENCODE              Exclude encoder
#   IOTDATA_NO_PRINT               Exclude Print output support
#   IOTDATA_NO_DUMP                Exclude Dump output support
#   IOTDATA_NO_JSON                Exclude JSON support
#   IOTDATA_NO_TLV_SPECIFIC        Exclude TLV specific type handling
#   IOTDATA_NO_CHECKS_STATE        Remove runtime state checks
#   IOTDATA_NO_CHECKS_TYPES        Remove runtime type checks (e.g. temp limits)
#   IOTDATA_NO_ERROR_STRINGS       Exclude error strings (iotdata_strerror)
#   IOTDATA_NO_FLOATING_DOUBLES    Use float instead of double for position
#   IOTDATA_NO_FLOATING            Integer-only mode (no float/double)
#

CC=gcc
CFLAGS_DEFINES=
CFLAGS_COMMON=-Wall -Wextra -Wpedantic
CFLAGS_STRICT=-Werror \
    -Wstrict-prototypes \
    -Wold-style-definition \
    -Wcast-align -Wcast-qual -Wconversion \
    -Wfloat-equal -Wformat=2 -Wformat-security \
    -Winit-self -Wjump-misses-init \
    -Wlogical-op -Wmissing-include-dirs \
    -Wnested-externs -Wpointer-arith \
    -Wredundant-decls -Wshadow \
    -Wstrict-overflow=2 -Wswitch-default \
    -Wundef \
    -Wunreachable-code -Wunused \
    -Wwrite-strings
CFLAGS_OPT=-Os
# CFLAGS_OPT=-O6
CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT)
CFLAGS_NO_FLOATING_POINT=-mno-sse -mno-mmx -mno-80387
CFLAGS_VERSIONS=-DIOTDATA_VARIANT_MAPS=test_version_variants -DIOTDATA_VARIANT_MAPS_COUNT=1
CFLAGS_SELECTIVE=-DIOTDATA_ENABLE_SELECTIVE \
    -DIOTDATA_ENABLE_TLV \
    -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_LINK \
    -DIOTDATA_ENABLE_ENVIRONMENT -DIOTDATA_ENABLE_TEMPERATURE -DIOTDATA_ENABLE_PRESSURE -DIOTDATA_ENABLE_HUMIDITY \
    -DIOTDATA_ENABLE_WIND -DIOTDATA_ENABLE_WIND_SPEED -DIOTDATA_ENABLE_WIND_DIRECTION -DIOTDATA_ENABLE_WIND_GUST \
    -DIOTDATA_ENABLE_RAIN -DIOTDATA_ENABLE_RAIN_RATE -DIOTDATA_ENABLE_RAIN_SIZE \
    -DIOTDATA_ENABLE_SOLAR -DIOTDATA_ENABLE_CLOUDS \
    -DIOTDATA_ENABLE_AIR_QUALITY -DIOTDATA_ENABLE_AIR_QUALITY_INDEX -DIOTDATA_ENABLE_AIR_QUALITY_PM -DIOTDATA_ENABLE_AIR_QUALITY_GAS \
    -DIOTDATA_ENABLE_RADIATION -DIOTDATA_ENABLE_RADIATION_CPM -DIOTDATA_ENABLE_RADIATION_DOSE \
    -DIOTDATA_ENABLE_DEPTH -DIOTDATA_ENABLE_POSITION -DIOTDATA_ENABLE_DATETIME \
    -DIOTDATA_ENABLE_FLAGS -DIOTDATA_ENABLE_IMAGE
AR      = ar
LDFLAGS =
LIBS      = -lm -lcjson
LIBS_NOJSON = -lm
LIBS_NOJSON_NO_MATH =
LIBS_NO_MATH=-lcjson
EXTRA   ?= -DIOTDATA_VARIANT_MAPS_DEFAULT

CC_MACHINE := $(shell $(CC) -dumpmachine)
ifneq ($(findstring x86_64,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else ifneq ($(findstring i686,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else ifneq ($(findstring i386,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else
    CFLAGS_NO_FLOATING_POINT =
endif
CFLAGS_STACK_USAGE=-fstack-usage
STACK_USAGE_FILE_LIST=*.su
STACK_USAGE_FILE_EXAMPLE_IOTDATA=test_example-iotdata.su

LIB_SRC    = iotdata.c
LIB_OBJ    = iotdata.o
LIB_HDR    = iotdata.h
LIB_STATIC = libiotdata.a

TEST_DEFAULT_SRC = test_default.c
TEST_DEFAULT_BIN = test_default
TEST_CUSTOM_SRC  = test_custom.c
TEST_CUSTOM_BIN  = test_custom
TEST_COMPLETE_SRC = test_complete.c
TEST_COMPLETE_BIN = test_complete
TEST_EXAMPLE_SRC = test_example.c
TEST_EXAMPLE_BIN = test_example
TEST_FAILURES_SRC = test_failures.c
TEST_FAILURES_BIN = test_failures
TEST_VERSION_SRC = test_version.c

MINIMAL_OBJ=iotdata_full.o iotdata_minimal.o

VERSION_BINS = \
    test_version_FULL \
    test_version_NO_PRINT \
    test_version_NO_DUMP \
    test_version_NO_JSON \
    test_version_NO_DECODE \
    test_version_NO_ENCODE \
    test_version_NO_FLOATING \
    test_version_NO_FLOATING_NO_JSON \
    test_version_NO_ERROR_STRINGS \
    test_version_NO_FLOATING_DOUBLES \
    test_version_SELECTIVE \
    test_version_NO_CHECKS

.PHONY: all test test-default test-custom test-complete test-failures test-example test-versions lib format clean minimal

all: lib $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_COMPLETE_BIN) $(TEST_FAILURES_BIN) $(TEST_EXAMPLE_BIN)

lib: $(LIB_STATIC)

$(LIB_OBJ): $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) $(EXTRA) -c $(LIB_SRC) -o $(LIB_OBJ)
$(LIB_STATIC): $(LIB_OBJ)
	$(AR) rcs $(LIB_STATIC) $(LIB_OBJ)

$(TEST_DEFAULT_BIN): $(TEST_DEFAULT_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT $(TEST_DEFAULT_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_DEFAULT_BIN)
$(TEST_CUSTOM_BIN): $(TEST_CUSTOM_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS=custom_variants -DIOTDATA_VARIANT_MAPS_COUNT=3 $(TEST_CUSTOM_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_CUSTOM_BIN)
$(TEST_COMPLETE_BIN): $(TEST_COMPLETE_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS=complete_variants -DIOTDATA_VARIANT_MAPS_COUNT=2 $(TEST_COMPLETE_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_COMPLETE_BIN)
$(TEST_FAILURES_BIN): $(TEST_FAILURES_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS=failure_variants -DIOTDATA_VARIANT_MAPS_COUNT=2 $(TEST_FAILURES_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_FAILURES_BIN)
$(TEST_EXAMPLE_BIN): $(TEST_EXAMPLE_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_STACK_USAGE) -DIOTDATA_VARIANT_MAPS_DEFAULT $(TEST_EXAMPLE_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_EXAMPLE_BIN)

test-default: $(TEST_DEFAULT_BIN)
	./$(TEST_DEFAULT_BIN)
test-custom: $(TEST_CUSTOM_BIN)
	./$(TEST_CUSTOM_BIN)
test-complete: $(TEST_COMPLETE_BIN)
	./$(TEST_COMPLETE_BIN)
test-failures: $(TEST_FAILURES_BIN)
	./$(TEST_FAILURES_BIN)
test-example: $(TEST_EXAMPLE_BIN)
	./$(TEST_EXAMPLE_BIN)

test: $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_COMPLETE_BIN) $(TEST_FAILURES_BIN)
	./$(TEST_DEFAULT_BIN)
	./$(TEST_CUSTOM_BIN)
	./$(TEST_COMPLETE_BIN)
	./$(TEST_FAILURES_BIN)

test_version_FULL: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_PRINT: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_PRINT \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_DUMP: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_DUMP \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_JSON: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_JSON \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NOJSON) -o $@
test_version_NO_DECODE: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_DECODE \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_ENCODE: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_ENCODE \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_FLOATING: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_FLOATING \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NO_MATH) -o $@
test_version_NO_FLOATING_NO_JSON: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_FLOATING -DIOTDATA_NO_JSON $(CFLAGS_NO_FLOATING_POINT) \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NOJSON_NO_MATH) -o $@
test_version_NO_ERROR_STRINGS: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_ERROR_STRINGS \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_FLOATING_DOUBLES: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_FLOATING_DOUBLES \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_SELECTIVE: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) $(CFLAGS_SELECTIVE) \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_CHECKS: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_VERSIONS) -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test-versions: $(VERSION_BINS)
	@echo ""
	@echo "=== iotdata — build variant smoke tests ==="
	@echo ""
	@for t in $(VERSION_BINS); do ./$$t; done
	@echo ""

test-all: test test-versions test-complete test-example

format:
	clang-format -i *.[ch]

clean:
	rm -f $(LIB_OBJ) $(LIB_STATIC) $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_COMPLETE_BIN) $(TEST_FAILURES_BIN) $(TEST_EXAMPLE_BIN) $(VERSION_BINS) $(MINIMAL_OBJ) $(STACK_USAGE_FILE_LIST)

################################################################################

stack-usage:
	@cat $(STACK_USAGE_FILE_EXAMPLE_IOTDATA) | sort -t"$$(printf '\t')" -k2 -rn | head -10 | column -t

################################################################################

minimal:
	@echo "--- Full library ---"
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -c $(LIB_SRC) -o iotdata_full.o
	@size iotdata_full.o
	@echo "--- Minimal encoder (battery + environment, integer-only) ---"
	$(CC) $(CFLAGS) $(CFLAGS_NO_FLOATING_POINT) \
		-DIOTDATA_NO_DECODE \
		-DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
		-DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
		-DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
		-c $(LIB_SRC) -o iotdata_minimal.o
	@echo "Minimal object size:"
	@size iotdata_minimal.o
	@objdump -t iotdata_minimal.o | grep ' \.data\| \.rodata' | sort -k5 -rn
	@rm -f iotdata_full.o iotdata_minimal.o

################################################################################

ESP_CC = riscv32-esp-elf-gcc
ESP_CFLAGS_BASE = -march=rv32imc -mabi=ilp32 -Os

minimal-esp32:
	@echo "--- ESP32-C3 full library (no JSON) ---"
	$(ESP_CC) $(ESP_CFLAGS_BASE) -DIOTDATA_NO_JSON -c $(LIB_SRC) -o iotdata_esp32c3_full.o
	@riscv32-esp-elf-size iotdata_esp32c3_full.o
	@echo "--- ESP32-C3 minimal encoder (battery + environment, integer-only) ---"
	$(ESP_CC) $(ESP_CFLAGS_BASE) \
		-DIOTDATA_NO_DECODE \
		-DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
		-DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
		-DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS -DIOTDATA_NO_CHECKS_STATE -DIOTDATA_NO_CHECKS_TYPES \
		-c $(LIB_SRC) -o iotdata_esp32c3_minimal.o
	@echo "Minimal object size:"
	@riscv32-esp-elf-size iotdata_esp32c3_minimal.o
	@riscv32-esp-elf-objdump -t iotdata_esp32c3_minimal.o | grep ' \.data\| \.rodata' | sort -k5 -rn
	@rm -f iotdata_esp32c3_full.o iotdata_esp32c3_minimal.o

################################################################################

