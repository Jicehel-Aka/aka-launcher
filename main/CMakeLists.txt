idf_component_register(
    SRCS "main.cpp" "catalog.cpp" "bmp.cpp" "flasher.cpp" "ui.cpp" "settings.cpp" "i18n.cpp" "breakout.cpp"
    INCLUDE_DIRS "."
    REQUIRES gamebuino
    PRIV_REQUIRES bootloader_support app_update esp_partition nvs_flash spi_flash esp_timer mbedtls)
