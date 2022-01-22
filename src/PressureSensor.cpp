#include "PressureSensor.h"
#include <BMP280_DEV.h>
#include <Wire.h>

static bool pressureSensorInit = false;

#ifdef ENABLE_PRESSURE_SENSOR
    #pragma message("Pressure sensor enabled in build!");

    static BMP280_DEV *bmp280 = nullptr;

    bool pressureSensorAvailable() {
        byte error, address;
        int nDevices;
        Serial.println("Scanning...");
        pinMode(BMP280_PIN_POWER, OUTPUT);
        digitalWrite(BMP280_PIN_POWER, 1);
        delay(500);
        Wire.begin(BMP280_PIN_SDA, BMP280_PIN_SCL);
        nDevices = 0;
        for(address = 1; address < 127; address++ ) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            nDevices++;
            }
            else if (error==4) {
            Serial.print("Unknow error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            }    
        }
        if (nDevices == 0) {
            Serial.println("No I2C devices found\n");
        }
        else {
            Serial.println("done\n");
        }
        return true;
    }
    bool initPressureSensor() {
        closePressureSensor();
        bmp280 = new BMP280_DEV(BMP280_PIN_SDA, BMP280_PIN_SCL);
        pressureSensorInit = bmp280->begin(Mode::SLEEP_MODE, BMP280_I2C_ALT_ADDR);
        bmp280->setTempOversampling(Oversampling::OVERSAMPLING_X2);
        bmp280->setPresOversampling(Oversampling::OVERSAMPLING_X16);
        bmp280->setIIRFilter(IIRFilter::IIR_FILTER_16);
        bmp280->setTimeStandby(TimeStandby::TIME_STANDBY_1000MS);
        return pressureSensorInit;
    }
    bool readPressureSensorHpa(float& output) {
        bool  read = false;
        float localPressure = 0;
        if (pressureSensorInit) {
            bmp280->startForcedConversion();
            // Wait for up to 5 seconds to get a reading...
            for(int i = 0 ; i < 20 && read == false ; i++) {
                if(bmp280->getPressure(localPressure)) {
                    read = true;
                }
                else {
                    delay(250);
                }
            }
        }
        output = localPressure;
        return read;
    }
    void closePressureSensor() {
        if(pressureSensorInit) {
            bmp280->reset();
            bmp280->stopConversion();
            pressureSensorInit = false;
        }
    }
#else
    bool pressureSensorAvailable() {
        return false;
    }
    bool initPressureSensor() {
        return false;
    }
    bool readPressureSensorHpa(float& output) {
        output = 0;
        return false;
    }
    void closePressureSensor() {
    }
#endif