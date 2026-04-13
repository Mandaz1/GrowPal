#include "SunlightSensor.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

RTC_DATA_ATTR int sunlightSeconds = 0; 
bool isCurrentlySunny = false; 
unsigned long lastSunReadTime = 0;
float shuntvoltage = 0;
float busvoltage = 0;
float loadvoltage = 0;
float current_mA = 0;
// Based on 100-ohm resistor tests:
// < 2mA = Indoor lighting
// > 10mA = Cloudy outdoor / Direct sunlight
const float SUN_THRESHOLD_MA = 10.0; 

void setupSunlightSensor() {
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip. Check wiring!");
    return;
  }
  
  // Boost sensitivity to cleanly read the tiny solar panel currents
  ina219.setCalibration_16V_400mA();
  Serial.println("INA219 Sunlight Sensor Ready!");
  
  // Put immediately to sleep (drops power draw from 1mA to 6 micro-amps)
  ina219.powerSave(true); 
}

void updateSunlightSensor() {
  if (millis() - lastSunReadTime < 1000) {
    return; 
  }

  // 1. Wake the sensor up
  ina219.powerSave(false);
  delay(2); // Give the ADC 2ms to stabilize

  // 2. Take ALL readings while awake
  current_mA = ina219.getCurrent_mA();
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  loadvoltage = busvoltage + (shuntvoltage / 1000); 

  // 3. Put immediately back to sleep
  ina219.powerSave(true);

  // 4. Update Logic
  if (current_mA >= 10.0) { 
    isCurrentlySunny = true;
    sunlightSeconds++;
    Serial.print("Sunlight detected! Total seconds: ");
    Serial.println(sunlightSeconds);
  } else {
    isCurrentlySunny = false;
  }

  // Reset the timer for the next second
  lastSunReadTime = millis();
}


void sleepSunlightSensor() {
  // A safety catch called right before the ESP32 goes to Deep Sleep
  ina219.powerSave(true);
}

void resetSunlight() {
  sunlightSeconds = 0;
  Serial.println("Sunlight time reset to 0.");
}