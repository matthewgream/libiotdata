idf_component_register(
    SRCS "app.c"
    PRIV_REQUIRES esp_timer esp_driver_gpio esp_driver_uart esp_driver_usb_serial_jtag 
    INCLUDE_DIRS
        "."
        "/opt/libiotdata"               # iotdata.c/h
        "/opt/e22900t22u/include"       # e22xxxtxx.h
        "../../iotdata"                 # iotdata_variant_suite.h
        "../../simulator"               # simulator
)
