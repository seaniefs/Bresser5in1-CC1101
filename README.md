# Bresser5in1-CC1101
Project to read data from a Bresser5-in-1 Weather Station using an ESP32 and CC1101 module.

The Bresser 5-in-1 Weather Stations seem to use two different protocols. Select the appropriate decoder by (un-)commenting `#define BRESSER_6_IN_1` in the source code.

| Model         | Decoder Function                |
| ------------- | ------------------------------- |
| 7002510..12   | decodeBresser**5In1**Payload()  |
| 7902510..12   | decodeBresser**5In1**Payload()  |
| 7002585       | decodeBresser**6In1**Payload()  |
