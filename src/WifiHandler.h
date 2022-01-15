#ifndef _WIFIHANDLER_H_

    #include <BearSSLClient.h>

    #define _WIFIHANDLER_H_ 1

    typedef enum {
        WIFI_CONN_STATUS_NOT_CONNECTED = 0,
        WIFI_CONN_STATUS_CONNECTING = 1,
        WIFI_CONN_STATUS_CONNECTED = 2
    } WifiConnectionStatus;

    void setWifiConnectionDetails(const char *ssid, const char *password);
    void setHostname(const char *hostName);
    unsigned long getTime();
    bool checkNetClockSet();
    WifiConnectionStatus getConnectionStatus();
    bool handleWifiConnection();
    bool disconnectWifi();
    BearSSLClient *obtainSecureWifiClient();

#endif