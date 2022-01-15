#include "PressureSensor.h"
#include <BMP280_DEV.h>

static bool pressureSensorInit = false;

#ifdef ENABLE_PRESSURE_SENSOR
    static BMP280_DEV bmp280(BMP280_PIN_SDA, BMP280_PIN_SCL);
    bool pressureSensorAvailable() {
        return true;
    }
    bool initPressureSensor() {
        closePressureSensor();
        pressureSensorInit = bmp280.begin(Mode::SLEEP_MODE,
                                          Oversampling::OVERSAMPLING_X2,
                                          Oversampling::OVERSAMPLING_X16, 
                                          IIRFilter::IIR_FILTER_16,
                                          TimeStandby::TIME_STANDBY_1000MS);
        return pressureSensorInit;
    }
    bool readPressureSensorHpa(float& output) {
        bool  read = false;
        float localPressure = 0;
        if (pressureSensorInit) {
            bmp280.startForcedConversion();
            // Wait for up to 5 seconds to get a reading...
            for(int i = 0 ; i < 20 && read == false ; i++) {
                if(bmp280.getPressure(localPressure)) {
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
            bmp280.reset();
            bmp280.stopConversion();
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