deps_config := \
	/home/AllyKalaf/esp/esp-idf/components/app_trace/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/aws_iot/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/bt/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/driver/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/esp32/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/ethernet/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/fatfs/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/freertos/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/heap/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/libsodium/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/log/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/lwip/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/mbedtls/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/openssl/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/pthread/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/spi_flash/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/spiffs/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/AllyKalaf/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/AllyKalaf/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/AllyKalaf/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/AllyKalaf/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
