//
// Created by Andri Yadi on 25/04/22.
//

#include "APIClient.h"

constexpr auto *TAG = "API";

APIClient::~APIClient() {
    end();
}

APIClient::APIClient() {

}

esp_err_t APIClient::http_event_handler_hook(esp_http_client_event_t *evt) {
    if (evt->user_data == nullptr) {
        return ESP_FAIL;
    }

    // Should be APIClient object
    auto *_self = static_cast<APIClient*>(evt->user_data);
    return _self->handleEvent(evt);
}

esp_err_t APIClient::handleEvent(esp_http_client_event_t *evt) {

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data for debugging purpose
                if (config_.isAsync) {
//                    printf("%.*s", evt->data_len, (char *) evt->data);
                }

                // Append to property
                httpResponseString_.append((char*)evt->data, evt->data_len);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }

    return ESP_OK;
}

esp_err_t APIClient::begin(const APIClientConfig_t &config) {

    //Copy
    config_ = config;

    esp_err_t ret;

    esp_http_client_config_t _httpConfig = {};
    _httpConfig.url = config_.endPoint;
    _httpConfig.event_handler = http_event_handler_hook;
    _httpConfig.user_data = this;
    //_httpConfig.cert_pem = postman_root_cert_pem_start;
    _httpConfig.is_async = config_.isAsync;
    _httpConfig.timeout_ms = 15000;

    client_ = esp_http_client_init(&_httpConfig);
    ret = esp_http_client_set_method(client_, HTTP_METHOD_POST);
    ret = esp_http_client_set_header(client_, "Content-Type", "application/json");

    ret = esp_http_client_set_header(client_, "Authorization", config_.authString);

    return ret;
}

esp_err_t APIClient::postTemperatureMeasurement(float temp) {
    if (client_ == nullptr) {
        return ESP_FAIL;
    }

    esp_err_t ret;

    // Clear response
    httpResponseString_.clear();

    // Construct payload
    char *_jsonPayload = nullptr;
    asprintf(&_jsonPayload, R"([{"field": "TEMPERATURE","value":%.2f}])", temp);
    if (_jsonPayload == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_http_client_set_post_field(client_, (char*)_jsonPayload, strlen(_jsonPayload));

    while (1) {
        ret = esp_http_client_perform(client_);
        if (ret != ESP_ERR_HTTP_EAGAIN) {
            break;
        }
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client_),
                 esp_http_client_get_content_length(client_));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(ret));
    }

    if (_jsonPayload) {
        free(_jsonPayload);
    }

    // If not async, at this point http response string is fully set
    if (!config_.isAsync) {
        ESP_LOGI(TAG, "HTTPS response:");
        printf("%s", httpResponseString_.c_str());
        printf("\r\n");
    }

    // Don't cleanup just yet
    //esp_http_client_cleanup(client_);

    return ret;
}

esp_err_t APIClient::end() {
    return esp_http_client_cleanup(client_);
}
