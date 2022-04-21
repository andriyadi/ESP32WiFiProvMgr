# ESP32 WiFi Provisioning Manager

Convenient reusable C++ ESP-IDF component that wraps `wifi_provisioning` component, that enables WiFi provisioning over WiFi. Usage example is shown in `main/main.cpp`. 

In this example, the `sdkconfig.defaults` file sets the `CONFIG_EVENT_LOOP_PROFILING` option. 
This enables both event loop profiling that will list out all registered events.

## How to use example

### Hardware Required
Any ESP32 family development board.

### Apps Required
Unless you develop your own app, it's faster to use existing "official" app from Espressif in order to do the provisioning.

* **Android**:
  * [BLE Provisioning app on Play Store](https://play.google.com/store/apps/details?id=com.espressif.provble)
  * [SoftAP Provisioning app on Play Store](https://play.google.com/store/apps/details?id=com.espressif.provsoftap)

* **iOS**:
  * [BLE Provisioning app on app store](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141)
  * [SoftAP Provisioning app on app Store](https://apps.apple.com/in/app/esp-softap-provisioning/id1474040630)

If you do need to integrate provisioning functionity in your app, get the library here:
* Source code on GitHub: [esp-idf-provisioning-android](https://github.com/espressif/esp-idf-provisioning-android)
* Source code on GitHub: [esp-idf-provisioning-ios](https://github.com/espressif/esp-idf-provisioning-ios)

### Configure the project

```
idf.py menuconfig
```

### Build and Flash

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

```
...
I (1154) WIFIPROV: Resetting provisioning...
I (1264) WIFIPROV: Starting provisioning
I (1284) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07
D (1394) WIFIPROV: >> Station started
D (1404) WIFIPROV: >> Unhandled event: WIFI_EVENT/12
D (1404) WIFIPROV: >> Unhandled event: WIFI_EVENT/13
W (1404) wifi_prov_scheme_softap: Error adding mDNS service! Check if mDNS is running
D (1404) WIFIPROV: >> Unhandled event: WIFI_EVENT/12
I (1414) wifi_prov_mgr: Provisioning started with service name : Feeder_001 
I (1424) WIFIPROV: >> Provisioning started
I (1424) WIFIPROV: Scan this QR code from the provisioning application for Provisioning.
I (1434) QRCODE: Encoding below text with ECC LVL 0 & QR Code Version 10
I (1444) QRCODE: {"ver":"v1","name":"DeviceName_001","pop":"abcd1234","transport":"softap"}
                                      
  █▀▀▀▀▀█ ▀▀▀█▄▀█ ▀▀▄▄█▀  ▀ █▀▀▀▀▀█   
  █ ███ █  ▀▄█ █▄▀ ▄▄▀████  █ ███ █   
  █ ▀▀▀ █  ▄▀█▀█  █▀▄▀▄▀██▄ █ ▀▀▀ █   
  ▀▀▀▀▀▀▀ █▄▀ █▄█▄▀ █ █ ▀▄█ ▀▀▀▀▀▀▀   
  ▀▀▄▀▀ ▀ ▄▀▄ ▀ ▄█ ▀▀█  ▀▄  ▀▄▄ ▄▄▀   
  ▄▄▄█▀ ▀▄▄▀  ▀▀▀█▀▄▀▄▀▀ █ █  ▄█▄█▀   
   ▄▄▄▄▀▀▄███ ▄▄ ▄█ ▀▀▀ ▀█▀▄▀█ ▀▄▄▀   
  ▀ █▄  ▀▀▀█▀ ▄  ▀█ ▄▀█ █ ▀▄▄█  ▄     
  █ ▄▀▀▀▀█ ▀██▀█ █▀▄█▄█▄▄▀▀ ███▄ ██   
  ▄ █   ▀▄▀▀█▄▀▄ █  █▀█▀ ▀ █▀▀ ▀▄▄▀   
  █▄▄█▀ ▀▀ ▄ ▀█ ██ ███▄ █▄▀█ ▀▄ ▄▀    
  █ █▀▄▄▀█▄▄▄█ ▀ █ █▄▄▀ ▄█  ▄█ ▀▄▄█   
  ▀▀    ▀ ▄▀█▄ ▄ ▄█ █ ▄ ▄▀█▀▀▀█▄▄▀    
  █▀▀▀▀▀█  ▄▄▄█▀█▀▄▀██▄▀███ ▀ █ ▄     
  █ ███ █ █▀█▀█▀ █ ▄ ▄▀▄██▀▀▀▀▀▄▄▀▀   
  █ ▀▀▀ █ ▄ █▀██▀█▀▀▄██ ▄ ██▄▀█ █▄█   
  ▀▀▀▀▀▀▀ ▀     ▀▀ ▀ ▀▀  ▀▀▀▀▀▀       
                                      

I (1654) WIFIPROV: If QR code is not visible, copy paste the below URL in a browser.
https://espressif.github.io/esp-jumpstart/qrcode.html?data={"ver":"v1","name":"DeviceName_001","pop":"abcd1234","transport":"softap"}

```

