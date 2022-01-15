#ifndef _PRESSURE_SENSOR_H_
  #define _PRESSURE_SENSOR_H_ 1

  bool pressureSensorAvailable();
  bool initPressureSensor();
  bool readPressureSensorHpa(float& output);
  void closePressureSensor();
#endif