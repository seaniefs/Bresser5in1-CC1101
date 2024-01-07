#ifndef _PRESSURE_SENSOR_H_
  #define _PRESSURE_SENSOR_H_ 1

  bool pressureSensorAvailable();
  bool initPressureSensor();
  bool readPressureSensorHpa(float& output);
  bool readPressureSensorTemp(float& output);
  bool readPressureSensorHpaAndTemp(float& hpaOutput, float &tempOutput);
  void closePressureSensor();
#endif