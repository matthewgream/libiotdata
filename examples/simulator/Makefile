
DIR_IOTDATA=../..
DIR_IOTDATA_VARIANT=../iotdata
DIR_SIMULATOR=.

CC=gcc
CFLAGS_DEFINES=
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
CFLAGS_OPT=-O3
CFLAGS_INCLUDES=-I$(DIR_IOTDATA) -I$(DIR_SIMULATOR) -I$(DIR_IOTDATA_VARIANT)
CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT) $(CFLAGS_INCLUDES)
LDFLAGS =
LIBS      = -lm

.PHONY: all clean

SIMULATOR=simulator
SIMULATOR_SRC=$(DIR_SIMULATOR)/iotdata_variant_simulator.c $(DIR_SIMULATOR)/iotdata_variant_simulator.h \
	$(DIR_IOTDATA_VARIANT)/iotdata_variant_suite.h \
	$(DIR_IOTDATA)/iotdata.h $(DIR_IOTDATA)/iotdata.c

all: $(SIMULATOR)

$(SIMULATOR): $(SIMULATOR_SRC)
	$(CC) $(CFLAGS) -DTEST_MAIN -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f $(SIMULATOR)

