idf_component_register(
    SRCS "app.c"
    PRIV_REQUIRES esp_timer esp_driver_gpio esp_driver_uart esp_driver_usb_serial_jtag 
    INCLUDE_DIRS
        "."
        "../../.."                      # iotdata.c/h
        "../../iotdata"                 # iotdata_variant_suite.h
        "../../e22xxxtxx"               # e22xxxtxx.h
        "../../simulator"               # simulator
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wcast-align -Wcast-qual -Wstrict-prototypes -Wold-style-definition -Wconversion -Wfloat-equal -Wformat=2 -Wformat-security -Winit-self -Wjump-misses-init -Wlogical-op -Wmissing-include-dirs -Wnested-externs -Wpointer-arith -Wno-redundant-decls -Wshadow -Wstrict-overflow=2 -Wswitch-default -Wswitch-enum -Wunreachable-code -Wunused -Wwrite-strings)
