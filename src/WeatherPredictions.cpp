#include <math.h>
#include <stdio.h>
#include "WeatherPredictions.h"

/* Testing
#ifndef _HEMISPHERE_
    #define _HEMISPHERE_ NORTH
#endif

#ifndef ALTITUDE_IN_METRES
  #define ALTITUDE_IN_METRES 64.0
#endif

#ifndef BAROMETER_LOW_HPA
  #define BAROMETER_LOW_HPA 950.0
#endif

#ifndef BAROMETER_HIGH_HPA
  #define BAROMETER_HIGH_HPA 1050.0
#endif
*/

// trend - computed over 3 hours with 1.6hPa change
//
// So - for this we need to keep an array of 18 floats which hold hPa
// when we get to 3 hours we can average the last and first half-hours and
// then use that to determine the trend

// NOTE: Assumes 6 readings an hour; naive and needs to be configurable!
const int        normalizedPressureArrayItems = 18;
static float     normalizedPressureArray[normalizedPressureArrayItems] = { 0 };
static int       nextWritingIndex = 0;
static int       recordedReadings = 0;

// ---- 'environment' variables ------------
const float ukBaroTop = BAROMETER_HIGH_HPA;	    // upper limits of your local 'weather window' (1050.0 hPa for UK)
const float ukBaroBottom = BAROMETER_LOW_HPA;	// lower limits of your local 'weather window' (950.0 hPa for UK)

static const char *forecastText[] = { 
    "Settled fine", "Fine weather", "Becoming fine",
    "Fine, becoming less settled", "Fine, possible showers", "Fairly fine, improving",
    "Fairly fine, possible showers early", "Fairly fine, showery later", "Showery early, improving",
    "Changeable, mending", "Fairly fine, showers likely", "Rather unsettled clearing later",
    "Unsettled, probably improving", "Showery, bright intervals", "Showery, becoming less settled",
    "Changeable, some rain", "Unsettled, short fine intervals", "Unsettled, rain later",
    "Unsettled, some rain", "Mostly very unsettled", "Occasional rain, worsening",
    "Rain at times, very unsettled", "Rain at frequent intervals", "Rain, very unsettled",
    "Stormy, may improve", "Stormy, much rain"
};

// equivalents of Zambretti 'dial window' letters A - Z
static const int rise_options[] = {
    25,25,25,24,24,19,16,12,11,9,8,6,5,2,1,1,0,0,0,0,0,0
};
static const int steady_options[] = {
    25,25,25,25,25,25,23,23,22,18,15,13,10,4,1,1,0,0,0,0,0,0
};

static const int fall_options[] = {
    25,25,25,25,25,25,25,25,23,23,21,20,17,14,7,3,1,1,1,0,0,0
};

float altitudeNormalizedPressure(float pressureInHpa, float tempInC) {
    float altitudeMultiplier = (0.0065 * ALTITUDE_IN_METRES);
    float normalizedPressure = pressureInHpa * pow(1 - altitudeMultiplier / (tempInC + altitudeMultiplier + 273.15), -5.257);
    return normalizedPressure;
}

void recordPressureReading(float normalizedPressure) {
  normalizedPressureArray[nextWritingIndex++] = normalizedPressure;
  nextWritingIndex %= normalizedPressureArrayItems;
  if (nextWritingIndex >= normalizedPressureArrayItems) {
    nextWritingIndex = 0;
  }
  if (recordedReadings < normalizedPressureArrayItems) {
    recordedReadings += 1;
  }
}

float getLastPressureReading() {
    if(recordedReadings == 0) {
        return -1.0;
    }
    int previousItem = nextWritingIndex - 1;
    if (previousItem < 0) {
        previousItem = normalizedPressureArrayItems - 1;
    }
    return normalizedPressureArray[previousItem];
}

// 0 - Steady 1 - Rise 2 - Fall 3 - Unknown
PressureTrend calculatePressureTrend() {
    if (recordedReadings < normalizedPressureArrayItems) {
        return PressureTrend::UNKNOWN;
    }
    const int averagedItems = 3;
    int initialReadIndex = nextWritingIndex;
    int finalReadIndex = nextWritingIndex - 1;
    float initialPressure = normalizedPressureArray[initialReadIndex];
    if (finalReadIndex < 0) {
        finalReadIndex += normalizedPressureArrayItems;
    }
    float finalPressure = normalizedPressureArray[finalReadIndex];
    float difference = finalPressure - initialPressure;
    if (difference <= -1.6) {
        return PressureTrend::DOWN;
    }
    else if (difference >= 1.6) {
        return PressureTrend::UP;
    }
    return PressureTrend::STEADY;
}

// C/C++ Conversion of:
// beteljuice.com - near enough Zambretti Algorithm 
// June 2008 - v1.0
CastOutput betel_cast( float normalizedHpa, int month,
                       WindDirection windDir, PressureTrend pressureTrend,
                       Hemisphere where, float baroTop, float baroBottom) {
	const float baroRange = baroTop - baroBottom;
	const float baroConstant = ((int)((baroRange / 22.0) * 1000)) / 1000.0;
    const bool isSummer = (month >= 4 && month <= 9) ; 	// true if 'Summer'
	if (where == Hemisphere::NORTH) {  		// North hemisphere
        switch(windDir) {
            case WindDirection::N:
			    normalizedHpa += 6 / 100.0 * baroRange ;
                break;
            case WindDirection::NNE:
                normalizedHpa += 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::NE:
			    normalizedHpa += 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::ENE:
			    normalizedHpa += 2 / 100.0 * baroRange ;  
                break;
            case WindDirection::E:
			    normalizedHpa -= 0.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::ESE:
			    normalizedHpa -= 2 / 100.0 * baroRange ;  
                break;
            case WindDirection::SE:
			    normalizedHpa -= 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::SSE:
		    	normalizedHpa -= 8.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::S:
	    		normalizedHpa -= 12 / 100.0 * baroRange ;  
                break;
            case WindDirection::SSW:
    			normalizedHpa -= 10 / 100.0 * baroRange ;  //
                break;
            case WindDirection::SW:
                normalizedHpa -= 6 / 100.0 * baroRange ;  
                break;
            case WindDirection::WSW:
                normalizedHpa -= 4.5 / 100.0 * baroRange ;  //
                break;
            case WindDirection::W:
                normalizedHpa -= 3 / 100.0 * baroRange ;  
                break;
            case WindDirection::WNW:
                normalizedHpa -= 0.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::NW:
                normalizedHpa += 1.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::NNW:
                normalizedHpa += 3 / 100.0 * baroRange ;  
                break;
		} 
		if (isSummer) {  	// if Summer
			if (pressureTrend == PressureTrend::UP) {  	// rising
				normalizedHpa += 7 / 100.0 * baroRange;  
			} else if (pressureTrend == PressureTrend::DOWN) {  //	falling
				normalizedHpa -= 7 / 100.0 * baroRange; 
			} 
		} 
	} else {  	// must be South hemisphere
        switch (windDir) {
            case WindDirection::S:
                normalizedHpa += 6 / 100.0 * baroRange ;  
                break;
            case WindDirection::SSW:
                normalizedHpa += 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::SW:
                normalizedHpa += 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::WSW:
                normalizedHpa += 2 / 100.0 * baroRange ;  
                break;
            case WindDirection::W:
                normalizedHpa -= 0.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::WNW:
                normalizedHpa -= 2 / 100.0 * baroRange ;  
                break;
            case WindDirection::NW:
                normalizedHpa -= 5 / 100.0 * baroRange ;  
                break;
            case WindDirection::NNW:
                normalizedHpa -= 8.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::N:
                normalizedHpa -= 12 / 100.0 * baroRange ;  
                break;
            case WindDirection::NNE:
                normalizedHpa -= 10 / 100.0 * baroRange ;  //
                break;
            case WindDirection::NE:
                normalizedHpa -= 6 / 100.0 * baroRange ;  
                break;
            case WindDirection::ENE:
                normalizedHpa -= 4.5 / 100.0 * baroRange ;  //
                break;
            case WindDirection::E:
                normalizedHpa -= 3 / 100.0 * baroRange ;  
                break;
            case WindDirection::ESE:
                normalizedHpa -= 0.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::SE:
                normalizedHpa += 1.5 / 100.0 * baroRange ;  
                break;
            case WindDirection::SSE:
                normalizedHpa += 3 / 100.0 * baroRange ;  
                break; 
        }
		if (!isSummer) { 	// if Winter
			if (pressureTrend == PressureTrend::UP) {  // rising
				normalizedHpa += 7 / 100.0 * baroRange;  
			} else if (pressureTrend == PressureTrend::DOWN) {  // falling
				normalizedHpa -= 7 / 100.0 * baroRange; 
			} 
		} 
	} 	// END North / South

	if(normalizedHpa == baroTop) {
        normalizedHpa = baroTop - 1;
    }
    CastOutput output = { 0 };
    output.ready = true;
    output.output = 0;
    output.forecastText = "";
    output.extremeWeatherForecast = false;
    int outputOption = floor((normalizedHpa - baroBottom) / baroConstant);
	if(outputOption < 0) {
		outputOption = 0;
		output.extremeWeatherForecast = true;
	}
	if(outputOption > 21) {
		outputOption = 21;
		output.extremeWeatherForecast = true;
	}

	if (pressureTrend == PressureTrend::UP) { 	// rising
		output.forecastText = forecastText[rise_options[outputOption]] ; 
        output.output = rise_options[outputOption];
	} else if (pressureTrend == PressureTrend::DOWN) { 	// falling
		output.forecastText = forecastText[fall_options[outputOption]] ; 
        output.output = fall_options[outputOption];
	} else { 	// must be 'steady'
		output.forecastText = forecastText[steady_options[outputOption]]; 
        output.output = steady_options[outputOption];
	} 
    return output;
}

CastOutput betel_cast( float normalizedHpa, int month,
                       WindDirection windDir, PressureTrend pressureTrend,
                       Hemisphere where) {
    return betel_cast(normalizedHpa, month, windDir, pressureTrend, where, ukBaroTop, ukBaroBottom);
}

WindDirection degreesToWindDirection(float windDir) {
    const float windDirectionDivision = 360.0 / 16;
    const float windDirectionRounding = windDirectionDivision / 2;
    float    windDirectionRounded = windDir + windDirectionRounding;
    windDirectionRounded = fmod(windDirectionRounded, 360);
    windDirectionRounded /= windDirectionDivision;
    WindDirection windDirection = static_cast<WindDirection>((int)windDirectionRounded);
    return windDirection;
}

bool generateForecast(CastOutput& castOutput, int month, float windDir, Hemisphere where) {
    WindDirection windDirection = degreesToWindDirection(windDir);
    return generateForecast(castOutput, month, windDirection, where);
}

bool generateForecast(CastOutput& castOutput, int month,
                      WindDirection windDir, Hemisphere where) {
    float lastPressureReading = getLastPressureReading();
    PressureTrend pressureTrend = calculatePressureTrend();
    if (lastPressureReading < 0 || pressureTrend == PressureTrend::UNKNOWN) {
        castOutput = { 0 };
        castOutput.ready = false;
    }
    else {
        castOutput = betel_cast(lastPressureReading, month, windDir, pressureTrend, where);
        castOutput.ready = true;
    }
    return castOutput.ready;
}

/* Testing
int main(int argc, char **argv) {
    float currentPressure = 1090.4;
    for(int i = 0 ; i < 180 ; i++) {
        float normalizedPressure = altitudeNormalizedPressure(currentPressure, 7.0);
        recordPressureReading(normalizedPressure);
        currentPressure -= 0.5;
        CastOutput castOutput = { 0 };
        if(!generateForecast(castOutput, 1, 200, Hemisphere::NORTH) || !castOutput.ready) {
            printf("Not enough data... %d\n", i);
        }
        else {
            PressureTrend pressureTrend = calculatePressureTrend();
            const char *pressureTrendText = pressureTrend == PressureTrend::UP ? "UP" : pressureTrend == PressureTrend::DOWN ? "DOWN" : pressureTrend == PressureTrend::STEADY ? "STEADY" : "?";
            printf("Forecast: %d %f %s %s%s\n", i, currentPressure, pressureTrendText, castOutput.extremeWeatherForecast ? "Extreme " : "", castOutput.forecastText);
        }
    }
    return 0;
}
*/