deps_config := \
	/home/borabutt/github/esp-idf/components/app_trace/Kconfig \
	/home/borabutt/github/esp-idf/components/aws_iot/Kconfig \
	/home/borabutt/github/esp-idf/components/bt/Kconfig \
	/home/borabutt/github/esp-idf/components/driver/Kconfig \
	/home/borabutt/github/esp-idf/components/efuse/Kconfig \
	/home/borabutt/github/esp-idf/components/esp32/Kconfig \
	/home/borabutt/github/esp-idf/components/esp_adc_cal/Kconfig \
	/home/borabutt/github/esp-idf/components/esp_event/Kconfig \
	/home/borabutt/github/esp-idf/components/esp_http_client/Kconfig \
	/home/borabutt/github/esp-idf/components/esp_http_server/Kconfig \
	/home/borabutt/github/esp-idf/components/esp_https_ota/Kconfig \
	/home/borabutt/github/esp-idf/components/espcoredump/Kconfig \
	/home/borabutt/github/esp-idf/components/ethernet/Kconfig \
	/home/borabutt/github/esp-idf/components/fatfs/Kconfig \
	/home/borabutt/github/esp-idf/components/freemodbus/Kconfig \
	/home/borabutt/github/esp-idf/components/freertos/Kconfig \
	/home/borabutt/github/esp-idf/components/heap/Kconfig \
	/home/borabutt/github/esp-idf/components/libsodium/Kconfig \
	/home/borabutt/github/esp-idf/components/log/Kconfig \
	/home/borabutt/github/esp-idf/components/lwip/Kconfig \
	/home/borabutt/github/esp-idf/components/mbedtls/Kconfig \
	/home/borabutt/github/esp-idf/components/mdns/Kconfig \
	/home/borabutt/github/esp-idf/components/mqtt/Kconfig \
	/home/borabutt/github/esp-idf/components/nvs_flash/Kconfig \
	/home/borabutt/github/esp-idf/components/openssl/Kconfig \
	/home/borabutt/github/esp-idf/components/pthread/Kconfig \
	/home/borabutt/github/esp-idf/components/spi_flash/Kconfig \
	/home/borabutt/github/esp-idf/components/spiffs/Kconfig \
	/home/borabutt/github/esp-idf/components/tcpip_adapter/Kconfig \
	/home/borabutt/github/esp-idf/components/unity/Kconfig \
	/home/borabutt/github/esp-idf/components/vfs/Kconfig \
	/home/borabutt/github/esp-idf/components/wear_levelling/Kconfig \
	/home/borabutt/github/esp-idf/components/wifi_provisioning/Kconfig \
	/home/borabutt/github/esp-idf/components/app_update/Kconfig.projbuild \
	/home/borabutt/github/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/borabutt/github/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/borabutt/github/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/borabutt/github/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
