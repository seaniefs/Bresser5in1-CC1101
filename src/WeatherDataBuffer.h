#ifndef _WEATHER_DATA_BUFFER_H_

    #define _WEATHER_DATA_BUFFER_H_ 1

    #include "stdint.h"

    typedef struct WeatherData_S {
        uint8_t sensor_id;
        float   temp_c;
        int     humidity;
        float   wind_direction_deg;
        float   wind_gust_meter_sec;
        float   wind_avg_meter_sec;
        float   rain_mm;
        float   pressure;
        bool    battery_ok;
    } WeatherData;

    typedef struct WeatherDataEntry_S {
        uint8_t     year;
        uint8_t     month;
        uint8_t     day;
        uint8_t     hour;
        uint8_t     minute;
        uint8_t     second;
        WeatherData weatherData;
    } WeatherDataEntry;

    void initWeatherDataBuffer();
    void appendWeatherDataEntry(WeatherData weatherData);
    int  getTotalWeatherDataEntries();
    bool readNextWeatherDataEntry(WeatherDataEntry *pDataEntry);
    bool clearAllWeatherDataEntries();
    bool peekNextWeatherDataEntry(WeatherDataEntry *pDataEntry, uint32_t *pEntryRead);
    void confirmPeekWeatherDataEntry(uint32_t entryRead);

#endif