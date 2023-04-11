#include "esp_wifi.h"
#include "WifiHandler.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <stdbool.h>

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

volatile WifiConnectionStatus wifiConnectionStatus = WIFI_CONN_STATUS_NOT_CONNECTED;
volatile uint64_t lastWifiConnectionTime = 0;
volatile uint64_t wifiConnectionTime = 0;
volatile uint32_t wifiConnectionAttempts = 0;
char              hostName[64 + 1] = { 0 };
char              newWifiSsid[32 + 1] = { 0 };
char              newWifiPassword[32 + 1] = { 0 };
volatile bool     wifiDetailsChanged = false;
char              currentWifiSsid[32 + 1] = { 0 };
char              currentWifiPassword[32 + 1] = { 0 };
volatile bool     mdnsInit = false;
volatile bool     ntpInit = false;
volatile bool     ntpClockInit = false;
volatile bool     ntpTimeSet = false;
volatile uint64_t lastNtpClockCheck = 0;
volatile WiFiClientSecure *pSslClient = nullptr;

static void time_sync_complete(struct timeval *tv) {
  if(tv->tv_sec > (24 * 60 * 60)) {
    struct tm *gm = gmtime(&tv->tv_sec);
    Serial.printf("Time has been synced: [%02d/%02d/%02d - %02d:%02d]\n", gm->tm_year - 100, gm->tm_mon + 1, gm->tm_mday, gm->tm_hour, gm->tm_min);
    ntpTimeSet = true;
  }
  else {
    ntpTimeSet = false;
  }
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID().c_str());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print((int32_t)rssi);
  Serial.println(" dBm");
}

bool disconnectWifi() {
    if (mdnsInit) {
      MDNS.end();
      mdnsInit = false;
    }
    if (pSslClient != nullptr) {
      const_cast<WiFiClientSecure *>(pSslClient)->stop();
      delete pSslClient;
      pSslClient = nullptr;
    }
    WiFi.disconnect();
    ntpInit = false;
    ntpClockInit = false;
    ntpTimeSet = false;
    lastWifiConnectionTime = 0;
    wifiConnectionTime = 0;
    wifiConnectionAttempts = 0;
    wifiConnectionStatus = WIFI_CONN_STATUS_NOT_CONNECTED;
    esp_wifi_stop();
    return true;
}

WiFiClientSecure *obtainSecureWifiClient() {
  return const_cast<WiFiClientSecure *>(pSslClient);
}

WifiConnectionStatus getConnectionStatus() {
    return wifiConnectionStatus;
}

void setWifiConnectionDetails(const char *inpWifiSsid, const char *inpWifiPassword) {
    wifiDetailsChanged = false;
    strncpy(newWifiSsid, inpWifiSsid, sizeof(newWifiSsid) - 1);
    newWifiSsid[sizeof(newWifiSsid) - 1] = '\0';
    strncpy(newWifiPassword, inpWifiPassword, sizeof(newWifiPassword) - 1);
    newWifiPassword[sizeof(newWifiPassword) - 1] = '\0';
    wifiDetailsChanged = true;
    wifiConnectionAttempts = 10;
}

void setHostname(const char *inpHostName) {
    strncpy(hostName, inpHostName, sizeof(hostName) - 1);
    hostName[sizeof(hostName) - 1] = '\0';
}

unsigned long getTime() {
    time_t nowSecs = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);
    return nowSecs;
}

// Not sure if WiFiClientSecure checks the validity date of the certificate.
// Setting clock just to be sure...
void initNtpClock() {
    Serial.println("Calling configTime...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

static unsigned long getSslTime() {
  time_t nowSecs = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  return nowSecs;
}

bool checkNtpClockSet() {
    time_t nowSecs = time(nullptr);
    if (nowSecs < 8 * 3600 * 2) {
        Serial.print(".");
        return false;
    }
    if (!ntpInit) {
        char dateTime[22];
        struct tm timeinfo = {0};
        setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);
        getLocalTime(&timeinfo);
        strftime(dateTime, 20, "%d/%b/%y %H:%M:%S", &timeinfo);
        Serial.printf("Local Time is: %s\n", dateTime);
        ntpInit = true;    
    }
    return true;
}

bool handleWifiConnection() {
  int32_t wifiStatus = wifiConnectionStatus == WIFI_CONN_STATUS_NOT_CONNECTED ? WL_DISCONNECTED : WiFi.status();
  bool newConnection = wifiDetailsChanged;
  wifiDetailsChanged = false;
  if (newConnection || (wifiConnectionStatus == WIFI_CONN_STATUS_CONNECTED && wifiStatus != WL_CONNECTED) ) {
    Serial.println("Disconnecting from WIFI...");
    if (wifiConnectionStatus == WIFI_CONN_STATUS_CONNECTED) {
      if (mdnsInit) {
        MDNS.end();
        mdnsInit = false;
      }
      WiFi.disconnect();
      wifiConnectionStatus = WIFI_CONN_STATUS_NOT_CONNECTED;
    }
    if (newWifiSsid[0] != '\0') {
        wifiConnectionStatus = WIFI_CONN_STATUS_NOT_CONNECTED;
        wifiConnectionAttempts = 10;
        strncpy(currentWifiSsid, newWifiSsid, sizeof(currentWifiSsid) - 1);
        currentWifiSsid[sizeof(currentWifiSsid) - 1] = '\0';
        strncpy(currentWifiPassword, newWifiPassword, sizeof(currentWifiPassword) - 1);
        currentWifiPassword[sizeof(currentWifiPassword) - 1] = '\0';
        newWifiSsid[0] = '\0';
        newWifiPassword[0] = '\0';
    }
    if (pSslClient != nullptr) {
      delete pSslClient;
      pSslClient = nullptr;
    }
    return false;
  }

  if ( currentWifiSsid[0] != '\0' &&
       ( (wifiConnectionStatus == WIFI_CONN_STATUS_CONNECTING && wifiStatus != WL_CONNECTED && (millis() - lastWifiConnectionTime) > 10000) ||
         wifiConnectionStatus == WIFI_CONN_STATUS_NOT_CONNECTED) ) {
    if (pSslClient != nullptr) {
      delete pSslClient;
      pSslClient = nullptr;
    }
    if (wifiConnectionStatus == WIFI_CONN_STATUS_NOT_CONNECTED) {
      Serial.println("Connecting to WIFI...");
      Serial.printf("Attempting to connect to: [%s]\n", currentWifiSsid);
    }
    if ((--wifiConnectionAttempts) > 0) {
      Serial.printf("Resetting connection to: [%s]\n", currentWifiSsid);
      WiFi.disconnect();
      esp_wifi_stop();
      esp_wifi_start();
      // Important as otherwise wifi doesn't work after deep sleep...
      WiFi.enableSTA( true );
      WiFi.mode( WIFI_STA );
      esp_wifi_set_ps(WIFI_PS_NONE);
      //
      WiFi.persistent(false);
      WiFi.begin(currentWifiSsid, currentWifiPassword);
      WiFi.persistent(false);
      wifiConnectionStatus = WIFI_CONN_STATUS_CONNECTING;
      lastWifiConnectionTime = millis();
    }
    else {
      WiFi.disconnect();
      wifiConnectionStatus = WIFI_CONN_STATUS_NOT_CONNECTED;
      Serial.printf("Will retry connection to: [%s]\n", currentWifiSsid);
      lastWifiConnectionTime = millis();
      wifiConnectionAttempts = 10;
    }
    return false;
  }

  if (wifiConnectionStatus == WIFI_CONN_STATUS_CONNECTING) {
    if (wifiStatus == WL_CONNECTED) {
      Serial.printf("Connected to: [%s]\n", currentWifiSsid);
      printWiFiStatus();
      wifiConnectionTime = millis();
      char strIpAddress[32];
      IPAddress ipAddress = WiFi.localIP();
      sprintf(strIpAddress, "%d.%d.%d.%d", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
      wifiConnectionStatus = WIFI_CONN_STATUS_CONNECTED;
    }
    return false;
  }

  if (wifiConnectionStatus == WIFI_CONN_STATUS_CONNECTED) {

    if (!ntpClockInit) {
      initNtpClock();
      ntpClockInit = true;
      ntpTimeSet = false;
      return false;
    }
    // Wait for callback...
    else if (!ntpTimeSet) {
      if ((millis() - lastNtpClockCheck) > 5000) {
        lastNtpClockCheck = millis();
        ntpTimeSet = checkNtpClockSet();
      }
      return false;
    }
    else if (!mdnsInit) {
      Serial.println("Setting up MDNS...");
      if (MDNS.begin(hostName)) {
        MDNS.addService("http", "tcp", 80);
        mdnsInit = true;
        Serial.println("OK\n");
      }
      else {
        Serial.println("ERROR\n");
      }
      delay(100);
    }
    else {
        if (pSslClient != nullptr) {
          delete pSslClient;
          pSslClient = nullptr;
        }
        Serial.println("Init SSL...");
        pSslClient = new WiFiClientSecure();
        const_cast<WiFiClientSecure *>(pSslClient)->setCACertBundle(rootca_crt_bundle_start);
        return true;
    }
  }
  return false;
}