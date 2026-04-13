#include "StepCounter.h"
#include <Wire.h>

const int8_t BMI160_ADDR = 0x68; 

RTC_DATA_ATTR int stepCount = 0; 
RTC_DATA_ATTR uint16_t lastBmiStepReading = 0; 
RTC_DATA_ATTR bool bmiIsAsleep = false;

// --- BARE METAL I2C ---
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BMI160_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint16_t readRegister16(uint8_t reg) {
  Wire.beginTransmission(BMI160_ADDR);
  Wire.write(reg);
  // false sends a repeated start, standard for I2C register reads
  if (Wire.endTransmission(false) != 0) { 
    return 65535; // I2C Error
  }
  Wire.requestFrom((uint8_t)BMI160_ADDR, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    return (msb << 8) | lsb;
  }
  return 65535; // Error
}

void setupStepCounter(bool isColdBoot) {
  if (isColdBoot) {
    Serial.println("Cold Boot: Factory resetting BMI160 via direct registers...");
    
    // 1. Soft Reset 
    writeRegister(0x7E, 0xB6); 
    delay(100); 
  
    // 2. Accelerometer to Normal Mode 
    writeRegister(0x7E, 0x11); 
    delay(50); // Wait for the analog circuitry to settle

    // 3. Configure and Enable Step Counter 
    // Normal mode config: 0x15 to 0x7A, 0x03 to 0x7B
    // Plus enable bit (bit 3) in 0x7B -> 0x03 | 0x08 = 0x0B
    writeRegister(0x7A, 0x15); 
    writeRegister(0x7B, 0x0B); 

    // 4. Reset internal hardware step counter 
    writeRegister(0x7E, 0xB2); 
    delay(10);
    
    lastBmiStepReading = 0;
    Serial.println("BMI160 Bare-Metal Engine Ready!");
  } else {
    // Waking from sleep
    if (bmiIsAsleep) {
        // Only send this if it was ACTUALLY asleep
        writeRegister(0x7E, 0x11); 
        delay(10); 
        bmiIsAsleep = false;
        Serial.println("Woke from sleep: BMI160 Restored to Normal Mode.");
    } else {
        // Do absolutely nothing. Leave the step pipeline intact
        Serial.println("Woke from sleep: Sensor already running flawlessly in background.");
    }
  }
}

void updateStepCounter() {
  // Read exactly from the hardware STEP_CNT registers (0x78 and 0x79)
  uint16_t currentBmiSteps = readRegister16(0x78); 
  
  if (currentBmiSteps == 65535 || currentBmiSteps == 0xFFFF) {
     return; 
  }

  if (currentBmiSteps < lastBmiStepReading) {
    lastBmiStepReading = currentBmiSteps; 
  }

  int newStepsTaken = currentBmiSteps - lastBmiStepReading;

  if (newStepsTaken > 0) {
    stepCount += newStepsTaken;
    lastBmiStepReading = currentBmiSteps; 
    Serial.print("New Steps Detected! Total Steps: ");
    Serial.println(stepCount);
  }
}

void resetSteps() {
  stepCount = 0; 
  writeRegister(0x7E, 0xB2); // Clear hardware step counter
  lastBmiStepReading = 0;
  Serial.println("Steps manually reset to 0.");
}

void sleepStepCounter(bool isNight) {
  if (isNight) {
    // 0x10 commands the Accelerometer to enter LOW POWER MODE
    bmiIsAsleep = true;
    writeRegister(0x7E, 0x12); 
    Serial.println("Deep Sleep: BMI160 in Low Power Mode (~15uA)");
  } else {
    // Leave it awake to track steps during the day
    bmiIsAsleep = false;
    Serial.println("Deep Sleep: Leaving BMI160 in Normal Mode (~180uA).");
  }
}