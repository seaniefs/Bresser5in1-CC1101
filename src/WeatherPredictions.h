#ifndef _WEATHERPREDICTOR_H_

    #define _WEATHERPREDICTOR_H_

    typedef struct CastOutput {
        bool        ready;
        int         output;
        const char *forecastText;
        bool        extremeWeatherForecast;
    } CastOutput;

    typedef enum Hemisphere {
        NORTH, SOUTH
    } Hemisphere;

    typedef enum WindDirection {
        N = 0,
        NNE = 1,
        NE = 2,
        ENE = 3,
        E = 4,
        ESE = 5,
        SE = 6,
        SSE = 7,
        S = 8,
        SSW = 9,
        SW = 10,
        WSW = 11,
        W = 12,
        WNW = 13,
        NW = 14,
        NNW = 15,
        CALM = 16
    } WindDirection;

    typedef enum PressureTrend {
        STEADY = 0, UP = 1, DOWN = 2, UNKNOWN = 3
    } PressureTrend;

    void initWeatherPredictionsBuffer(bool initialBoot);

    //
    // TODO: Setup to take elevation in meters/Hemisphere from #define or flash also upper/lower pressure bounds
    //
    // Convert a RAW pressure reading in HPA using current temperature
    // to convert into sea-level normalized pressure
    float altitudeNormalizedPressure(float pressureInHpa, float tempInC);

    // Record a sea-level normalized pressure reading
    void recordPressureReading(float pressureInHpa);

    WindDirection degreesToWindDirection(float windDir);

    // Generate a forecast given month of year 1-12 wind direction 0-359 and where Hemisphere:NORTH/SOUTH
    // returns true if forecast generated else false (awaiting more data)
    bool generateForecast(CastOutput& castOutput, int month,
                          WindDirection windDir, Hemisphere where);

#endif