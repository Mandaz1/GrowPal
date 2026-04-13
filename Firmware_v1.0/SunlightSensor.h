#ifndef SUNLIGHT_SENSOR_H
#define SUNLIGHT_SENSOR_H

#include <Arduino.h>

extern RTC_DATA_ATTR int sunlightSeconds; // Survives deep sleep
extern bool isCurrentlySunny; // Live state for the OLED bitmap
extern float current_mA;
extern float loadvoltage;
void setupSunlightSensor();
void updateSunlightSensor();
void sleepSunlightSensor();
void resetSunlight();

#endif