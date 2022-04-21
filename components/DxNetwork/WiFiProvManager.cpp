//
// Created by Andri Yadi on 09/12/21.
//

#include "WiFiProvManager.h"
#include <cstring>
#include "wifi_provisioning/scheme_softap.h"
#include "qrcode.h"
#include "esp_check.h"

namespace dx {
    namespace network {

        constexpr auto *TAG = "WIFIPROV";

        const ESPEventID IDF_EVENT_ID_ESP_EVENT_ANY(ESP_EVENT_ANY_ID);
        const ESPEventID IDF_EVENT_ID_IP_EVENT_STA_GOT_IP(IP_EVENT_STA_GOT_IP);

        const ESPEvent IDF_EVENT_WIFI_PROV_EVENT_ANY(WIFI_PROV_EVENT, IDF_EVENT_ID_ESP_EVENT_ANY);
        const ESPEvent IDF_EVENT_WIFI_EVENT_ANY(WIFI_EVENT, IDF_EVENT_ID_ESP_EVENT_ANY);
        const ESPEvent IDF_EVENT_IP_EVENT_GOT_IP(IP_EVENT, IDF_EVENT_ID_IP_EVENT_STA_GOT_IP);

        ESP_EVENT_DEFINE_BASE(DX_NET_EVENT);
        const ESPEvent DX_NET_EVENT_WIFI_PROV_ANY(DX_NET_EVENT, DX_NET_EVENT_ID_WIFI_PROV_ANY);
        const ESPEvent DX_NET_EVENT_WIFI_PROV_SUCCESS(DX_NET_EVENT, DX_NET_EVENT_ID_WIFI_PROV_SUCCESS);
        const ESPEvent DX_NET_EVENT_WIFI_PROV_GOT_IP(DX_NET_EVENT, DX_NET_EVENT_ID_WIFI_PROV_GOT_IP);
        const ESPEvent DX_NET_EVENT_WIFI_PROV_FAILED(DX_NET_EVENT, DX_NET_EVENT_ID_WIFI_PROV_FAILED);
        const ESPEvent DX_NET_EVENT_WIFI_PROV_GIVEUP(DX_NET_EVENT, DX_NET_EVENT_ID_WIFI_PROV_GIVEUP);

        #define DX_WIFIPROV_CONNECTED_BIT   BIT0
        #define DX_WIFIPROV_FAILED_BIT      BIT1
        static EventGroupHandle_t dx_wifiprov_event_group;

        static wifi_prov_mgr_config_t createProvConfig() {
            /* Configuration for the provisioning manager */
            wifi_prov_mgr_config_t _config = {};

#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_BLE
            _config.scheme = wifi_prov_scheme_ble;
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_BLE */
#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP
            _config.scheme = wifi_prov_scheme_softap ;
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP */

            /* Any default scheme specific event handler that you would
             * like to choose. Since our example application requires
             * neither BT nor BLE, we can choose to release the associated
             * memory once provisioning is complete, or not needed
             * (in case when device is already provisioned). Choosing
             * appropriate scheme specific event handler allows the manager
             * to take care of this automatically. This can be set to
             * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_BLE
            _config.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_BLE */
#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP
            _config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP */

            return _config;
        }

        static void getDeviceServiceName(char *service_name, size_t max)
        {
            uint8_t eth_mac[6];
            const char *ssid_prefix = "PROV_";
            esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
            snprintf(service_name, max, "%s%02X%02X%02X",
                     ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
        }

        /* Handler for the optional provisioning endpoint registered by the application.
         * The data format can be chosen by applications. Here, we are using plain ascii text.
         * Applications can choose to use other formats like protobuf, JSON, XML, etc.
         */
        esp_err_t handleCustomProvData(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                           uint8_t **outbuf, ssize_t *outlen, void *priv_data)
        {
            if (inbuf) {
                ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
            }
            char response[] = "SUCCESS";
            *outbuf = (uint8_t *)strdup(response);
            if (*outbuf == nullptr) {
                ESP_LOGE(TAG, "System out of memory");
                return ESP_ERR_NO_MEM;
            }
            *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

            return ESP_OK;
        }


        static void printQR(const char *name, const char *pop, const char *transport)
        {
            if (!name || !transport) {
                ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
                return;
            }
            char payload[150] = {0};
            if (pop) {
                snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                         DX_PROV_QR_VERSION, name, pop, transport);
            } else {
                snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"transport\":\"%s\"}",
                         DX_PROV_QR_VERSION, name, transport);
            }
#ifdef CONFIG_DXNETWORK_PROV_SHOW_QR
            ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
            esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
            esp_qrcode_generate(&cfg, payload);
#endif /* CONFIG_DXNETWORK_PROV_SHOW_QR */
            ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", DX_PROV_QRCODE_BASE_URL, payload);
        }

        esp_err_t WiFiProvManager::begin(const char *provDevName, bool forceReset) {

            esp_err_t ret = ESP_OK;
            wifi_prov_mgr_config_t _provCfg = {};
            wifi_init_config_t _wifiCfg = {};

#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            using namespace std::placeholders;

            auto _handler = std::bind(&WiFiProvManager::handleProvEvents, this, _1, _2);
//            unique_ptr<ESPEventReg> _ipEventHandler = loop_->register_event(DX_WIFI_EVENT, [](const ESPEvent &event, void *data) {
//                cout << "received event: " << event.base << "/" << event.id;
//                if (data) {
//                    cout << "; event data: " << *(static_cast<int*>(data));
//                }
//                cout << endl;
//            });

            wifiProvEventHandler_ = defaultEventLoop_->register_event(IDF_EVENT_WIFI_PROV_EVENT_ANY, _handler);
            wifiEventHandler_ = defaultEventLoop_->register_event(IDF_EVENT_WIFI_EVENT_ANY, _handler);
            ipEventHandler_ = defaultEventLoop_->register_event(IDF_EVENT_IP_EVENT_GOT_IP, _handler);

#else
            ESP_ERROR_CHECK(esp_event_loop_create_default());

            ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(WIFI_PROV_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &do_handle_prov_events,
                                                                this,
                                                                &evHandlerAnyId_),
                                GETOUT_OF_HERE, TAG, "Cannot register event %s/%d", WIFI_PROV_EVENT, ESP_EVENT_ANY_ID);

            ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &do_handle_prov_events,
                                                                this,
                                                                &evHandlerGotIp_),
                                GETOUT_OF_HERE, TAG, "Cannot register event %s/%d", WIFI_EVENT, ESP_EVENT_ANY_ID);

            ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                                IP_EVENT_STA_GOT_IP,
                                                                &do_handle_prov_events,
                                                                this,
                                                                &evHandlerProvId_),
                                GETOUT_OF_HERE, TAG, "Cannot register event %s/%d", IP_EVENT, IP_EVENT_STA_GOT_IP);

            // Register handler for DX_NET_EVENT events
            ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(DX_NET_EVENT,
                                                                DX_NET_EVENT_ID_NUM_WIFI_PROV_ANY,
                                                                &event_handler_hook,
                                                                this,
                                                                &evHandlerDxProvId_),
                                GETOUT_OF_HERE, TAG, "Cannot register event %s/%d", DX_NET_EVENT, DX_NET_EVENT_ID_NUM_WIFI_PROV_ANY);

#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX

            esp_netif_create_default_wifi_sta();

#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP
            esp_netif_create_default_wifi_ap();
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP */

            _wifiCfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&_wifiCfg));

#if 0
            wifi_config_t sta_config = {};
            //Assign ssid & password strings
//            strcpy((char*)sta_config.sta.ssid, "DyWare-C");
//            strcpy((char*)sta_config.sta.password, "429dycodex!");
            strcpy((char*)sta_config.sta.ssid, "Jivank_Ext");
            strcpy((char*)sta_config.sta.password, "p@ssw0rD4");
            //sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
            ESP_ERROR_CHECK(esp_wifi_start() );

            ESP_LOGI(TAG, "wifi_init_sta finished.");
#endif

            _provCfg = createProvConfig();

            /* Initialize provisioning manager with the
             * configuration parameters set above */
            ESP_ERROR_CHECK(wifi_prov_mgr_init(_provCfg));

#if CONFIG_DXNETWORK_RESET_PROVISIONED
            // Override
            bool _forceReset = CONFIG_DXNETWORK_RESET_PROVISIONED;
#endif //CONFIG_DXNETWORK_RESET_PROVISIONED

            if (forceReset) {
                ESP_LOGI(TAG, "Resetting provisioning...");
                wifi_prov_mgr_reset_provisioning();
            } else {
                /* Let's find out if the device is provisioned */
                ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
            }

#if CONFIG_EVENT_LOOP_PROFILING
#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            ESPEventLoop::dump();
#else
            esp_event_dump(stdout);
#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX
#endif //CONFIG_EVENT_LOOP_PROFILING

            /* If device is not yet provisioned start provisioning service */
            if (!provisioned) {
                ret = start(provDevName);

            } else {
                ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

                /* We don't need the manager as device is already provisioned,
                * so let's release it's resources */
                wifi_prov_mgr_deinit();

                /* Start Wi-Fi in station mode */
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                ESP_ERROR_CHECK(esp_wifi_start());

                ret = ESP_OK;
            }

GETOUT_OF_HERE:
            return ret;
        }


        WiFiProvManager::WiFiProvManager() {
#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            defaultEventLoop_ = std::make_shared<ESPEventLoop>();
#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX

            dx_wifiprov_event_group = xEventGroupCreate();
            xEventGroupClearBits(dx_wifiprov_event_group, DX_WIFIPROV_CONNECTED_BIT);
            xEventGroupClearBits(dx_wifiprov_event_group, DX_WIFIPROV_FAILED_BIT);
        }

        WiFiProvManager::~WiFiProvManager() {
            stop();
        }

        esp_err_t WiFiProvManager::start(const char *provDevName) {

            ESP_LOGI(TAG, "Starting provisioning");

            /* What is the Device Service Name that we want.
             * This translates to :
             *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
             *     - device name when scheme is wifi_prov_scheme_ble
             */
            char service_name[16];
            if (provDevName == nullptr) {
                getDeviceServiceName(service_name, sizeof(service_name));
            }
            else {
                snprintf(service_name, 16, "%s", provDevName);
            }

            /* What is the security level that we want (0 or 1):
             *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
             *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
             *          using X25519 key exchange and proof of possession (pop) and AES-CTR
             *          for encryption/decryption of messages.
             */
            wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

            /* Do we want a proof-of-possession (ignored if Security 0 is selected):
             *      - this should be a string with length > 0
             *      - NULL if not used
             */
            const char *pop = "abcd1234";

            /* What is the service key (could be NULL)
             * This translates to :
             *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
             *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
             *     - simply ignored when scheme is wifi_prov_scheme_ble
             */
            const char *service_key = nullptr;

#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_BLE
            /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
             * set a custom 128 bit UUID which will be included in the BLE advertisement
             * and will correspond to the primary GATT service that provides provisioning
             * endpoints as GATT characteristics. Each GATT characteristic will be
             * formed using the primary service UUID as base, with different auto assigned
             * 12th and 13th bytes (assume counting starts from 0th byte). The client side
             * applications must identify the endpoints by reading the User Characteristic
             * Description descriptor (0x2901) for each characteristic, which contains the
             * endpoint name of the characteristic */
            uint8_t custom_service_uuid[] = {
                /* LSB <---------------------------------------
                 * ---------------------------------------> MSB */
                0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
            };

            /* If your build fails with linker errors at this point, then you may have
             * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
             * the sdkconfig.defaults in the example project) */
            wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_BLE */

            /* An optional endpoint that applications can create if they expect to
             * get some additional custom data during provisioning workflow.
             * The endpoint name can be anything of your choice.
             * This call must be made before starting the provisioning.
             */
            wifi_prov_mgr_endpoint_create("custom-data");

            /* Start provisioning service */
            ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

            /* The handler for the optional endpoint created above.
             * This call must be made after starting the provisioning, and only if the endpoint
             * has already been created above.
             */
            wifi_prov_mgr_endpoint_register("custom-data", handleCustomProvData, nullptr);

            /* Uncomment the following to wait for the provisioning to finish and then release
             * the resources of the manager. Since in this case de-initialization is triggered
             * by the default event loop handler, we don't need to call the following */
            // wifi_prov_mgr_wait();
            // wifi_prov_mgr_deinit();

            /* Print QR code for provisioning */
#ifdef CONFIG_DXNETWORK_PROV_TRANSPORT_BLE
            printQR(service_name, pop, PROV_TRANSPORT_BLE);
#else /* CONFIG_DXNETWORK_PROV_TRANSPORT_SOFTAP */
            printQR(service_name, pop, DX_PROV_TRANSPORT_SOFTAP);
#endif /* CONFIG_DXNETWORK_PROV_TRANSPORT_BLE */

            return ESP_OK;
        }

        esp_err_t WiFiProvManager::postEvent(const ESPEvent &event, const chrono::milliseconds &wait_time) {
#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            if (defaultEventLoop_) {
                try {
                    defaultEventLoop_->post_event_data(event, wait_time);
                } catch (idf::ESPException &e) {
                    ESP_LOGE(TAG, "%s", esp_err_to_name(e.error));
                    return ESP_FAIL;
                }
            }

            return ESP_OK;
#else
            return esp_event_post(event.base,
                          event.id.get_id(),
                          nullptr,
                          0,
                          (wait_time.count() / portTICK_PERIOD_MS));
#endif
        }

#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
        void WiFiProvManager::handleProvEvents(const ESPEvent &event, void *event_data) {

            auto _eventBase = event.base;
            auto _eventIdNum = event.id.get_id();

            WiFiProvManager::do_handle_prov_events(this, _eventBase, _eventIdNum, event_data);
        }
#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX

        void WiFiProvManager::do_handle_prov_events(void *handler_arg, esp_event_base_t _eventBase, int32_t _eventIdNum,
                                                   void *event_data) {

            auto *_self = static_cast<WiFiProvManager*>(handler_arg);

#ifdef CONFIG_DXNETWORK_RESET_PROV_MGR_ON_FAILURE
            static int retries;
#endif

            if (_eventBase == WIFI_PROV_EVENT) {
                switch (_eventIdNum) {
                    case WIFI_PROV_START:
                        ESP_LOGI(TAG, ">> Provisioning started");
                        break;
                    case WIFI_PROV_CRED_RECV: {
                        auto *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                        ESP_LOGD(TAG, "Received Wi-Fi credentials:"
                                      "\n\tSSID     : %s\n\tPassword : %s",
                                      (const char *) wifi_sta_cfg->ssid,
                                      (const char *) wifi_sta_cfg->password);
                        break;
                    }
                    case WIFI_PROV_CRED_FAIL: {
                        ESP_LOGE(TAG, "Provisioning failed!");

                        // Commented, let handler display this
                        auto *reason = (wifi_prov_sta_fail_reason_t *)event_data;
//                        ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
//                                      "\n\tPlease reset to factory and retry provisioning",
//                                      (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
//                                      "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");

                        // Must copy event_data before passing
                        wifi_prov_sta_fail_reason_t _dispatchedEvt = *reason;

#ifdef CONFIG_DXNETWORK_RESET_PROV_MGR_ON_FAILURE
                        retries++;

                        if (retries >= CONFIG_DXNETWORK_PROV_MGR_MAX_RETRY_CNT) {

                            // Notify give up
                            _self->postEvent(DX_NET_EVENT_WIFI_PROV_GIVEUP, _dispatchedEvt);

                            ESP_LOGE(TAG, "Failed to connect with provisioned AP, resetting provisioned credentials");
                            //wifi_prov_mgr_reset_sm_state_on_failure();
                            wifi_prov_mgr_stop_provisioning();

                            retries = 0;
                        }
                        else {
                            // Notify event handler on failed, with reason data
                            _self->postEvent(DX_NET_EVENT_WIFI_PROV_FAILED, _dispatchedEvt);

                            // Restart state machine to retry provisioning
                            wifi_prov_mgr_reset_sm_state_on_failure();
                        }
#else
                        // Notify event handler on failed, with reason data
                        _self->postEvent(DX_NET_EVENT_WIFI_PROV_FAILED, _dispatchedEvt);
#endif //CONFIG_DXNETWORK_RESET_PROV_MGR_ON_FAILURE

                        xEventGroupSetBits(dx_wifiprov_event_group, DX_WIFIPROV_FAILED_BIT);

                        break;
                    }
                    case WIFI_PROV_CRED_SUCCESS:
                        //ESP_LOGI(TAG, "Provisioning successful");
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
                        retries = 0;
#endif //CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE

                        // Notify event handler on success
                        _self->postEvent(DX_NET_EVENT_WIFI_PROV_SUCCESS);
                        xEventGroupSetBits(dx_wifiprov_event_group, DX_WIFIPROV_CONNECTED_BIT);

                        break;
                    case WIFI_PROV_END:
                        ESP_LOGI(TAG, ">> Provisioning ended");

                        // De-initialize manager once provisioning is finished
                        wifi_prov_mgr_deinit();
                        break;
                    default:
                        break;
                }
            }
            else
            if (_eventBase == WIFI_EVENT && _eventIdNum == WIFI_EVENT_STA_START) {
                ESP_LOGD(TAG, ">> Station started");

                // Actually connect to AP with provided credentials
                esp_wifi_connect();

            } else if (_eventBase == WIFI_EVENT && _eventIdNum == WIFI_EVENT_STA_CONNECTED) {
                ESP_LOGD(TAG, ">> Station connected");

            } else if (_eventBase == WIFI_EVENT && _eventIdNum == WIFI_EVENT_STA_DISCONNECTED) {
                ESP_LOGE(TAG, "Connect to the AP fail");

                // Notify event handler on failed
                //_self->postEvent(DX_NET_EVENT_WIFI_PROV_FAILED, nullptr);

            } else if (_eventBase == IP_EVENT && _eventIdNum == IP_EVENT_STA_GOT_IP) {
                auto* _ipData = (ip_event_got_ip_t*) event_data;
                ESP_LOGD(TAG, ">> Station got IP: " IPSTR, IP2STR(&_ipData->ip_info.ip));

                // Notify event handler on getting IP

                // Must copy
                ip_event_got_ip_t evt = {};
                evt.if_index = -1; evt.ip_changed = false;
                memcpy(&evt.ip_info, &_ipData->ip_info, sizeof(esp_netif_ip_info_t));

                _self->postEvent(DX_NET_EVENT_WIFI_PROV_GOT_IP, evt);

                // If previously provisioned
                if (_self->provisioned) {
                    // Notify event handler on success
                    _self->postEvent(DX_NET_EVENT_WIFI_PROV_SUCCESS);
                }
            }
            else {
                ESP_LOGD(TAG, ">> Unhandled event: %s/%d", _eventBase, _eventIdNum);
            }
        }

        void WiFiProvManager::setHandler(std::function<void(const ESPEvent &, void *)> cb) {

#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            if (!defaultEventLoop_) {
                return;
            }

            dxProvEventHandler_ = defaultEventLoop_->register_event(DX_NET_EVENT_WIFI_PROV_ANY, std::move(cb));

#else
            // Store user callback of this WiFiProvManager
            userCallback_ = std::move(cb);

#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX

        }


        void WiFiProvManager::stop() {
#if CONFIG_DXNETWORK_USE_EVENT_API_CXX
            wifiProvEventHandler_.reset();
            wifiEventHandler_.reset();
            ipEventHandler_.reset();
            dxProvEventHandler_.reset();

            defaultEventLoop_.reset();
#else
            // Unregister events
            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_PROV_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &evHandlerAnyId_));

            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &evHandlerGotIp_));

            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
                                                                IP_EVENT_STA_GOT_IP,
                                                                &evHandlerProvId_));

            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(DX_NET_EVENT,
                                                                DX_NET_EVENT_ID_NUM_WIFI_PROV_ANY,
                                                                &evHandlerDxProvId_));

            // Delete event loop as it's created by this class
            esp_event_loop_delete_default();
#endif //CONFIG_DXNETWORK_USE_EVENT_API_CXX

            wifi_prov_mgr_deinit();
        }

        bool WiFiProvManager::isProvisioned(bool recheck) {

            if (recheck) {
                auto _err = wifi_prov_mgr_is_provisioned(&provisioned);
                if (_err != ESP_OK) {
                    return false;
                }
            }

            return provisioned;
        }

        bool WiFiProvManager::waitUntilConnected() {
            EventBits_t bits = xEventGroupWaitBits(dx_wifiprov_event_group,
                                                   DX_WIFIPROV_CONNECTED_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   portMAX_DELAY);

            return (bits & DX_WIFIPROV_CONNECTED_BIT);
        }

        void WiFiProvManager::event_handler_hook(void *handler_arg, esp_event_base_t event_base, int32_t event_id,
                                                 void *event_data) {

            auto *object = static_cast<WiFiProvManager*>(handler_arg);
            object->dispatchEventHandling(ESPEvent(event_base, ESPEventID(event_id)), event_data);
        }

        void WiFiProvManager::dispatchEventHandling(const ESPEvent& event, void *event_data) {
            // Trigger user callback if set
            if (userCallback_) {
                userCallback_(event, event_data);
            }
        }

    }
}