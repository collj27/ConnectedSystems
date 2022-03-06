deps_config := \
	/Users/james/esp/esp-idf/components/app_trace/Kconfig \
	/Users/james/esp/esp-idf/components/aws_iot/Kconfig \
	/Users/james/esp/esp-idf/components/bt/Kconfig \
	/Users/james/esp/esp-idf/components/driver/Kconfig \
	/Users/james/esp/esp-idf/components/esp32/Kconfig \
	/Users/james/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/Users/james/esp/esp-idf/components/esp_http_client/Kconfig \
	/Users/james/esp/esp-idf/components/ethernet/Kconfig \
	/Users/james/esp/esp-idf/components/fatfs/Kconfig \
	/Users/james/esp/esp-idf/components/freertos/Kconfig \
	/Users/james/esp/esp-idf/components/heap/Kconfig \
	/Users/james/esp/esp-idf/components/http_server/Kconfig \
	/Users/james/esp/esp-idf/components/libsodium/Kconfig \
	/Users/james/esp/esp-idf/components/log/Kconfig \
	/Users/james/esp/esp-idf/components/lwip/Kconfig \
	/Users/james/esp/esp-idf/components/mbedtls/Kconfig \
	/Users/james/esp/esp-idf/components/mdns/Kconfig \
	/Users/james/esp/esp-idf/components/mqtt/Kconfig \
	/Users/james/esp/esp-idf/components/nvs_flash/Kconfig \
	/Users/james/esp/esp-idf/components/openssl/Kconfig \
	/Users/james/esp/esp-idf/components/pthread/Kconfig \
	/Users/james/esp/esp-idf/components/spi_flash/Kconfig \
	/Users/james/esp/esp-idf/components/spiffs/Kconfig \
	/Users/james/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/Users/james/esp/esp-idf/components/vfs/Kconfig \
	/Users/james/esp/esp-idf/components/wear_levelling/Kconfig \
	/Users/james/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/james/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/james/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/james/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
