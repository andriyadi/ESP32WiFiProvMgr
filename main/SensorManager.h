//
// Created by Andri Yadi on 25/04/22.
//

#ifndef WIFI_PROV_MGR_SENSORMANAGER_H
#define WIFI_PROV_MGR_SENSORMANAGER_H

#include "esp_log.h"
#include "esp_err.h"

#define SENSOR_TEMP_OFFSET (-40.0f)

class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    esp_err_t begin();
    esp_err_t readCPUTemperature(float *outTemp);

protected:
    float lastTemperature_ = 0.0f;
};


#endif //WIFI_PROV_MGR_SENSORMANAGER_H
