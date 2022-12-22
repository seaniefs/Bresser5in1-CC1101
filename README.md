> :warning: This repository is deprecated and no longer maintained. 
> The recommended alternative is [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver)
> which provides a much cleaner design and allows to use an SX1276 or RFM95W radio receiver as an alternative to the CC1101.

# Bresser5in1-CC1101
Project to read data from a Bresser5-in-1 Weather Station using an ESP32 and CC1101 module.

The Bresser 5-in-1 Weather Stations seem to use two different protocols. Select the appropriate decoder by (un-)commenting `#define BRESSER_6_IN_1` in the source code.

| Model         | Decoder Function                |
| ------------- | ------------------------------- |
| 7002510..12   | decodeBresser**5In1**Payload()  |
| 7902510..12   | decodeBresser**5In1**Payload()  |
| 7002585       | decodeBresser**6In1**Payload()  |
