set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/qrcode)

idf_component_register(SRCS "WiFiProvManager.cpp"
        INCLUDE_DIRS "."
        PRIV_REQUIRES esp_netif driver wifi_provisioning
        REQUIRES qrcode
        )

