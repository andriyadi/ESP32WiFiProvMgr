//
// Created by Andri Yadi on 25/04/22.
//

#ifndef WIFI_PROV_MGR_APICLIENT_H
#define WIFI_PROV_MGR_APICLIENT_H

#include "sdkconfig.h"
#include <string>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

struct APIClientConfig_t {
    explicit APIClientConfig_t() {}
    APIClientConfig_t(const char *ep, const char *as, bool a): endPoint(ep), authString(as), isAsync(a) {}


    const char *endPoint = nullptr;
    const char *authString = nullptr;
    bool isAsync = true;
};

class APIClient {
public:
    explicit APIClient();
    ~APIClient();

    esp_err_t begin(const APIClientConfig_t &config);
    esp_err_t end();

    esp_err_t postTemperatureMeasurement(float temp);

    esp_err_t handleEvent(esp_http_client_event_t *evt);

protected:

    APIClientConfig_t config_;
    esp_http_client_handle_t client_ = nullptr;

    std::string httpResponseString_;

    static esp_err_t http_event_handler_hook(esp_http_client_event_t *evt);
};


#endif //WIFI_PROV_MGR_APICLIENT_H
