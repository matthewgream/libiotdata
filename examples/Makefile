CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -O2
CFLAGS  += -I.. -Icommon

.PHONY: all clean

TEST_SIM=test_sim
TEST_SIM_SRC= common/iotdata_variant_simulator.c common/iotdata_variant_simulator.h common/iotdata_variant_suite.h ../iotdata.h ../iotdata.c

all: $(TEST_SIM)

$(TEST_SIM): $(TEST_SIM_SRC)
	$(CC) $(CFLAGS) -DTEST_MAIN -o $@ $< -lm

clean:
	rm -f $(TEST_SIM)
