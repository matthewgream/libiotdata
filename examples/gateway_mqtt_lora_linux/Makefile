
DIR_IOTDATA=/opt/libiotdata
DIR_IOTDATA_VARIANT=../iotdata
DIR_E22XXXTXX=/opt/e22900t22u/include

##

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
CFLAGS_OPT=-O3
CFLAGS_INCLUDES=-I$(DIR_IOTDATA) -I$(DIR_E22XXXTXX) -I$(DIR_IOTDATA_VARIANT)
CFLAGS=$(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT) $(CFLAGS_INCLUDES)
LDFLAGS=
LIBS=-lcjson -lmosquitto -lm -lpthread

##

TARGET=iotdata_gateway
MAIN=iotdata_gateway.c
SOURCES=config_linux.h serial_linux.h mqtt_linux.h \
    $(DIR_E22XXXTXX)/e22xxxtxx.h \
    $(DIR_IOTDATA_VARIANT)/iotdata_variant_suite.h \
    $(DIR_IOTDATA)/iotdata.h $(DIR_IOTDATA)/iotdata.c

##

all: $(TARGET)

$(TARGET): $(MAIN) $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(MAIN) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
format:
	clang-format -i $(MAIN) $(SOURCES)

.PHONY: all clean format

##

INSTALL=iotdata-gateway
DIR_INSTALL=/usr/local/bin
DIR_DEFAULT=/etc/default
DIR_SYSTEMD=/etc/systemd/system
define install_service_systemd
	-systemctl stop $(2) 2>/dev/null || true
	-systemctl disable $(2) 2>/dev/null || true
	install -m 644 $(1).service $(DIR_SYSTEMD)/$(2).service
	systemctl daemon-reload
	systemctl enable $(2)
	systemctl start $(2) || echo "Warning: Failed to start $(2)"
endef
install_target: $(TARGET)
	install -m 755 $(TARGET) $(DIR_INSTALL)/$(INSTALL)
install_default: $(TARGET).cfg
	install -m 644 $(TARGET).cfg $(DIR_DEFAULT)/$(INSTALL)
install_service: $(TARGET).service
	$(call install_service_systemd,$(TARGET),$(INSTALL))
install: install_target install_default install_service
restart:
	systemctl restart $(INSTALL)
.PHONY: install install_target install_default install_service
.PHONY: restart

##

