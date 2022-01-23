#include "WeatherDataBuffer.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>

// TODO: Rationalised version - 8 bits free for "reserved" data
// could record pressure change in 2 bits rising, falling, steady leaving 6 bits
// Pressure trend - CC      = 00 - Awaiting 01 - Up 10 - Down 11 - Steady
//                  IIIIIII = 7-bit ID (0-127)
//                  WWW     = Weather Type 
//                            0 - Unknown  1 - Sunny  2 - Sunny Cloudy
//                            3 - Cloudy   4 - Rainy  5 - Worsening
/*
	   00       01       02       03		
	76543210 76543210 76543210 76543210		
00	YYYYYYYM MMMDDDDD HHHHHMMM MMMSSSSS		
01  TTTTTTTT 1111RRRR RRRR1111 PPPPPPPP
02  PPP11112 222HHHHH HHGGGGGG G1111SSS
03  SSSS1111 DDDDCC-- IIIIIIII WWWWWWWW
*/

// TODO: Consider only recording 2DP of rain and use other 4 bits to allow 12 bits of rain in mm to be recorded
//       or weather type in the 4 bits - 
/*
	   00       01       02       03		
	76543210 76543210 76543210 76543210		
00	YYYYYYYM MMMDDDDD HHHHHMMM MMMSSSSS		
01  TTTTTTTT 11112222 RRRRRRRR 11112222
02  3333PPPP PPPPPPP1 1112222H HHHHHHGG
03  GGGGG111 12222SSS SSSS1111 2222DDDD
*/
const int entrySize = 16; // 16 bytes per cached entry
const int totalEntries = 20 * 6 * 24; // Allow up to 20 days of entries to be stored at 6 readings an hour, 24 hours a day
// Add 128 to temperatures so as we don't have to deal with -ve items - can be
// -128 to 127
const int temperatureBias = 128;
// Wind direction is stored at 16 points (22.5 degree divisions) rather than true because it's close enough - round up
// to nearest division.
const float windDirectionDivision = 360.0 / 16;
const float windDirectionRounding = windDirectionDivision / 2.0;
static uint8_t weatherDataEntryArray[totalEntries][entrySize] = { 0 };

// Should not be needed but just in case we use 1 thread to read and another to write
static SemaphoreHandle_t _lock;

// Circular buffer
static volatile int itemsInUse = 0;
static volatile int readPosition = 0;
static volatile int writePosition = 0;

// Keep track of rain in mm since it's a cumulative total over time.
static volatile float    startOfDayRainMm = 0;
static volatile uint32_t startOfDayRainMmDay = 0;

static void internalLimitWeatherDataIndexes() {
    if(readPosition >= totalEntries) {
        readPosition = 0;
    }
    if(writePosition >= totalEntries) {
        writePosition = 0;
    }
}

static uint32_t floatToInt32(float inputValue, uint32_t bias) {
    double intValue = 0;
    double fracValue = modf(inputValue + bias, &intValue);
    uint32_t output = intValue;
    //printf("%f\n", intValue);
    for(int i = 0 ; i < 4 ; i++) {
        fracValue *= 10.0;
        fracValue = modf(fracValue, &intValue);
        output <<= 4;
        output |= (0xF & (int32_t)intValue);
    } 
    return output;
}

static float int32ToFloat(uint32_t inputValue, uint32_t bias) {
    float output = 0;
    for(int i = 0 ; i < 4 ; i++) {
        output += inputValue & 0xF;
        inputValue >>= 4;
        output /= 10.0;
    }
    output += inputValue;
    output -= bias;
    return output;
}

/* Original
static void internalDecodeWeatherDataEntry(WeatherDataEntry *pDataEntry, uint8_t *pSourceData) {
    // 00	YYYYYYYM MMMDDDDD HHHHHMMM MMMSSSSS		
    uint32_t *pSourceDataArray = (uint32_t *)pSourceData;
    uint32_t packedTime = *pSourceDataArray;
    pSourceDataArray++;
    pDataEntry->second = (packedTime & 0x1F) * 2;
    packedTime >>= 5;
    pDataEntry->minute = (packedTime & 0x3F);
    packedTime >>= 6;
    pDataEntry->hour = (packedTime & 0x1F);
    packedTime >>= 5;
    pDataEntry->day = (packedTime & 0x1F);
    packedTime >>= 5;
    pDataEntry->month = (packedTime & 0xF);
    packedTime >>= 4;
    pDataEntry->year = (packedTime & 0x7F);

    // 01 TTTTTTTT 11112222 RRRRRRRR 11112222
    uint32_t packedTempAndRain = *pSourceDataArray;
    pSourceDataArray++;

    uint32_t tempPacked = (packedTempAndRain >> 8) & 0xFFFF00;
    pDataEntry->weatherData.temp_c = int32ToFloat(tempPacked, temperatureBias);
    uint32_t rainfallPacked = (packedTempAndRain << 8) & 0xFFFF00;
    // 02 3333PPPP PPPPPPP1 1112222H HHHHHHGG
    uint32_t packedPressureAndHumidity = *pSourceDataArray;
    pSourceDataArray++;

    rainfallPacked |= (packedPressureAndHumidity >> 28) & 0xF;
    pDataEntry->weatherData.rain_mm = int32ToFloat(rainfallPacked, 0);

    uint32_t pressurePacked = (packedPressureAndHumidity >> 1) & 0x7FFFF00;
    pDataEntry->weatherData.pressure = int32ToFloat(pressurePacked, 0);

    uint32_t humidityPercent = (packedPressureAndHumidity >> 2) & 0x7F;
    humidityPercent = humidityPercent > 100 ? 100 : humidityPercent;

    pDataEntry->weatherData.humidity = humidityPercent;
    uint32_t gustPacked = packedPressureAndHumidity & 0x3;

    // 03 GGGGG111 12222SSS SSSS1111 2222DDDD
    uint32_t packedWindData = *pSourceDataArray;
    pSourceDataArray++;

    gustPacked <<= 21;
    gustPacked |= (packedWindData >> 11) & 0x1FFF00;
    pDataEntry->weatherData.wind_gust_meter_sec = int32ToFloat(gustPacked, 0);

    uint32_t windPacked = (packedWindData & 0x7FFF0) << 4;
    pDataEntry->weatherData.wind_avg_meter_sec = int32ToFloat(windPacked, 0);
    float windDirection = (packedWindData & 0xF) * windDirectionDivision;
    pDataEntry->weatherData.wind_direction_deg = windDirection;
}

static void internalEncodeWeatherDataEntry(WeatherDataEntry *pDataEntry, uint8_t *pDestData) {
    // 00	YYYYYYYM MMMDDDDD HHHHMMMM MMSSSSSS
    uint32_t packedTime = 0;
    packedTime |= (pDataEntry->year & 0x7F);
    packedTime <<= 4;
    packedTime |= (pDataEntry->month & 0xF);
    packedTime <<= 5;
    packedTime |= (pDataEntry->day & 0x1F);
    packedTime <<= 5;
    packedTime |= (pDataEntry->hour & 0x1F);
    packedTime <<= 6;
    packedTime |= (pDataEntry->minute & 0x3F);
    packedTime <<= 5;
    packedTime |= (pDataEntry->second / 2) & 0x1F;

    // 01 TTTTTTTT 11112222 RRRRRRRR 11112222
    // 02 3333PPPP PPPPPPP1 1112222H HHHHHHGG
    // 03 GGGGG111 12222SSS SSSS1111 2222DDDD
    uint32_t tempPacked = floatToInt32(pDataEntry->weatherData.temp_c, temperatureBias);
    tempPacked &= 0xFFFFFF;
    tempPacked >>= 8;
    uint32_t rainfallPacked = (floatToInt32(pDataEntry->weatherData.rain_mm, 0) >> 4) & 0xFFFFF;
    uint32_t pressurePacked = (floatToInt32(pDataEntry->weatherData.pressure, 0) >> 8) & 0x7FFFF;
    uint32_t humidityPercent = pDataEntry->weatherData.humidity;
    humidityPercent = humidityPercent > 100 ? 100 : humidityPercent;

    uint32_t windGustPacked = (floatToInt32(pDataEntry->weatherData.wind_gust_meter_sec, 0) >> 8) & 0x7FFF;
    uint32_t windSpeedPacked = (floatToInt32(pDataEntry->weatherData.wind_avg_meter_sec, 0) >> 8) & 0x7FFF;
    float    windDirectionRounded = (pDataEntry->weatherData.wind_direction_deg) + windDirectionRounding;
    windDirectionRounded = fmod(windDirectionRounded, 360);
    windDirectionRounded /= windDirectionDivision;
    uint32_t windDirectionPacked = (uint32_t)windDirectionRounded;
    uint32_t tempAndRainfallPacked = tempPacked << 16;
    tempAndRainfallPacked |= (rainfallPacked >> 4) & 0xFFFF;
    uint64_t otherData = 0;
    otherData = rainfallPacked & 0xF;
    otherData <<= 19;
    otherData |= pressurePacked;
    otherData <<= 7;
    otherData |= humidityPercent;
    otherData <<= 15;
    otherData |= windGustPacked;
    otherData <<= 15;
    otherData |= windSpeedPacked;
    otherData <<= 4;
    otherData |= windDirectionPacked;
    uint32_t *pOutputData = (uint32_t *)pDestData;
    pOutputData[0] = packedTime;
    pOutputData[1] = tempAndRainfallPacked;
    pOutputData[3] = otherData & 0xFFFFFFFF;
    pOutputData[2] = (otherData >> 32) & 0xFFFFFFFF;
}
*/

static void internalDecodeWeatherDataEntry(WeatherDataEntry *pDataEntry, uint8_t *pSourceData) {
    // 00	YYYYYYYM MMMDDDDD HHHHHMMM MMMSSSSS
    uint32_t *pSourceDataArray = (uint32_t *)pSourceData;
    uint32_t packedTime = *pSourceDataArray;
    pSourceDataArray++;
    pDataEntry->second = (packedTime & 0x1F) * 2;
    packedTime >>= 5;
    pDataEntry->minute = (packedTime & 0x3F);
    packedTime >>= 6;
    pDataEntry->hour = (packedTime & 0x1F);
    packedTime >>= 5;
    pDataEntry->day = (packedTime & 0x1F);
    packedTime >>= 5;
    pDataEntry->month = (packedTime & 0xF);
    packedTime >>= 4;
    pDataEntry->year = (packedTime & 0x7F);

    // 01  TTTTTTTT 1111RRRR RRRR1111 PPPPPPPP
    uint32_t packedTempAndRain = *pSourceDataArray;
    pSourceDataArray++;

    uint32_t tempPacked = (packedTempAndRain >> 8) & 0xFFF000;
    pDataEntry->weatherData.temp_c = int32ToFloat(tempPacked, temperatureBias);
    uint32_t rainfallPacked = (packedTempAndRain << 4) & 0xFFF000;
    pDataEntry->weatherData.rain_mm = int32ToFloat(rainfallPacked, 0);

    // 02  PPP11112 222HHHHH HHGGGGGG G1111SSS
    uint32_t packedPressure = (packedTempAndRain & 0xFF);
    packedPressure <<= 19;
    packedPressure |= ((*pSourceDataArray) >> 13) & 0x0007FF00;
    pDataEntry->weatherData.pressure = int32ToFloat(packedPressure, 0);

    uint32_t humidityPercent = ((*pSourceDataArray) >> 14) & 0x7F;
    humidityPercent = humidityPercent > 100 ? 100 : humidityPercent;
    pDataEntry->weatherData.humidity = humidityPercent;

    uint32_t gustPacked = ((*pSourceDataArray) & 0x3FF8) << 9;
    pDataEntry->weatherData.wind_gust_meter_sec = int32ToFloat(gustPacked, 0);

    uint32_t speedPacked = ((*pSourceDataArray) << 20) & 0x700000;
    pSourceDataArray++;
    // Pressure trend and id not yet encoded so not decoded.
    // 03 SSSS1111 DDDDCC-- IIIIIIII WWWWWWWW
    speedPacked |= ((*pSourceDataArray) >> 12) & 0xFF000;
    pDataEntry->weatherData.wind_avg_meter_sec = int32ToFloat(speedPacked, 0);

    uint32_t windDirectionEncoded = ((*pSourceDataArray) & 0xF00000) >> 20;
    float windDirection = windDirectionEncoded * windDirectionDivision;
    pDataEntry->weatherData.wind_direction_deg = windDirection;

    pDataEntry->weatherData.forecast = (*pSourceDataArray) & 0xFF;
}

static void internalEncodeWeatherDataEntry(WeatherDataEntry *pDataEntry, uint8_t *pDestData) {
    // 00	YYYYYYYM MMMDDDDD HHHHMMMM MMSSSSSS
    uint32_t packedTime = 0;
    packedTime |= (pDataEntry->year & 0x7F);
    packedTime <<= 4;
    packedTime |= (pDataEntry->month & 0xF);
    packedTime <<= 5;
    packedTime |= (pDataEntry->day & 0x1F);
    packedTime <<= 5;
    packedTime |= (pDataEntry->hour & 0x1F);
    packedTime <<= 6;
    packedTime |= (pDataEntry->minute & 0x3F);
    packedTime <<= 5;
    packedTime |= (pDataEntry->second / 2) & 0x1F;

    // 01 TTTTTTTT 1111RRRR RRRR1111 PPPPPPPP
    uint32_t firstWord = 0;
    uint32_t tempPacked = floatToInt32(pDataEntry->weatherData.temp_c, temperatureBias);
    firstWord |= (tempPacked >> 12) & 0xFFF;
    firstWord <<= 12;
    firstWord |= (floatToInt32(pDataEntry->weatherData.rain_mm, 0) >> 12) & 0xFFF;
    firstWord <<= 8;
    uint32_t pressurePacked = floatToInt32(pDataEntry->weatherData.pressure, 0);
    //printf("%08X\n", pressurePacked);
    firstWord |= (pressurePacked >> 19) & 0xFF;

    // 02 PPP11112 222HHHHH HHGGGGGG G1111SSS
    uint32_t secondWord = ((pressurePacked >> 8) & 0x7FF) << 21;

    uint32_t humidityPercent = pDataEntry->weatherData.humidity;
    humidityPercent = humidityPercent > 100 ? 100 : humidityPercent;

    secondWord |= (humidityPercent & 0x7F) << 14;

    uint32_t windGustPacked = floatToInt32(pDataEntry->weatherData.wind_gust_meter_sec, 0);
    secondWord |= (windGustPacked >> 9) & 0x3FF8;

    uint32_t windSpeedPacked = floatToInt32(pDataEntry->weatherData.wind_avg_meter_sec, 0);
    secondWord |= (windSpeedPacked >> 20) & 0x7;

    // 03 SSSS1111 DDDDCC-- IIIIIIII WWWWWWWW
    uint32_t thirdWord = (windSpeedPacked << 12) & 0xFF000000;

    float    windDirectionRounded = (pDataEntry->weatherData.wind_direction_deg) + windDirectionRounding;
    windDirectionRounded = fmod(windDirectionRounded, 360);
    windDirectionRounded /= windDirectionDivision;
    uint32_t windDirectionPacked = (uint32_t)windDirectionRounded;
    thirdWord |= (windDirectionPacked << 20) & 0xF00000;
    thirdWord |= (pDataEntry->weatherData.forecast & 0xFF);

    uint32_t *pOutputData = (uint32_t *)pDestData;
    pOutputData[0] = packedTime;
    pOutputData[1] = firstWord;
    pOutputData[2] = secondWord;
    pOutputData[3] = thirdWord;
}

static bool internalReadNextWeatherDataEntry(WeatherDataEntry *pDataEntry, bool advancePointer) {
    bool itemRead = false;
    if(itemsInUse > 0) {
        itemRead = true;
        internalDecodeWeatherDataEntry(pDataEntry, &weatherDataEntryArray[readPosition][0]);
        if (advancePointer) {
            itemsInUse -= 1;
            readPosition += 1;
            internalLimitWeatherDataIndexes();
        }
    }
    return itemRead;
}

void initWeatherDataBuffer() {
    _lock = xSemaphoreCreateMutex();
}

bool peekNextWeatherDataEntry(WeatherDataEntry *pDataEntry, uint32_t *pEntryRead) {
    bool itemRead = false;

    if(xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
        itemRead = internalReadNextWeatherDataEntry(pDataEntry, false);
        if (itemRead) {
            *pEntryRead = readPosition;
        }
        xSemaphoreGive(_lock);
    }
    else {
        Serial.printf("Failed to acquire semaphore in: %s\n", __func__);
    }

    return itemRead;
}

void confirmPeekWeatherDataEntry(uint32_t entryRead) {
    if(xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
        if ( entryRead != 0xFFFFFFFF 
             && readPosition == entryRead ) {
            WeatherDataEntry tempEntry;
            internalReadNextWeatherDataEntry(&tempEntry, true);
        }
        xSemaphoreGive(_lock);
    }
    else {
        Serial.printf("Failed to acquire semaphore in: %s\n", __func__);
    }
}

void appendWeatherDataEntry(WeatherData weatherData) {
    if(xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
        // If full then remove last entry...
        if(itemsInUse == totalEntries) {
            WeatherDataEntry dummyEntry;
            internalReadNextWeatherDataEntry(&dummyEntry, true);
        }
        WeatherDataEntry tempEntry;
        time_t t = time(NULL);
        struct tm tm = *gmtime(&t);
        tempEntry.day = (uint8_t) tm.tm_mday;
        tempEntry.month = (uint8_t) tm.tm_mon + 1;   // 0 is January
        tempEntry.year = (uint8_t) tm.tm_year - 100; // years since 1900
        tempEntry.hour = (uint8_t) tm.tm_hour;
        tempEntry.minute = (uint8_t) tm.tm_min;
        tempEntry.second = (uint8_t) tm.tm_sec;
        tempEntry.weatherData = weatherData;
        Serial.printf("Timestamp: %02d/%02d/%02d %02d:%02d:%02d\n", tempEntry.year, tempEntry.month, tempEntry.day, tempEntry.hour, tempEntry.minute, tempEntry.second);
        // The rain gauge is a continuous count which increments, so each day we need to
        // subtract the number of mm to get the total per day.
        // Do this by keeping the 1st reading for each day and then subtracting this from other readings,
        // unless the result would be -ve in which case use that instead.
        uint32_t rainMmDay = (tempEntry.year * 10000) + (tempEntry.month * 100) + tempEntry.day;
        if (rainMmDay != startOfDayRainMmDay 
            || weatherData.rain_mm < startOfDayRainMm) {
            startOfDayRainMm = weatherData.rain_mm;
            startOfDayRainMmDay = rainMmDay;
            Serial.printf("Resetting rain mm for: %06d relative to %.2f\n", startOfDayRainMmDay, startOfDayRainMm);
        }
        tempEntry.weatherData.rain_mm -= startOfDayRainMm;
        internalEncodeWeatherDataEntry(&tempEntry, &weatherDataEntryArray[writePosition][0]);
        itemsInUse++;
        writePosition++;
        internalLimitWeatherDataIndexes();
        xSemaphoreGive(_lock);
    }
    else {
        Serial.printf("Failed to acquire semaphore in: %s\n", __func__);
    }
}
int getTotalWeatherDataEntries() {
    return itemsInUse;
}
bool readNextWeatherDataEntry(WeatherDataEntry *pDataEntry) {
    bool itemRead = false;

    if(xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
        itemRead = internalReadNextWeatherDataEntry(pDataEntry, true);
        xSemaphoreGive(_lock);
    }
    else {
        Serial.printf("Failed to acquire semaphore in: %s\n", __func__);
    }

    return itemRead;
}
bool clearAllWeatherDataEntries() {
    if(xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
        itemsInUse = 0;
        readPosition = 0;
        writePosition = 0;
        xSemaphoreGive(_lock);
    }
    else {
        Serial.printf("Failed to acquire semaphore in: %s\n", __func__);
    }
    return true;
}
