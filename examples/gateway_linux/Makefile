
DIR_IOTDATA=../..
DIR_E22XXXTXX=../e22xxxtxx
DIR_IOTDATA_VARIANT=../iotdata


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
CFLAGS_OPT=-O6
CFLAGS_INCLUDES = -I$(DIR_IOTDATA) -I$(DIR_E22XXXTXX) -I$(DIR_IOTDATA_VARIANT)
CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT) $(CFLAGS_INCLUDES)
LDFLAGS = -lcjson -lmosquitto -lm
MAIN = iotdata_gateway.c
TARGET = iotdata_gateway
SOURCES=config_linux.h serial_linux.h mqtt_linux.h $(DIR_E22XXXTXX)/e22xxxtxx.h $(DIR_IOTDATA_VARIANT)/iotdata_variant_suite.h
HOSTNAME = $(shell hostname)

##

all: $(TARGET)

$(TARGET): $(MAIN) $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(MAIN) $(LDFLAGS)
clean:
	rm -f $(TARGET)
format:
	clang-format -i *.[ch]
.PHONY: all clean format

##

SYSTEMD_DIR = /etc/systemd/system
UDEVRULES_DIR = /etc/udev/rules.d
define install_systemd_service
	-systemctl stop $(1) 2>/dev/null || true
	-systemctl disable $(1) 2>/dev/null || true
	cp $(2).service $(SYSTEMD_DIR)/$(1).service
	systemctl daemon-reload
	systemctl enable $(1)
	systemctl start $(1) || echo "Warning: Failed to start $(1)"
endef
install_systemd_service: $(TARGET).service
	$(call install_systemd_service,$(TARGET),$(TARGET))
install_udev_rules: 90-$(TARGET).rules
	cp 90-$(TARGET).rules $(UDEVRULES_DIR)
	udevadm control --reload-rules
	udevadm trigger
install: install_udev_rules install_systemd_service
restart:
	systemctl restart $(TARGET)
.PHONY: install install_systemd_service install_udev_rules

