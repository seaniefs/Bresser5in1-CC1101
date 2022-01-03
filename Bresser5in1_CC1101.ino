/*
Test to decode bresser-5-in-1 data on 868Mhz using CC1101

Uses information/code cribbed from a variety of sources:

https://github.com/RFD-FHEM/RFFHEM/blob/00b9492bc3e4a88d7457e68bde55a91846247b4c/FHEM/lib/SD_ProtocolData.pm
https://github.com/RFD-FHEM/SIGNALDuino
https://github.com/jgromes/RadioLib/issues/168
https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c

02/01/22 - Sean Siford
*/

#include <Arduino.h>
#include <RadioLib.h>
#include <stdint.h>

CC1101 radio = new Module(PIN_CC1101_CS, PIN_CC1101_GDO0, RADIOLIB_NC, PIN_CC1101_GDO2);

typedef enum DecodeStatus {
  DECODE_OK, DECODE_PAR_ERR, DECODE_CHK_ERR
} DecodeStatus;

struct WeatherData_S {
    uint8_t sensor_id;
    float   temp_c;
    int     humidity;
    float   wind_direction_deg;
    float   wind_gust_meter_sec;
    float   wind_avg_meter_sec;
    float   rain_mm;
    bool    battery_ok;
};

typedef struct WeatherData_S WeatherData;

// Cribbed from rtl_433 project - but added extra checksum to verify uu
//
// Example input data:
//   EA EC 7F EB 5F EE EF FA FE 76 BB FA FF 15 13 80 14 A0 11 10 05 01 89 44 05 00
//   CC CC CC CC CC CC CC CC CC CC CC CC CC uu II SS GG DG WW  W TT  T HH RR  R Bt
// - C = Check, inverted data of 13 byte further
// - uu = checksum (number/count of set bits within bytes 14-25)
// - I = station ID (maybe)
// - G = wind gust in 1/10 m/s, normal binary coded, GGxG = 0x76D1 => 0x0176 = 256 + 118 = 374 => 37.4 m/s.  MSB is out of sequence.
// - D = wind direction 0..F = N..NNE..E..S..W..NNW
// - W = wind speed in 1/10 m/s, BCD coded, WWxW = 0x7512 => 0x0275 = 275 => 27.5 m/s. MSB is out of sequence.
// - T = temperature in 1/10 °C, BCD coded, TTxT = 1203 => 31.2 °C
// - t = temperature sign, minus if unequal 0
// - H = humidity in percent, BCD coded, HH = 23 => 23 %
// - R = rain in mm, BCD coded, RRxR = 1203 => 31.2 mm
// - B = Battery. 0=Ok, 8=Low.
// - S = sensor type, only low nibble used, 0x9 for Bresser Professional Rain Gauge
//
// Parameters:
//
// msg     - Pointer to message
// msgSize - Size of message
// pOut    - Pointer to WeatherData
//
// Returns:
//
// DECODE_OK      - OK - WeatherData will contain the updated information
// DECODE_PAR_ERR - Parity Error
// DECODE_CHK_ERR - Checksum Error
//
DecodeStatus decodeBresser5In1Payload(uint8_t *msg, uint8_t msgSize, WeatherData *pOut) { 
    // First 13 bytes need to match inverse of last 13 bytes
    for (unsigned col = 0; col < msgSize / 2; ++col) {
        if ((msg[col] ^ msg[col + 13]) != 0xff) {
            Serial.printf("%s: Parity wrong at %u\n", __func__, col);
            return DECODE_PAR_ERR;
        }
    }

    // Verify checksum (number number bits set in bytes 14-25)
    uint8_t bitsSet = 0;
    uint8_t expectedBitsSet = msg[13];

    for(uint8_t p = 14 ; p < msgSize ; p++) {
      uint8_t currentByte = msg[p];
      while(currentByte) {
        bitsSet += (currentByte & 1);
        currentByte >>= 1;
      }
    }

    if (bitsSet != expectedBitsSet) {
       Serial.printf("%s: Checksum wrong actual [%02X] != expected [%02X]\n", __func__, bitsSet, expectedBitsSet);
       return DECODE_CHK_ERR;
    }

    pOut->sensor_id = msg[14];

    int temp_raw = (msg[20] & 0x0f) + ((msg[20] & 0xf0) >> 4) * 10 + (msg[21] &0x0f) * 100;
    if (msg[25] & 0x0f) {
        temp_raw = -temp_raw;
    }
    pOut->temp_c = temp_raw * 0.1f;

    pOut->humidity = (msg[22] & 0x0f) + ((msg[22] & 0xf0) >> 4) * 10;

    pOut->wind_direction_deg = ((msg[17] & 0xf0) >> 4) * 22.5f;

    int gust_raw = ((msg[17] & 0x0f) << 8) + msg[16];
    pOut->wind_gust_meter_sec = gust_raw * 0.1f;

    int wind_raw = (msg[18] & 0x0f) + ((msg[18] & 0xf0) >> 4) * 10 + (msg[19] & 0x0f) * 100;
    pOut->wind_avg_meter_sec = wind_raw * 0.1f;

    int rain_raw = (msg[23] & 0x0f) + ((msg[23] & 0xf0) >> 4) * 10 + (msg[24] & 0x0f) * 100;
    pOut->rain_mm = rain_raw * 0.1f;

    pOut->battery_ok = (msg[25] & 0x80) ? false : true;

    return DECODE_OK;
}

void setup() {
    Serial.begin(115200);
    Serial.println("[CC1101] Initializing ... ");
    int state = radio.begin(868.35, 8.22, 57.136417, 270.0, 10, 32);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("success!");
        state = radio.setCrcFiltering(false);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("[CC1101] Error disabling crc filtering: [%d]\n", state);
            while (true)
                ;
        }
        state = radio.fixedPacketLengthMode(27);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("[CC1101] Error setting fixed packet length: [%d]\n", state);
            while (true)
                ;
        }
        // Preamble: AA AA AA AA AA
        // Sync is: 2D D4 
        // Preamble 40 bits but the CC1101 doesn't allow us to set that
        // so we use a preamble of 32 bits and then use the sync as AA 2D
        // which then uses the last byte of the preamble - we recieve the last sync byte
        // as the 1st byte of the payload.
        state = radio.setSyncWord(0xAA, 0x2D, 0, false);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("[CC1101] Error setting sync words: [%d]\n", state);
            while (true)
                ;
        }
    } else {
        Serial.printf("[CC1101] Error initialising: [%d]\n", state);
        while (true)
            ;
    }
    Serial.println("[CC1101] Setup complete - awaiting incoming messages...");
}
void loop() {
    uint8_t recvData[27];
    int state = radio.receive(recvData, 27);
    if (state == RADIOLIB_ERR_NONE) {
      // Verify last syncword is 1st byte of payload (saa above)
      if (recvData[0] == 0xD4) {
          #ifdef _DEBUG_MODE_
            // print the data of the packet
            Serial.print("[CC1101] Data:\t\t");
            for(int i = 0 ; i < sizeof(recvData) ; i++) {
              Serial.printf(" %02X", recvData[i]);
            }
            Serial.println();

            Serial.printf("[CC1101] R [0x%02X] RSSI: %f LQI: %d\n", recvData[0], radio.getRSSI(), radio.getLQI());
          #endif

          // Decode the information - skip the last sync byte we use to check the data is OK
          WeatherData weatherData = { 0 };
          if(decodeBresser5In1Payload(&recvData[1], sizeof(recvData) - 1, &weatherData) == DECODE_OK) {
            const float METERS_SEC_TO_MPH = 2.237;
            printf("Type: [Bresser-5in1] Id: [%d] Battery: [%s]\nTemp: [%.1fC] Hum: [%d]\nWind Gust: [%.1f mph] Speed: [%.1f mph] Direction: [%.1f]\nRain [%.1f mm]\n",
                  weatherData.sensor_id,
                  weatherData.battery_ok ? "OK" : "Low",
                  weatherData.temp_c,
                  weatherData.humidity,
                  weatherData.wind_gust_meter_sec * METERS_SEC_TO_MPH,
                  weatherData.wind_avg_meter_sec * METERS_SEC_TO_MPH,
                  weatherData.wind_direction_deg, weatherData.rain_mm);
            //printf("{\"sensor_type\": \"bresser-5-in-1\", \"sensor_id\": %d, \"battery\": \"%s\", \"temp_c\": %.1f, \"hum_pc\": %d, \"wind_gust_ms\": %.1f, \"wind_speed_ms\": %.1f, \"wind_dir\": %.1f, \"rain_mm\": %.1f}\n",
            //       sensor_id, !battery_low ? "OK" : "Low",
            //       temperature, humidity, wind_gust, wind_avg, wind_direction_deg, rain);
          }

      }
      else {
        #ifdef _DEBUG_MODE_
          Serial.printf("[CC1101] R [0x%02X] RSSI: %f LQI: %d\n", recvData[0], radio.getRSSI(), radio.getLQI());
        #endif
      }
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        #ifdef _DEBUG_MODE_
          Serial.print("T");
        #endif
    }
    else {
        // some other error occurred
        Serial.printf("[CC1101] Receive failed - failed, code %d\n", state);
    }
}