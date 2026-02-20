
DIR_IOTDATA=../..
DIR_SIMULATOR=.
DIR_IOTDATA_VARIANT=../iotdata

CC?=gcc
CFLAGS?=-Wall -Wextra -Werror -O2
CFLAGS+=-I$(DIR_IOTDATA) -I$(DIR_SIMULATOR) -I$(DIR_IOTDATA_VARIANT)

.PHONY: all clean

SIMULATOR=simulator
SIMULATOR_SRC=$(DIR_SIMULATOR)/iotdata_variant_simulator.c $(DIR_SIMULATOR)/iotdata_variant_simulator.h \
	$(DIR_IOTDATA_VARIANT)/iotdata_variant_suite.h \
	$(DIR_IOTDATA)/iotdata.h $(DIR_IOTDATA)/iotdata.c

all: $(SIMULATOR)

$(SIMULATOR): $(SIMULATOR_SRC)
	$(CC) $(CFLAGS) -DTEST_MAIN -o $@ $< -lm

clean:
	rm -f $(SIMULATOR)

