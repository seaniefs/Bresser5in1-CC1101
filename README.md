# Bresser5in1-CC1101
Project to read data from a Bresser5-in-1 Weather Station using an ESP32 and CC1101 module.

_Setup Instructions (Provisional):_

* Create a service account and populate `google_service_account.h` as indicated in [CreateGoogleAccount](../CreateGoogleAccount.md) and create a Google Sheet to hold the data.

```
#ifndef _GOOGLE_SERVICE_ACCOUNT_H_

  #define _GOOGLE_SERVICE_ACCOUNT_H_ 1

  #define GOOGLE_SA_PROJECT_ID   "<< project_id >>"
  #define GOOGLE_SA_CLIENT_EMAIL "<< client_email >>"
  #define GOOGLE_SA_PRIVATE_KEY  "<< private_key >>"
  #define GOOGLE_SHEET_ID        "<< google sheet id >>"

#endif
```

* Create `wifi_details.h` and populate as follows:
```
#ifndef _WIFI_DETAILS_H_

  #define _WIFI_DETAILS_H_ 1

  #define WIFI_SSID "<< SSID >>"
  #define WIFI_PASSWORD "<< PASSWORD >>"

#endif
```

* Setup the pins you want to use for reading the CC1101 in `platformio.ini`:
```
  '-DPIN_CC1101_CS=<<esp32 pin connected to cs on CC1101>>'
  '-DPIN_CC1101_GDO0=<<esp32 pin connected to gd0 on CC1101>>'
  '-DPIN_CC1101_GDO2=<<esp32 pin connected to gd2 on CC1101>>'
```

* If you want to use a BMP280 sensor to get pressure readings, enable it and setup the i2c pins (SDA/SCL) you want to use for reading the BMP280 in `platformio.ini`:
```
  '-DENABLE_PRESSURE_SENSOR=1'
  '-DBMP280_PIN_SDA=<< esp32 pin connected to SDA on CC1101 >>'
  '-DBMP280_PIN_SCL=<< esp32 pin connected to SCL on CC1101 >>'
```

## TODO:

* Add a web interface to allow setup of:
    * Service account information.
    * WiFi details.
    * CC1101 pins.
    * BMP280 on/off and pin assignment.
