#include "PressureSensor.h"
#include <BMP280_DEV.h>
#include <Wire.h>

static bool pressureSensorInit = false;

#ifdef ENABLE_PRESSURE_SENSOR
    #pragma message("Pressure sensor enabled in build!")

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
            Serial.print("Unknown error at address 0x");
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
        return pressureSensorInit;
    }
    bool readPressureSensorHpa(float& output) {
        bool  read = false;
        bool  doReset = true;
        float localPressure = 0;
        if (pressureSensorInit) {
            // Need 2 readings; discard the 1st...
            int reads = 0;
            for(int i = 0 ; i < 160 && read == false ; i++) {
                if (doReset) {
                    if (i > 0) {
                        bmp280->stopConversion();
                    }
                    bmp280->reset();
                    delay(100);
                    bmp280->setTempOversampling(Oversampling::OVERSAMPLING_X2);
                    bmp280->setPresOversampling(Oversampling::OVERSAMPLING_X16);
                    bmp280->setIIRFilter(IIRFilter::IIR_FILTER_16);
                    bmp280->setTimeStandby(TimeStandby::TIME_STANDBY_4000MS);
                    bmp280->startNormalConversion();
                    delay(100);
                    doReset = false;
                }
                if(bmp280->getPressure(localPressure)) {
                    reads++;
                    doReset = true;
                    read = reads > 1;
                    Serial.print(" P - OK");
                    Serial.print(localPressure);
                    Serial.print(" ");
                }
                else {
                    delay(1000);
                    Serial.print(" PX");
                }
            }
            bmp280->stopConversion();
        }
        output = localPressure;
        return read;
    }

    bool readPressureSensorHpaAndTemp(float& hpaOutput, float &tempOutput) {
        bool  read = false;
        bool  doReset = true;
        float localPressure = 0;
        float localTemp = -63;
        if (pressureSensorInit) {
            // Need 2 readings; discard the 1st...
            int tempReads = 0;
            int pressureReads = 0;
            int32_t endMillis = millis() + 120000;
            for(int i = 0 ; millis() < endMillis && read == false ; i++) {
                if (doReset) {
                    if (i > 0) {
                        bmp280->stopConversion();
                    }
                    bmp280->reset();
                    delay(100);
                    bmp280->setTempOversampling(Oversampling::OVERSAMPLING_X2);
                    bmp280->setPresOversampling(Oversampling::OVERSAMPLING_X16);
                    bmp280->setIIRFilter(IIRFilter::IIR_FILTER_16);
                    bmp280->setTimeStandby(TimeStandby::TIME_STANDBY_4000MS);
                    bmp280->startNormalConversion();
                    delay(100);
                    doReset = false;
                }
                if(pressureReads < 2) {
                    if (bmp280->getPressure(localPressure)) {
                    pressureReads++;
                    doReset = true;
                    Serial.print(" P - OK");
                    Serial.print(localPressure);
                    Serial.print(" ");
                    }
                }
                if(tempReads < 2) {
                    if(bmp280->getTemperature(localTemp)) {
                        tempReads++;
                        doReset = true;
                        Serial.print(" T - OK");
                        Serial.print(localTemp);
                        Serial.print(" ");
                    }
                }
                read = pressureReads > 1 && tempReads > 1;
                delay(10);
            }
            bmp280->stopConversion();
        }
        hpaOutput = localPressure;
        tempOutput = localTemp;
        return read;
    }

    bool readPressureSensorTemp(float& output) {
        bool  read = false;
        bool  doReset = true;
        float localTemp = -32;
        if (pressureSensorInit) {
            // Need 2 readings; discard the 1st...
            int reads = 0;
            for(int i = 0 ; i < 160 && read == false ; i++) {
                if (doReset) {
                    if (i > 0) {
                        bmp280->stopConversion();
                    }
                    bmp280->reset();
                    delay(100);
                    bmp280->setTempOversampling(Oversampling::OVERSAMPLING_X2);
                    bmp280->setPresOversampling(Oversampling::OVERSAMPLING_X16);
                    bmp280->setIIRFilter(IIRFilter::IIR_FILTER_16);
                    bmp280->setTimeStandby(TimeStandby::TIME_STANDBY_4000MS);
                    bmp280->startNormalConversion();
                    delay(100);
                    doReset = false;
                }
                if(bmp280->getTemperature(localTemp)) {
                    reads++;
                    doReset = true;
                    read = reads > 1;
                    Serial.print(" LT - OK");
                    Serial.print(localTemp);
                    Serial.print(" ");
                }
                else {
                    delay(1000);
                    Serial.print(" LTX");
                }
            }
            bmp280->stopConversion();
        }
        output = localTemp;
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