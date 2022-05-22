//
// Created by Andri Yadi on 09/12/21.
//

#ifndef DXNETWORK_WIFIPROVMANAGER_H
#define DXNETWORK_WIFIPROVMANAGER_H

#include <iostream>
#include "esp_event.h"
#include "esp_err.h"
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>
#include "sdkconfig.h"

#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
#include "esp_event_cxx.hpp"
#include "esp_exception.hpp"
using namespace idf::event;
#else
#include <functional>
#include <chrono>
#include "DxESPEventWrapper.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#endif

using namespace std;

namespace dx {
namespace network {

ESP_EVENT_DECLARE_BASE(DX_NET_EVENT);

#define DX_NET_EVENT_ID_NUM_WIFI_PROV_ANY   (-1)
enum DxWifiProvEvent_e {
    DX_NET_EVENT_ID_NUM_WIFI_PROV_SUCCESS = 0,
    DX_NET_EVENT_ID_NUM_WIFI_PROV_GOT_IP,
    DX_NET_EVENT_ID_NUM_WIFI_PROV_FAILED,
    DX_NET_EVENT_ID_NUM_WIFI_PROV_GIVEUP
};

const ESPEventID DX_NET_EVENT_ID_WIFI_PROV_ANY(DX_NET_EVENT_ID_NUM_WIFI_PROV_ANY);
const ESPEventID DX_NET_EVENT_ID_WIFI_PROV_SUCCESS(DX_NET_EVENT_ID_NUM_WIFI_PROV_SUCCESS);
const ESPEventID DX_NET_EVENT_ID_WIFI_PROV_GOT_IP(DX_NET_EVENT_ID_NUM_WIFI_PROV_GOT_IP);
const ESPEventID DX_NET_EVENT_ID_WIFI_PROV_FAILED(DX_NET_EVENT_ID_NUM_WIFI_PROV_FAILED);
const ESPEventID DX_NET_EVENT_ID_WIFI_PROV_GIVEUP(DX_NET_EVENT_ID_NUM_WIFI_PROV_GIVEUP);

#define DX_PROV_QR_VERSION         "v1"
#define DX_PROV_TRANSPORT_SOFTAP   "softap"
#define DX_PROV_TRANSPORT_BLE      "ble"
#define DX_PROV_QRCODE_BASE_URL    "https://espressif.github.io/esp-jumpstart/qrcode.html"

class WiFiProvManager {
public:

    WiFiProvManager();
    ~WiFiProvManager();

    esp_err_t begin(const char *provDevName = nullptr, bool forceReset=false);
    esp_err_t start(const char *provDevName = nullptr);
    esp_err_t restart(const char *provDevName = nullptr);
    void stop();

    void setHandler(std::function<void(const ESPEvent &, void*)> cb);

    bool isProvisioned(bool recheck=true);
    bool provisioned = false;

    /**
     * Will block until WiFi provisioning is successful
     */
    bool waitUntilConnected();

    esp_err_t postEvent(const ESPEvent &event,
                        const std::chrono::milliseconds &wait_time=PLATFORM_MAX_DELAY_MS);

    template<typename T>
    esp_err_t postEvent(const ESPEvent &event,
                   T &event_data,
                   const std::chrono::milliseconds &wait_time=PLATFORM_MAX_DELAY_MS);
private:

    std::string provDeviceName_;
    bool wifiProvMgrInitted_ = false;

#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
    std::shared_ptr<ESPEventLoop> defaultEventLoop_;

    unique_ptr<ESPEventReg> wifiProvEventHandler_;
    unique_ptr<ESPEventReg> wifiEventHandler_;
    unique_ptr<ESPEventReg> ipEventHandler_;
    unique_ptr<ESPEventReg> dxProvEventHandler_;

    void handleProvEvents(const ESPEvent &event, void *data);
#else
    /**
     * User event handler.
     */
    std::function<void(const ESPEvent &, void*)> userCallback_;

    esp_event_handler_instance_t evHandlerAnyId_{};
    esp_event_handler_instance_t evHandlerGotIp_{};
    esp_event_handler_instance_t evHandlerProvId_{};
    esp_event_handler_instance_t evHandlerDxProvId_{};
#endif

    /**
     * This is esp_event's handler, for WiFiProvManager's events to be handled by its caller
     */
    static void event_handler_hook(void *handler_arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data);

    /**
     * Helper function to enter the instance's scope from the generic \c event_handler_hook().
     */
    void dispatchEventHandling(const ESPEvent& event, void *event_data);

    static void do_handle_prov_events(void *handler_arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data);
};

template<typename T>
esp_err_t
WiFiProvManager::postEvent(const ESPEvent &event, T &event_data, const chrono::milliseconds &wait_time) {
#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
    if (defaultEventLoop_) {
        try {
            defaultEventLoop_->post_event_data(event, event_data, wait_time);
        } catch (idf::ESPException &e) {
            ESP_LOGE(TAG, "%s", esp_err_to_name(e.error));
            return ESP_FAIL;
        }
    }

    return ESP_OK;
#else
    return esp_event_post(event.base,
                          event.id.get_id(),
                          &event_data,
                          sizeof(event_data),
                          (wait_time.count() / portTICK_PERIOD_MS));
#endif
}

}
}

#endif //DXNETWORK_WIFIPROVMANAGER_H
