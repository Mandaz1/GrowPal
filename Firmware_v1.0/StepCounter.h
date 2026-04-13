#ifndef STEP_COUNTER_H
#define STEP_COUNTER_H

#include <Arduino.h>

// Expose the step count to the main sketch so the OLED can display it
extern int stepCount; 

// Core functions
void setupStepCounter(bool isColdBoot);
void updateStepCounter();
void resetSteps();
void sleepStepCounter(bool isNight);
#endif