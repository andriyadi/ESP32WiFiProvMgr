/* WiFi Provisioning C++ Example

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstdlib>
#include <thread>
#include "esp_log.h"

#include "nvs_flash.h"
#include "WiFiProvManager.h"

using namespace std;
using namespace dx;
using namespace network;

std::shared_ptr<dx::network::WiFiProvManager> mgr;

constexpr auto *TAG = "APP";

static void wifiProvEventHandler(const ESPEvent &event, void *event_data) {
    if (event.base == DX_NET_EVENT && event.id == DX_NET_EVENT_ID_WIFI_PROV_SUCCESS) {
        ESP_LOGI(TAG, ">> Yay... Provisioning success!");

    } else if (event.base == DX_NET_EVENT && event.id == DX_NET_EVENT_ID_WIFI_PROV_GOT_IP) {
        if (event_data != nullptr) {
            auto *_eventData = (ip_event_got_ip_t *) event_data;
            ESP_LOGI(TAG, ">> Provisioning got IP: " IPSTR, IP2STR(&_eventData->ip_info.ip));
        }

    } else if (event.base == DX_NET_EVENT && event.id == DX_NET_EVENT_ID_WIFI_PROV_FAILED) {

        if (event_data != nullptr) {
            auto *reason = (wifi_prov_sta_fail_reason_t *) event_data;
            ESP_LOGE(TAG, "Oopps... Provisioning failed!\n\tReason : %s"
                          "\n\tPlease retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point NOT found");
        }
    } else if (event.base == DX_NET_EVENT && event.id == DX_NET_EVENT_ID_WIFI_PROV_GIVEUP) {
        ESP_LOGE(TAG, ">> Provisioning gives up!");
    }
    else {
        ESP_LOGV(TAG, ">> Unhandled event: %s/%d", event.base, event.id.get_id());
    }
}

extern "C" void app_main(void)
{
    // Set logs verbosity level in runtime for some tags
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("WIFIPROV", ESP_LOG_VERBOSE);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    // Instantiate WiFi Provisioning Manager
    mgr = std::make_shared<dx::network::WiFiProvManager>();
    // Set method to handle all events
    mgr->setHandler(wifiProvEventHandler);
    // Begin provsioning process. Note `forceReset` is set true so previous provisioning is cleared, for demo purpose
    mgr->begin("Feeder_001", true);
    //mgr->begin("Feeder_001");


    // Wait indefinitely until WiFi Provisioning is successful.
    mgr->waitUntilConnected();

    // Code below won't be executed until WiFi Provisioning is successful.
    ESP_LOGI(TAG, "All is good!");

}
