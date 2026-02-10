# iotdata library Makefile
#
# Targets:
#   all           - Build library and both test suites
#   test          - Build and run all tests
#   test-default  - Build and run default variant tests
#   test-custom   - Build and run custom variant tests
#   test-example  - Build and run example default variant test
#   test-versions - Build and run all compile-time variant smoke tests
#   lib           - Build static library
#   clean         - Remove build artifacts
#
# Compile-time options (pass via EXTRA=):
#   IOTDATA_VARIANT_MAPS_DEFAULT    - Auto-enable weather station elements
#   IOTDATA_ENABLE_SELECTIVE       - Only compile explicitly enabled elements
#   IOTDATA_NO_FLOATING_DOUBLES     - Use float instead of double for position
#   IOTDATA_NO_FLOATING             - Integer-only mode (no float/double)
#   IOTDATA_ENCODE_ONLY             - Exclude decoder, JSON, print, dump
#   IOTDATA_DECODE_ONLY             - Exclude encoder
#
# Examples:
#   make                            # Full build with all elements
#   make test                       # Build and run both test suites
#   make test-versions              # Build all compile-time variants
#   make EXTRA=-DIOTDATA_ENCODE_ONLY  # Encoder-only build (no tests)
#   make minimal                    # Measure minimal encoder-only build

CC=gcc
CFLAGS_DEFINES=-D_GNU_SOURCE
CFLAGS_COMMON=-Wall -Wextra -Wpedantic
CFLAGS_STRICT=-Werror -Wcast-align -Wcast-qual \
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
# CFLAGS_OPT=-Os
CFLAGS_OPT=-O6
CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT)
CFLAGS_NO_FLOATING_POINT=-mno-sse -mno-mmx -mno-80387
AR      = ar
LDFLAGS =
LIBS      = -lm -lcjson
LIBS_NOJSON = -lm
LIBS_NOJSON_NO_MATH =
LIBS_NO_MATH=-lcjson
EXTRA   = -DIOTDATA_VARIANT_MAPS_DEFAULT

LIB_SRC    = iotdata.c
LIB_OBJ    = iotdata.o
LIB_HDR    = iotdata.h
LIB_STATIC = libiotdata.a

TEST_DEFAULT_SRC = test_default.c
TEST_DEFAULT_BIN = test_default
TEST_CUSTOM_SRC  = test_custom.c
TEST_CUSTOM_BIN  = test_custom
TEST_EXAMPLE_SRC = test_example.c
TEST_EXAMPLE_BIN = test_example
TEST_VERSION_SRC = test_version.c

VERSION_BINS = \
    test_version_FULL \
    test_version_NO_PRINT \
    test_version_NO_DUMP \
    test_version_NO_JSON \
    test_version_ENCODE_ONLY \
    test_version_DECODE_ONLY \
    test_version_NO_FLOATING \
    test_version_NO_FLOATING_NO_JSON

.PHONY: all test test-default test-custom test-example test-versions lib format clean minimal

all: lib $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_EXAMPLE_BIN)

lib: $(LIB_STATIC)

$(LIB_OBJ): $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) $(EXTRA) -c $(LIB_SRC) -o $(LIB_OBJ)

$(LIB_STATIC): $(LIB_OBJ)
	$(AR) rcs $(LIB_STATIC) $(LIB_OBJ)

$(TEST_DEFAULT_BIN): $(TEST_DEFAULT_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT $(TEST_DEFAULT_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_DEFAULT_BIN)

$(TEST_CUSTOM_BIN): $(TEST_CUSTOM_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS=custom_variants -DIOTDATA_VARIANT_MAPS_COUNT=3 $(TEST_CUSTOM_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_CUSTOM_BIN)

$(TEST_EXAMPLE_BIN): $(TEST_EXAMPLE_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT $(TEST_EXAMPLE_SRC) $(LIB_SRC) $(LIBS) -o $(TEST_EXAMPLE_BIN)

test: $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_EXAMPLE_BIN)
	./$(TEST_DEFAULT_BIN)
	./$(TEST_CUSTOM_BIN)

test-default: $(TEST_DEFAULT_BIN)
	./$(TEST_DEFAULT_BIN)

test-custom: $(TEST_CUSTOM_BIN)
	./$(TEST_CUSTOM_BIN)

test-example: $(TEST_EXAMPLE_BIN)
	./$(TEST_EXAMPLE_BIN)

test_version_FULL: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_PRINT: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_NO_PRINT \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_DUMP: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_NO_DUMP \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_JSON: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_NO_JSON \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NOJSON) -o $@
test_version_ENCODE_ONLY: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_ENCODE_ONLY \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_DECODE_ONLY: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_DECODE_ONLY \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS) -o $@
test_version_NO_FLOATING: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_NO_FLOATING \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NO_MATH) -o $@
test_version_NO_FLOATING_NO_JSON: $(TEST_VERSION_SRC) $(LIB_HDR) $(LIB_SRC)
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -DIOTDATA_NO_FLOATING -DIOTDATA_NO_JSON \
		$(CFLAGS_NO_FLOATING_POINT) \
		$(TEST_VERSION_SRC) $(LIB_SRC) $(LIBS_NOJSON_NO_MATH) -o $@

test-versions: $(VERSION_BINS)
	@echo ""
	@echo "=== iotdata — build variant smoke tests ==="
	@echo ""
	@for t in $(VERSION_BINS); do ./$$t; done
	@echo ""

format:
	clang-format -i *.[ch]

clean:
	rm -f $(LIB_OBJ) $(LIB_STATIC) $(TEST_DEFAULT_BIN) $(TEST_CUSTOM_BIN) $(TEST_EXAMPLE_BIN) $(VERSION_BINS)

minimal:
	@echo "--- Full library ---"
	$(CC) $(CFLAGS) -DIOTDATA_VARIANT_MAPS_DEFAULT -c $(LIB_SRC) -o iotdata_full.o
	@size iotdata_full.o
	@echo "--- Minimal encoder (battery + environment, integer-only) ---"
	$(CC) $(CFLAGS) \
		-DIOTDATA_ENCODE_ONLY \
		-DIOTDATA_ENABLE_SELECTIVE -DIOTDATA_ENABLE_BATTERY -DIOTDATA_ENABLE_ENVIRONMENT \
		-DIOTDATA_NO_JSON -DIOTDATA_NO_DUMP -DIOTDATA_NO_PRINT \
		-DIOTDATA_NO_FLOATING -DIOTDATA_NO_ERROR_STRINGS \
		-c $(LIB_SRC) -o iotdata_minimal.o
	@echo "Minimal object size:"
	@size iotdata_minimal.o
	@objdump -t iotdata_minimal.o | grep ' \.data\| \.rodata' | sort -k5 -rn
	@rm -f iotdata_full.o iotdata_minimal.o
