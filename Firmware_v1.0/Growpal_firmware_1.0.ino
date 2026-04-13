/*********************************************************************
  Project GrowPal - A gamified health companion

  This is version 1.0 of the embedded firmware of GrowPal
  
  Version 1.0 features:
    - Designed for the Seeed studio Xiao esp32-s3 plus
    - Libraries for INA219 and BMI160 sensors
    - UI elementes designed for SH1107 128x128 Oled display
    - Tunes and melodies using a passive buzzer
    - Navigable menues
    - Game logic
    - Step counting
    - Water intake reminders
    - Sunlight exposure
    - Notifications
    - Character progression assets
    - Deep sleep
    - Battery readout and optimization (1000 mAH)
    - Wifi connection (Portal and auto reconnection)
    - Online time sync
    - Data sync to firebase 

  Written by Muhammad Bagosher.
  
*********************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Battery.h>
#include "driver/rtc_io.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "time.h"
#include "esp_sntp.h"

Preferences preferences;
WiFiManager wm;

#include "StepCounter.h"
#include "SunlightSensor.h"
#include "Assets.h"


#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET -1     // Oled reset pin

Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 400000, 100000); // was 1000000

#define BAT_ADC  7      // ADC pin to read battery voltage
#define BUTTON_UP_PIN 9 // pin for UP button 
#define BUTTON_ENTER_PIN 8 // pin for Enter button
#define BUTTON_DOWN_PIN 3 // pin for DOWN button
#define BUTTON_BACK_PIN 2 // pin for BACK button

Battery battery(3000, 4200, BAT_ADC, 12); // 3.0 - 4.2 volt

// ---------- BATTERY VARIABLES ----------
float batteryVoltage = 0.0;
int batteryPercent = 0;
int displayPercent = 0;
unsigned long lastBatteryRead = 0;
bool showPercent = true;
unsigned long lastToggle = 0;

// ---------- ANIMATION VARIABLES ----------
unsigned long lastFrameTime = 0;
int currentFrame = 0;
const int totalFrames = 2;     
const int frameInterval = 400;  // ms between frames

//----------- UI VARIABLES -----------
const int NUM_ITEMS = 6; // number of items in the list
const int MAX_ITEM_LENGTH = 20; // maximum characters for the item name

char menu_items [NUM_ITEMS] [MAX_ITEM_LENGTH] = {  // array with item names
  { "Missions" }, 
  { "Drink Water" }, 
  { "Reset Steps" }, 
  { "Mini Games" },
  { "Go To Sleep" },
  { "Settings"  }
 };

int button_up_clicked = 0; // only perform action when button is clicked, and wait until another press
int button_enter_clicked = 0; // same as above
int button_down_clicked = 0; // same as above
int button_back_clicked = 0; // same as above
int item_selected = 0; // currently selected item in the menu
int item_sel_previous; // previous item - used in the menu screen to draw the item before the selected one
int item_sel_next; // next item - used in the menu screen to draw next item after the selected one
enum ScreenState {
  SCREEN_MAIN,
  SCREEN_SETTINGS,
  SCREEN_SLEEP_TIMER,
  SCREEN_SOUND,
  SCREEN_BRIGHTNESS,
  SCREEN_MISSIONS
};
ScreenState current_screen = SCREEN_MAIN;

//----------- SETTINGS VARIABLES -----------
const int NUM_SETTINGS = 4;
int settings_selected = 0;

char settings_items[NUM_SETTINGS][20] = {
  { "Sleep Timer" },
  { "Sound" },
  { "Brightness" },
  { "WiFi setup" }
};

//----------- SLEEP TIMER VARIABLES -----------
const int NUM_TIMEOUT_OPTIONS = 5;
int timeout_selected = 1;  // default 30s

unsigned long timeout_values[NUM_TIMEOUT_OPTIONS] = {
  15000,
  30000,
  60000,
  120000,
  300000
};

char timeout_labels[NUM_TIMEOUT_OPTIONS][20] = {
  { "15 sec" },
  { "30 sec" },
  { "1 min" },
  { "2 min" },
  { "5 min" }
};

//----------- SOUND VARIABLES -----------
const int NUM_SOUND_OPTIONS = 2;
int sound_selected = 0;

char sound_items[NUM_SOUND_OPTIONS][20] = {
  { "On" },
  { "Off" }
};

//----------- BRIGHTNESS VARIABLES -----------
uint8_t brightnessLevel = 1;   // 0,1,2
int brightness_selected = 1;
uint8_t brightnessBeforeEdit;
const int NUM_BRIGHTNESS_OPTIONS = 3;

char brightness_items[NUM_BRIGHTNESS_OPTIONS][20] = {
  { "Level 1" },
  { "Level 2" },
  { "Level 3" }
};

uint8_t brightness_values[NUM_BRIGHTNESS_OPTIONS] = {
  40,   // dim
  120,  // medium
  255   // max
};

//----------- BUZZER VARIABLES -----------
#define BUZZER_PIN 4  
#define BUZZER_CHANNEL 0
#define BUZZER_RESOLUTION 8
bool soundEnabled = true; // default

//----------- SLEEP VARIABLES -----------
unsigned long lastInteractionTime = 0;
unsigned long sleepTimeout = 30000;  // 30 sec

//----------- TIME VARIABLES -----------
String cachedTime = "--:--";
String cachedPeriod = "--";
struct tm cachedTimeInfo; // Saved time without Wifi
RTC_DATA_ATTR time_t lastTimeSync = 0;
unsigned long lastTimeUpdate = 0;

//----------- TIMED EVENTS VARIABLES -----------
struct TimedEvent {
  time_t lastTrigger;
  uint32_t interval;   // seconds
};
RTC_DATA_ATTR TimedEvent waterReminderEvent = {0, 1800}; // default 30 min 
RTC_DATA_ATTR bool dailySyncDone = false; // For the 10 PM Upload
RTC_DATA_ATTR bool morningSyncDone = false; // For the 10 AM Time Correction


//----------- WATER INTAKE VARIABLES -----------
int waterGoal = 2;
RTC_DATA_ATTR int waterDrank = 0;
RTC_DATA_ATTR int lastDay = -1;
RTC_DATA_ATTR bool waterDrinkAllowed = false;
RTC_DATA_ATTR bool pendingWaterReminder = false;
//----------- NOTIFICATIONS VARIABLES -----------
enum NotificationType {
  NOTIF_WATER,
  NOTIF_CLOUD_SYNC,
  NOTIF_GENERIC,
  NOTIF_STEP_GOAL,
  NOTIF_SUNLIGHT_GOAL,
  NOTIF_WATER_GOAL,
  NOTIF_LEVEL_UP
};
bool notificationActive = false;
NotificationType currentNotification;

const int reminderStartHour = 10;
const int reminderEndHour = 22;
bool timeIsValid = (cachedTimeInfo.tm_year >= 100);
bool isNight = timeIsValid && (cachedTimeInfo.tm_hour >= reminderEndHour || cachedTimeInfo.tm_hour < reminderStartHour); // Check if time is after 10pm but before 10am
//----------- MISSION VARIABLES -----------
RTC_DATA_ATTR int dailyStepGoal = 50;
RTC_DATA_ATTR bool stepMissionComplete = false;
RTC_DATA_ATTR bool sunMissionComplete = false;
RTC_DATA_ATTR bool sunlightMissionActive = false;
RTC_DATA_ATTR int sunGoalSeconds = 120; // 2 minutes
RTC_DATA_ATTR bool waterMissionComplete = false;

const int NUM_MISSIONS = 3;
int mission_selected = 0;

char mission_items[NUM_MISSIONS][20] = {
  { "Daily Steps" },
  { "Sunlight" },
  { "Water" }
};
// -------------------------------------------------------------------------
//                             MELODIES
// -------------------------------------------------------------------------
const int melody[] = {1047, 1319, 1568, 2093}; // C6 E6 G6 C7
const int noteDurations[] = {120,120,120,200};
const int melodyLength = 4;
int noteIndex = 0;
unsigned long noteStart = 0;
bool playingSound = false;
int melodyRepeatCount = 0;
const int maxMelodyRepeats = 3;

//----------- SOUND MANAGER VARIABLES -----------
enum MelodyType { MELODY_NONE, MELODY_WATER, MELODY_ERROR, MELODY_VICTORY };
MelodyType activeMelody = MELODY_NONE;

// Water Reminder: Upbeat F-major arpeggio (C6, F6, A6, C7)
const int waterNotes[] = {1047, 1396, 1760, 2093}; 
const int waterDurations[] = {80, 80, 80, 150};
const int waterLength = 4;
const int maxWaterRepeats = 3;

// Error Buzz: "teet teet" (G4, short pause, G4)
const int errorNotes[] = {392, 0, 392}; // '0' acts as a silent rest
const int errorDurations[] = {100, 50, 100};
const int errorLength = 3;

// Victory Arpeggio: C6, E6, G6, C7
const int victoryNotes[] = {1047, 1319, 1568, 2093}; 
const int victoryDurations[] = {120, 120, 120, 200};
const int victoryLength = 4;
const int maxVictoryRepeats = 1;


//----------- PROGRESSION & RPG VARIABLES -----------
// 3 Missions (300) + Daily Bonus (50) = 350 XP possible per day
RTC_DATA_ATTR bool dailyBonusAwarded = false;
RTC_DATA_ATTR int currentLevel = 0;
RTC_DATA_ATTR int currentXP = 0;
const int MAX_LEVEL = 6;

// Base XP Rewards
const int XP_PER_MISSION = 100;
const int XP_DAILY_BONUS = 50; 

int getXPRequirement(int level) {
  return 350; 
}

void updateDynamicGoals() {
  waterGoal = 2 + currentLevel; // Base 2 increases lineary with level increase
  dailyStepGoal = 500 + (currentLevel * 1000); // Increases by 1000 for every level
  sunGoalSeconds = 120 + (currentLevel * 120); // Increases by 2 minuts for every level
  
  updateWaterReminderInterval(waterGoal);
  Serial.print("Goals Updated -> Water: "); Serial.print(waterGoal);
  Serial.print(" | Steps: "); Serial.print(dailyStepGoal);
  Serial.print(" | Sun: "); Serial.println(sunGoalSeconds);
}

void gainXP(int amount) {
  currentXP += amount;
  Serial.print("Gained XP: "); Serial.println(amount);
  
  bool leveledUp = false;
  
  // MAX_LEVEL restriction removed
  while (currentXP >= getXPRequirement(currentLevel)) {
    currentXP -= getXPRequirement(currentLevel);
    currentLevel++;
    
    // ---> TEMPORARY LOOP-BACK LOGIC <---
    // If we level up past the Max Level, loop back to the Egg (Level 0)
    if (currentLevel > MAX_LEVEL) {
      currentLevel = 0; 
      Serial.println("DEV MODE: Max Level crossed! Looping back to Level 0.");
    }
    
    leveledUp = true;
  }
  
  if (leveledUp) {
    Serial.print("LEVEL UP! Now Level: "); Serial.println(currentLevel);
    updateDynamicGoals();
    preferences.putInt("level", currentLevel);
    triggerNotification(NOTIF_LEVEL_UP, true); 
  }
  
  preferences.putInt("xp", currentXP); 
}

void loseXP(int amount) {
  // If already at Level 0 Egg with 0 XP, they can't be punished further
  if (currentLevel == 0 && currentXP == 0) return; 

  currentXP -= amount;
  Serial.print("Failed Missions. Lost XP: "); Serial.println(amount);
  
  // Check for Level Down
  if (currentXP < 0) {
    if (currentLevel > 0) {
      currentLevel--;
      currentXP = 0; // Empty the XP bar for the previous level
      
      Serial.print("LEVEL DOWN! Now Level: "); Serial.println(currentLevel);
      
      // Update goals to make it slightly easier to climb back up
      updateDynamicGoals(); 
      preferences.putInt("level", currentLevel);
    } else {
      // Failsafe: Stuck at Level 0
      currentXP = 0; 
    }
  }
  
  // Save the new XP state
  preferences.putInt("xp", currentXP); 
}

void checkDailyBonus() { // Check if all daily mission have been completed and award bonus XP
  if (!dailyBonusAwarded && stepMissionComplete && sunMissionComplete && waterMissionComplete) {
    Serial.println("All Daily Missions Complete! Awarding Bonus XP.");
    dailyBonusAwarded = true;
    gainXP(XP_DAILY_BONUS);
  }
}

void saveDailyAnalytics() { // Save daily data to memory
  preferences.putInt("lastSteps", stepCount);
  preferences.putInt("lastWater", waterDrank);
  preferences.putInt("lastSun", sunlightSeconds);
  preferences.putBool("lastAllDone", dailyBonusAwarded);
  
  Serial.println("Analytics snapshot saved. Ready for Firebase Sync.");
}

//----------- FIREBASE CREDENTIALS AND FUNCTION -----------
const String FIREBASE_PROJECT_ID = "---"; // Found in Project Settings
const String FIREBASE_API_KEY = "---"; // Found in Project Settings
const String DEVICE_ID = "GrowPal_001"; // Unique ID for this specific device
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
void uploadDailyData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot upload to Firebase: No WiFi connection.");
    return;
  }

  Serial.println("Starting Official Firebase Cloud Upload...");

  // 1. Initialize Firebase connection (Only needs to happen when WiFi is on)
  config.api_key = FIREBASE_API_KEY;
  
  // Speeds up the connection since we are in Test Mode and not using email/password
  config.signer.test_mode = true; 
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // 2. Format today's date for the Document Name
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &cachedTimeInfo);
  String documentPath = "users/" + DEVICE_ID + "/daily_logs/" + String(dateStr);

  // 3. Build the Payload using FirebaseJson
  FirebaseJson content;
  content.set("fields/steps/integerValue", stepCount);
  content.set("fields/water/integerValue", waterDrank);
  content.set("fields/sunlight_seconds/integerValue", sunlightSeconds);
  content.set("fields/level/integerValue", currentLevel);
  content.set("fields/all_missions_done/booleanValue", dailyBonusAwarded);

  // 4. Send the PATCH request to Firestore
  String updateMask = "steps,water,sunlight_seconds,level,all_missions_done";
  
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), updateMask)) {
    Serial.println("Firebase Upload Success! Document Updated.");
  } else {
    Serial.print("Firebase Upload FAILED! Error: ");
    Serial.println(fbdo.errorReason());
  }
  
  // 5. Free up memory and close the SSL connection before going back to sleep
  fbdo.clear(); 
}

// ---------- FUNCTION PROTOTYPES ----------
void drawScreen(void);
int PercentFunction(int batteryPercent);
void attemptTimeSync();
void triggerNotification(NotificationType type, bool triggerAudio = true);
bool connectWiFi(bool forcePortal = false);
void disconnectWiFi();
void uploadDailyData();
void syncTimeFromInternet(bool forcePortal = false);

void setup()   {
  rtc_gpio_deinit((gpio_num_t)BUTTON_UP_PIN);
  rtc_gpio_deinit((gpio_num_t)BUTTON_ENTER_PIN);
  rtc_gpio_deinit((gpio_num_t)BUTTON_DOWN_PIN);
  rtc_gpio_deinit((gpio_num_t)BUTTON_BACK_PIN);

  preferences.begin("settings", false);
  sleepTimeout   = preferences.getULong("sleepTime", 30000);
  soundEnabled   = preferences.getBool("sound", true);
  brightnessLevel = preferences.getUChar("bright", 2);
  btStop();

  // --- RECOVER SAVED DATA ON COLD BOOT ---
  esp_sleep_wakeup_cause_t bootReason = esp_sleep_get_wakeup_cause();
  if (bootReason != ESP_SLEEP_WAKEUP_TIMER && bootReason != ESP_SLEEP_WAKEUP_EXT1) {
    waterDrank = preferences.getInt("water", 0);
    lastDay = preferences.getInt("lastDay", -1);
    dailySyncDone = preferences.getBool("syncDone", false);
    morningSyncDone = preferences.getBool("morningSync", false);
  // RECOVER RPG DATA
    currentLevel = preferences.getInt("level", 0);
    currentXP = preferences.getInt("xp", 0);
  }
  
  // Calculate difficulty based on loaded level
  updateDynamicGoals();
  // Dynamically space reminders based on the user's daily goal
  updateWaterReminderInterval(waterGoal);
  // Initialize Time limits (time sync and wifi portal)
  configTime(4 * 3600, 0, "pool.ntp.org"); 
  wm.setConnectTimeout(10);
  wm.setConfigPortalTimeout(120);
  
  Serial.begin(9600);

  // HARDWARE INIT 
  
  // 1. Initialize Display
  display.begin(0x3C, true);
  display.oled_command(SH110X_DISPLAYON); 
  display.setContrast(brightness_values[brightnessLevel]);
  display.clearDisplay();
  
  // 2. Initialize Battery 
  battery.begin(3000, 2.34); 
  analogReadResolution(12); 
  delay(250); // Allow ADC and OLED to stabilize

  // 3. Initialize Buzzer
  ledcAttach(BUZZER_PIN, 2000, 8); 
  ledcWriteTone(BUZZER_PIN, 0);
  ledcWrite(BUZZER_PIN, 0);
  
  // 4. Take a stable boot voltage reading
  float bootVoltage = 0;
  for (int i = 0; i < 10; i++) {
    bootVoltage += battery.voltage();
    delay(5);
  }
  bootVoltage = (bootVoltage / 10.0) / 1000.0; // Convert to Volts
  Serial.print("Boot Voltage: ");
  Serial.println(bootVoltage);


  // ---------- WAKE UP HANDLING ----------
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  Serial.println(reason);
  
  if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    getLocalTime(&cachedTimeInfo, 10);
    int hour = cachedTimeInfo.tm_hour;

    // 1. Quick local voltage check
    battery.begin(3000, 2.46);
    analogReadResolution(12);
    delay(50);
    float quickVolts = 0;
    for (int i = 0; i < 5; i++) { quickVolts += battery.voltage(); delay(5); }
    quickVolts = (quickVolts / 5.0) / 1000.0;

    // 2. CHECK STEPS FIRST
    setupStepCounter(false); 
    updateStepCounter();
    
    // If they hit the goal during this sleep cycle, trigger notification screen
    if (stepCount >= dailyStepGoal && !stepMissionComplete) {
      stepMissionComplete = true;
      triggerNotification(NOTIF_STEP_GOAL, true);
      gainXP(XP_PER_MISSION);
      checkDailyBonus();
    }

    // 3. CHECK WATER
    if (hour >= reminderStartHour && hour < reminderEndHour) {
      if (waterDrank < waterGoal && checkTimedEvent(waterReminderEvent)) {
          pendingWaterReminder = true;
      }
    }

    // 4. HANDLE SYNCS
    // 10 PM Sync: Uploads data and syncs time
    bool is10PMSync = (hour >= reminderEndHour && !dailySyncDone && cachedTimeInfo.tm_year >= 100);
    
    // 10 AM Sync: Strictly for fixing overnight RTC time drift
    bool isMorningSync = (hour >= reminderStartHour && hour < reminderEndHour && !morningSyncDone && cachedTimeInfo.tm_year >= 100);
    
    // Failsafe: Battery died and forgot the year
    bool isLostTimeSync = (cachedTimeInfo.tm_year < 100);

    if (is10PMSync || isMorningSync || isLostTimeSync) {
      
      display.begin(0x3C, true);
      display.setContrast(brightness_values[brightnessLevel]);
      display.clearDisplay();

      currentNotification = NOTIF_CLOUD_SYNC;
      drawNotificationScreen(); 
      
      syncTimeFromInternet(); // Fixes the time drift
      
      if (is10PMSync) {
        uploadDailyData();
        dailySyncDone = true;
        preferences.putBool("syncDone", true);
      }
      
      if (isMorningSync) {
        morningSyncDone = true;
        preferences.putBool("morningSync", true);
      }

      disconnectWiFi();

      // If a step/water notification was triggered, DO NOT sleep yet
      if (!notificationActive) {
         goToSleep(); 
      }
    }

    // 5. THE SLEEP DECISION
    // If the device woke up, checked steps/water, and NO notification was triggered
    // Go right back to sleep without turning on the OLED or Buzzer
    if (!notificationActive && !pendingWaterReminder) {
        Serial.println("Background checks complete. No notifications. Sleeping...");
        goToSleep();
    }
  }

  // ---------- STANDARD INITIALIZATION (Only reached if not sleeping) ----------
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP); 
  pinMode(BUTTON_ENTER_PIN, INPUT_PULLUP); 
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP); 
  pinMode(BUTTON_BACK_PIN, INPUT_PULLUP); 

  bool isColdBoot = (reason != ESP_SLEEP_WAKEUP_TIMER && reason != ESP_SLEEP_WAKEUP_EXT1);
  setupStepCounter(isColdBoot);
  setupSunlightSensor();
  
  drawScreen();
  display.display();

  if (pendingWaterReminder) { // If reminder has been triggered but user didn't drink water yet
      triggerNotification(NOTIF_WATER, true); // Trigger notification screen again
  }
  while (digitalRead(BUTTON_UP_PIN) == LOW || // Wait for buttons release
         digitalRead(BUTTON_DOWN_PIN) == LOW || 
         digitalRead(BUTTON_ENTER_PIN) == LOW || 
         digitalRead(BUTTON_BACK_PIN) == LOW) {
      delay(10);
  }
  delay(50);
}

void loop() {
  updateSound();
  if (notificationActive) {

  if (!digitalRead(BUTTON_UP_PIN) || // Wait for buttons release
      !digitalRead(BUTTON_DOWN_PIN) ||
      !digitalRead(BUTTON_ENTER_PIN) ||
      !digitalRead(BUTTON_BACK_PIN)) {

      notificationActive = false; // Dismiss notification
      lastInteractionTime = millis();
  }

  drawNotificationScreen();
  display.display();
  if (millis() - lastInteractionTime > sleepTimeout) {
        Serial.println("Notification ignored. Going back to sleep...");
        
        // Ensure we don't wake up immediately to the same notification
        notificationActive = false; 
        
        goToSleep();
  }

  return;
  }
  
  updateStepCounter();

  // Step Mission Check
  if (stepCount >= dailyStepGoal && !stepMissionComplete) {
      stepMissionComplete = true;
      Serial.println("MISSION COMPLETE: Step Goal Reached!");
      gainXP(XP_PER_MISSION);
      checkDailyBonus();
      triggerNotification(NOTIF_STEP_GOAL, true);
  }

  // Sunlight mission check
  if (sunlightMissionActive) {
    updateSunlightSensor();
    
    // Check for victory using the dynamic variable
    if (sunlightSeconds >= sunGoalSeconds && !sunMissionComplete) {
      sunMissionComplete = true;
      sunlightMissionActive = false; // Turn off the active state
      isCurrentlySunny = false;      // Hide the sun icon
      gainXP(XP_PER_MISSION);
      checkDailyBonus();
      Serial.println("MISSION COMPLETE: Sunlight Goal Reached!");
      triggerNotification(NOTIF_SUNLIGHT_GOAL, true); // use the Sunlight Goal UI
    }
  }
  // Handle Up and Down buttons for all menus
  if (current_screen == SCREEN_MAIN) {
    handleUpDown(item_selected, NUM_ITEMS);
  }
  else if (current_screen == SCREEN_MISSIONS) {
    handleUpDown(mission_selected, NUM_MISSIONS);
  }
  else if (current_screen == SCREEN_SETTINGS) {
    handleUpDown(settings_selected, NUM_SETTINGS);
  }
  else if (current_screen == SCREEN_SLEEP_TIMER) {
    handleUpDown(timeout_selected, NUM_TIMEOUT_OPTIONS);
  }
  else if (current_screen == SCREEN_SOUND) {
    handleUpDown(sound_selected, NUM_SOUND_OPTIONS);
  }
  else if (current_screen == SCREEN_BRIGHTNESS) {
      if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0)) {
        lastInteractionTime = millis();
        clickSound();
        if (brightness_selected > 0) brightness_selected--; // Brightness down
          display.setContrast(brightness_values[brightness_selected]); // Live brightness update
          button_up_clicked = 1;
      }

      else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0)) {
          lastInteractionTime = millis();
          clickSound();
          if (brightness_selected < NUM_BRIGHTNESS_OPTIONS - 1) brightness_selected++; // Brightness Up
            display.setContrast(brightness_values[brightness_selected]); // live brightness update
            button_down_clicked = 1;
      }
    }
  // ENTER button logic
  if ((digitalRead(BUTTON_ENTER_PIN) == LOW) && (button_enter_clicked == 0)) {
    lastInteractionTime = millis();
    clickSound();
    if (current_screen == SCREEN_MAIN) {
      if (item_selected == 0) {  // Missions Menu
          current_screen = SCREEN_MISSIONS;
      }
       if (item_selected == 1) {  // Drink Water
      drinkWater();
      }
      if (item_selected == 2) {  // Reset Steps (hijacked for testing)
        /*
        resetSteps();
        
        // ---> TEMPORARY TESTING FULL SYSTEM RESET <---
        waterDrank = 0;
        sunlightSeconds = 0;
        
        // Reset the mission locks so you can earn XP again
        stepMissionComplete = false;
        waterMissionComplete = false;
        sunMissionComplete = false;
        dailyBonusAwarded = false;
        waterDrinkAllowed = true; // Unlock the water button
        
        
        preferences.putInt("water", 0);
        Serial.println("DEV MODE: All missions and water reset!");
        */
        // ---> MANUAL FIREBASE UPLOAD <---
        Serial.println("DEV MODE: FORCING FIREBASE UPLOAD...");
        
        // Ensure WiFi is connected before syncing
        if (WiFi.status() != WL_CONNECTED) {
          connectWiFi(false); 
        }
        
        // Force the time sync so the document gets the correct date
        syncTimeFromInternet(); 
        
        // Upload Data
        uploadDailyData(); 
        
        disconnectWiFi(); // Newly added to fix connection to firestore

        triggerNotification(NOTIF_GENERIC, true); // Notification to confirm
        
      }
      if (item_selected == 4) { //sleep
          goToSleep();
      }
      if (item_selected == 5) {   // Settings
          current_screen = SCREEN_SETTINGS;
      }
      
    }
    else if (current_screen == SCREEN_MISSIONS) {
      if (mission_selected == 1) {   // Sunlight Mission
        if (!sunMissionComplete) {
          sunlightMissionActive = !sunlightMissionActive; // Toggle ON/OFF
          
          if (sunlightMissionActive) {
            Serial.println("Sunlight Mission STARTED");
            if (soundEnabled) {
              // Custom activation tone
              // Note 1: Lower pitch, fast
              ledcWriteTone(BUZZER_PIN, 1200); 
              delay(40); 
              ledcWriteTone(BUZZER_PIN, 0);
              ledcWrite(BUZZER_PIN, 0); 
              
              delay(30); // Microscopic pause for crispness
              
              // Note 2: Higher pitch, slightly longer
              ledcWriteTone(BUZZER_PIN, 2000); 
              delay(60); 
              ledcWriteTone(BUZZER_PIN, 0);
              ledcWrite(BUZZER_PIN, 0); // Kill duty cycle
            }
          } else {
            Serial.println("Sunlight Mission PAUSED");
            isCurrentlySunny = false; // Turn off the sun icon if paused
          }
        }
      }
    }
    else if (current_screen == SCREEN_SETTINGS) {
      if (settings_selected == 0) {   // Sleep Timer
        // sync selection with current timeout
        for (int i = 0; i < NUM_TIMEOUT_OPTIONS; i++) {
          if (timeout_values[i] == sleepTimeout) {
            timeout_selected = i;
            break;
          }
        }
        current_screen = SCREEN_SLEEP_TIMER;
      }
      else if (settings_selected == 1) {   // Sound
        sound_selected = soundEnabled ? 0 : 1;
        current_screen = SCREEN_SOUND;
      }
      else if (settings_selected == 2) {   // Brightness
        brightnessBeforeEdit = brightnessLevel; 
        brightness_selected = brightnessLevel;
        current_screen = SCREEN_BRIGHTNESS;
      }
      else if (settings_selected == 3) {   // Sync Time
      syncTimeFromInternet(true);

      disconnectWiFi();
      }
    }
    else if (current_screen == SCREEN_SLEEP_TIMER) { // Sleep timer info
      sleepTimeout = timeout_values[timeout_selected];
      preferences.putULong("sleepTime", sleepTimeout);
      current_screen = SCREEN_SETTINGS;
    }
    else if (current_screen == SCREEN_SOUND) { // Sound info
      soundEnabled = (sound_selected == 0);
      preferences.putBool("sound", soundEnabled);
      current_screen = SCREEN_SETTINGS;
    }
    else if (current_screen == SCREEN_BRIGHTNESS) { // Brightness info
      brightnessLevel = brightness_selected;
      preferences.putUChar("bright", brightnessLevel);
      current_screen = SCREEN_SETTINGS;
    }
    button_enter_clicked = 1;
  }
  // BACK button Logic
  if ((digitalRead(BUTTON_BACK_PIN) == LOW) && (button_back_clicked == 0)) {
    lastInteractionTime = millis();
    clickSound();
    if (current_screen == SCREEN_MISSIONS) {
      current_screen = SCREEN_MAIN;
    }
    else if (current_screen == SCREEN_SETTINGS) {
      current_screen = SCREEN_MAIN;
    }
    else if (current_screen == SCREEN_SLEEP_TIMER) {
      current_screen = SCREEN_SETTINGS;
    }
    else if (current_screen == SCREEN_SOUND) {
      current_screen = SCREEN_SETTINGS;
    }
    else if (current_screen == SCREEN_BRIGHTNESS) {
      display.setContrast(brightness_values[brightnessBeforeEdit]); // Restore Brightness
      current_screen = SCREEN_SETTINGS;
    }
    button_back_clicked = 1;
  }
  if ((digitalRead(BUTTON_UP_PIN) == HIGH) && (button_up_clicked == 1)) { // unclick 
    button_up_clicked = 0;
  }
  if ((digitalRead(BUTTON_DOWN_PIN) == HIGH) && (button_down_clicked == 1)) { // unclick
    button_down_clicked = 0;
  }
  if ((digitalRead(BUTTON_ENTER_PIN) == HIGH) && (button_enter_clicked == 1)) { // unclick 
    button_enter_clicked = 0;
  }
  if ((digitalRead(BUTTON_BACK_PIN) == HIGH) && (button_back_clicked == 1)) { // unclick 
    button_back_clicked = 0;
  }
  // Battery voltage readout
  if (millis() - lastBatteryRead > 1000) { // read every second
  lastBatteryRead = millis();

  batteryVoltage = battery.voltage();
  batteryPercent = battery.level();
  displayPercent = PercentFunction(batteryPercent);
  Serial.print("Voltage: ");
  Serial.print(batteryVoltage / 1000.0, 3);
  Serial.print("V  |  ");
  Serial.print(batteryPercent);
  Serial.println("%");
  }

  // Toggle display every 5 seconds
  if (millis() - lastToggle > 5000) {
    lastToggle = millis();
    showPercent = !showPercent;
  }

  // Water intake notification
  if (waterDrank < waterGoal &&
    cachedTimeInfo.tm_hour >= reminderStartHour &&
    cachedTimeInfo.tm_hour < reminderEndHour &&
    checkTimedEvent(waterReminderEvent)) {
    pendingWaterReminder = true;
    triggerNotification(NOTIF_WATER);
}

  updateTimeCache();
  checkNewDay();
  
  updateAnimation();
  
  display.clearDisplay();
  drawScreen();
  display.display();
  
  delay(50); // Small delay to prevent screen flickering 
}

//---------- Universal UP and Down buttons Function ----------
void handleUpDown(int &selected, int max_items) {

    if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0)) {
        lastInteractionTime = millis();
        clickSound();
        if (selected > 0) selected--;
        button_up_clicked = 1;
    }

    else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0)) {
        lastInteractionTime = millis();
        clickSound();
        if (selected < max_items - 1) selected++;
        button_down_clicked = 1;
    }
}

//---------- Universal Menu Drawing Function ----------
void drawMenu(char items[][20], int num_items, int selected) {

  int prev = selected - 1;
  int next = selected + 1;

  if (selected > 0) {
    display.setCursor(3, 91);
    display.println(items[prev]);
  }

  display.setCursor(16, 103);
  display.println(items[selected]);

  if (selected < num_items - 1) {
    display.setCursor(3, 115);
    display.println(items[next]);
  }
}

void updateAnimation() {
  if (millis() - lastFrameTime > frameInterval) {
    lastFrameTime = millis();
    currentFrame++;
    if (currentFrame >= totalFrames) {
      currentFrame = 0;
    }
  }

  if (millis() - lastInteractionTime > sleepTimeout) {
    goToSleep();
  }
}

//---------- Screen UI Function ----------
void drawScreen(void) {
  if (notificationActive) {
    drawNotificationScreen();
    return;
  }
  // battery_empty
  display.drawBitmap(104, 0, image_battery_empty_bits, 24, 16, SH110X_WHITE);
 
  //wifi connected
  if (WiFi.status() == WL_CONNECTED){ 
    display.drawBitmap(5, 1, image_wifi_1_bits, 19, 16, SH110X_WHITE);
    // cloud_sync
    display.drawBitmap(6, 19, image_cloud_sync_bits, 17, 16, SH110X_WHITE);
  }
  else{
  // wifi not connected
    display.drawBitmap(5, 1, image_wifi_not_connected_bits, 19, 16, SH110X_WHITE);
  }
  // Layer 6
  display.drawBitmap(51, 2, image_Layer_6_bits, 38, 10, SH110X_WHITE);
  
  // Sunlight icon - Only draw the sun icon if the INA219 detects outdoor light
  if (isCurrentlySunny) {
     display.drawBitmap(7, 57, image_sunlight, 15, 16, SH110X_WHITE);
  }
  // ---------- XP FILL ----------
  // Level text
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(26, 3);
  display.println("lvl");
  display.setCursor(44, 3);
  display.println(currentLevel);
  int xpMaxWidth = 32;
  int xpPercent = (currentXP * 100) / getXPRequirement(currentLevel);
  if (xpPercent > 100) xpPercent = 100;
  int xpFillWidth = map(xpPercent, 0, 100, 0, xpMaxWidth);
  
  // Start from left side
  int xpStartX = 54;
  display.fillRect(xpStartX, 5, xpFillWidth, 4, SH110X_WHITE);

  // ---------- BATTERY FILL ----------
  int maxWidth = 17;
  int fillWidth = map(displayPercent, 0, 100, 0, maxWidth);

  // Start from right side
  int startX = 109 + (maxWidth - fillWidth);

  display.fillRect(startX, 3, fillWidth, 9, SH110X_WHITE);


  // layer 25 sunlight exposure bar
  int maxHeight = 50;
  // Map the seconds to the pixel height
  int sunFillHeight = map(sunlightSeconds, 0, sunGoalSeconds, 0, maxHeight);
  // Cap the bar so it doesn't overflow if they stay outside longer!
  if (sunFillHeight > maxHeight) sunFillHeight = maxHeight; 
  // Draw the bar (starts from the bottom and grows up)
  display.fillRect(120, 34 + (maxHeight - sunFillHeight), 5, sunFillHeight, SH110X_WHITE);

  // layer 26 water intake bar
  int fillHeight = map(waterDrank, 0, waterGoal, 0, maxHeight);
  display.fillRect(109, 34 + (maxHeight - fillHeight), 5, fillHeight, SH110X_WHITE);
  
  // notification_bell
  //display.drawBitmap(7, 39, image_notification_bell_bits, 16, 16, SH110X_WHITE);

  // ---------- DYNAMIC CHARACTER RENDERING ----------
  // Max Level is 6. Levels 0-6 = 7 Total Evolutions.

  if (currentLevel == MAX_LEVEL) {
    // 1. ANIMATED MAX LEVEL DRAGON (Level 6)
    if (currentFrame == 0) {
      display.drawBitmap(35, 23, image_lvl6_1_bits, 60, 58, SH110X_WHITE);
    } else {
      display.drawBitmap(35, 23, image_lvl6_2_bits, 60, 58, SH110X_WHITE);
    }
      
  } else {
    // 2. STATIC EVOLUTIONS (Levels 0 to 5)
    // Centered around the same X/Y anchor points
    switch (currentLevel) {
      case 0: // Level 0: Unhatched Egg (42x52)
        display.drawBitmap(42, 26, image_lvl0_bits, 42, 52, SH110X_WHITE);
        break;
      case 1: // Level 1 (58x70)
        display.drawBitmap(34, 17, image_lvl1_bits, 58, 70, SH110X_WHITE);
        break;
      case 2: // Level 2 (58x70)
        display.drawBitmap(34, 17, image_lvl2_bits, 58, 70, SH110X_WHITE);
        break;
      case 3: // Level 3 (57x57)
        display.drawBitmap(35, 24, image_lvl3_bits, 57, 57, SH110X_WHITE);
        break;
      case 4: // Level 4 (58x57)
        display.drawBitmap(35, 24, image_lvl4_bits, 58, 57, SH110X_WHITE);
        break;
      case 5: // Level 5 (57x57)
        display.drawBitmap(35, 24, image_lvl5_bits, 57, 57, SH110X_WHITE);
        break;
    }
  }


  // Menus Layer
  if (current_screen == SCREEN_MAIN) {
    drawMenu(menu_items, NUM_ITEMS, item_selected);
  }
  else if (current_screen == SCREEN_MISSIONS) {
    drawMenu(mission_items, NUM_MISSIONS, mission_selected);
  }
  else if (current_screen == SCREEN_SETTINGS) {
      drawMenu(settings_items, NUM_SETTINGS, settings_selected);
  }
  else if (current_screen == SCREEN_SLEEP_TIMER) {
      drawMenu(timeout_labels, NUM_TIMEOUT_OPTIONS, timeout_selected);
  }
  else if (current_screen == SCREEN_SOUND) {
      drawMenu(sound_items, NUM_SOUND_OPTIONS, sound_selected);
  }
  else if (current_screen == SCREEN_BRIGHTNESS) {
    drawMenu(brightness_items, NUM_BRIGHTNESS_OPTIONS, brightness_selected);
  }


  // Layer 23
  display.drawBitmap(0, 19, image_Layer_23_bits, 128, 109, SH110X_WHITE);

  // Layer 22
  display.setCursor(87, 120);
  display.println("Growpal");

  // ---------- Battery % Text and TIME ----------
  if (millis() - lastTimeUpdate > 1000) {
    lastTimeUpdate = millis();
    getFormattedTime(cachedTime, cachedPeriod);
  }
  // ---------- Battery % Text ----------
  if (showPercent) {
    display.setCursor(88, 15);
    display.print(batteryPercent);
    display.print("%");
  } else {
    display.setCursor(88, 15);
    display.print(batteryVoltage / 1000.0, 2); 
    display.print("V");
  }

  // ---------- Dynamic Bottom-Right Info Panel ----------
  if (current_screen == SCREEN_MISSIONS && mission_selected == 0) {
      
      // 1. Hovering over "Daily Steps"
      int progressPercent = (stepCount * 100) / dailyStepGoal;
      if (progressPercent > 100) progressPercent = 100; // Cap at 100%

      if (showPercent) {
        display.setCursor(94, 91);
        display.println("Goal");
        display.setCursor(104, 102);
        display.println(dailyStepGoal);
      } else {
        display.setCursor(94, 91);
        display.println("Prog.");
        display.setCursor(104, 102);
        display.print(progressPercent);
        display.println("%");
      }

  } else if (current_screen == SCREEN_MISSIONS && mission_selected == 1) {
      
      // 2. Hovering over "Sunlight"
      int sunProgressPercent = (sunlightSeconds * 100) / sunGoalSeconds;
      if (sunProgressPercent > 100) sunProgressPercent = 100; // Cap at 100%

      if (showPercent) {
        display.setCursor(94, 91);
        display.println("Goal");
        display.setCursor(104, 102);
        display.print(sunGoalSeconds / 60); 
        display.println(" Min");
      } else {
        display.setCursor(94, 91);
        
        // Dynamically change the header text to show the user it is running
        if (sunlightMissionActive) {
          display.println("Active");

        } else {
          display.println("Prog.");
        }
        
        display.setCursor(104, 102);
        display.print(sunProgressPercent);
        display.println("%");
      }

  } else if (current_screen == SCREEN_MISSIONS && mission_selected == 2) {
    // 3. Hovering over "Water Intake"
      int progressPercent = (waterDrank * 100) / waterGoal;
      if (progressPercent > 100) progressPercent = 100; // Cap at 100%

      if (showPercent) {
        display.setCursor(94, 91);
        display.println("Goal");
        display.setCursor(104, 102);
        display.println(waterGoal);
      } else {
        display.setCursor(94, 91);
        display.println("Prog.");
        display.setCursor(104, 102);
        display.print(progressPercent);
        display.println("%");
      }

  } else {
      
      // 3. Default Behavior (Standard Time/Steps for all other screens)
      if (showPercent) {
        display.setCursor(94, 91);
        display.println("Steps");
        display.setCursor(104, 102);
        display.println(stepCount);
      } else {
        display.setCursor(94, 91);
        display.println(cachedTime);
        display.setCursor(104, 102);
        display.println(cachedPeriod);
      }
  }
  if (sunlightMissionActive) {
    display.setCursor(2, 73); 
    display.print(loadvoltage, 2); // Limit to 2 decimal places
    display.println(" V");
    
    display.setCursor(2, 81);      
    display.print(current_mA, 1);  // Limit to 1 decimal place
    display.println(" mA");    
  }
}
  
// ---------- Battery Functions ----------
  
int PercentFunction(int batteryPercent) {

   if (batteryPercent >= 90) return 100;
   if (batteryPercent >= 70) return 75;
   if (batteryPercent >= 45) return 50;
   if (batteryPercent >= 25) return 25;
   if (batteryPercent >= 0) return 0;
   return 0;
}

//---------- BUZZER Functions ----------

void clickSound() {
  if (!soundEnabled) return;
  ledcWriteTone(BUZZER_PIN, 2500);
  delay(20);
  ledcWriteTone(BUZZER_PIN, 0);
  ledcWrite(BUZZER_PIN, 0);
}

// --------- SLEEP Functions -----------

uint64_t getSleepDuration() {//DYNAMIC SLEEP CALCULATOR
  
  // Default sleep time in seconds (30 minutes)
  uint64_t sleepSeconds = 1800; 

  // If time isn't set yet, fallback to the 30 min default
  if (cachedTimeInfo.tm_year < 100) {
    return sleepSeconds * 1000000ULL; // Convert to microseconds and return
  }

  int hour = cachedTimeInfo.tm_hour;

  // Check if outside of active hours (10 PM to 10 AM)
  if (hour >= reminderEndHour || hour < reminderStartHour) {
    
    // Safety check: If it's past 10 PM but the sync FAILED, sleep for 30 mins and try again
    if (hour >= reminderEndHour && !dailySyncDone) {
      Serial.println("Sync pending. Sleeping 30 mins to retry...");
      return sleepSeconds * 1000000ULL;
    }

    // 1. Find current time in seconds
    int currentSeconds = (hour * 3600) + (cachedTimeInfo.tm_min * 60) + cachedTimeInfo.tm_sec;
    
    // 2. Find the target wake up time in seconds (10:00 AM)
    int targetSeconds = reminderStartHour * 3600; 

    // 3. Calculate the difference
    if (hour >= reminderEndHour) {
      // It is night time (e.g., 10 PM). Calculate time until midnight, then add the morning hours
      sleepSeconds = (86400 - currentSeconds) + targetSeconds;
    } else {
      // It is early morning (e.g., 3 AM). Just subtract current time from 10 AM
      sleepSeconds = targetSeconds - currentSeconds;
    }

    // Add a 10-second buffer to guarantee it wakes up slightly after 10:00:00 AM
    sleepSeconds += 10; 

    Serial.print("NIGHT MODE ACTIVATED! Sleeping for ");
    Serial.print(sleepSeconds / 3600.0);
    Serial.println(" hours until 10 AM.");
  }

  // Convert to microseconds
  return sleepSeconds * 1000000ULL;
}
void goToSleep() {

  Serial.println("Preparing to sleep...");

  delay(200);
  // Fix floating pins
  rtc_gpio_init((gpio_num_t)BUTTON_UP_PIN);// Put pin into RTC mode
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_UP_PIN); // Enable strong RTC pullup
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_UP_PIN);

  esp_sleep_enable_ext1_wakeup(
      (1ULL << BUTTON_UP_PIN),
      ESP_EXT1_WAKEUP_ANY_LOW
  );

  // Turn off OLED
  display.clearDisplay();
  display.display();
  display.oled_command(SH110X_DISPLAYOFF); // Turn off Oled
  display.oled_command(0xAD); 
  display.oled_command(0x8A);


  sleepStepCounter(isNight);
  sleepSunlightSensor();
  // Isolate buttons so they don't float
  rtc_gpio_isolate((gpio_num_t)BUTTON_ENTER_PIN);
  rtc_gpio_isolate((gpio_num_t)BUTTON_DOWN_PIN);
  rtc_gpio_isolate((gpio_num_t)BUTTON_BACK_PIN);

  delay(50);
  Serial.println("Going to sleep...");
  delay(100);
  // Wake every 30 minutes
  uint64_t sleepTime = getSleepDuration();
  esp_sleep_enable_timer_wakeup(sleepTime);
  disconnectWiFi();

  Serial.flush();
  esp_deep_sleep_start();
}

//---------- WIFI Functions ----------
bool connectWiFi(bool forcePortal) {
  WiFi.mode(WIFI_STA);

  if (forcePortal) {
    // 1. MANUAL MODE (WiFi Setup Button)
    Serial.println("Manual WiFi mode initiated...");
    wm.setConfigPortalTimeout(120); // Give user 2 mins if portal opens
    
    // autoConnect tries saved credentials first. 
    // If it connects, it skips the portal. If it fails, the portal opens.
    bool res = wm.autoConnect("GrowPal-Setup"); 
    
    if (!res) {
      Serial.println("WiFi failed or portal timed out.");
      return false;
    }
    Serial.println("WiFi connected via Manual Mode!");
    return true;
    
  } else {
    // 2. BACKGROUND MODE
    Serial.println("Attempting silent background WiFi connection...");
    
    // Bypass WiFiManager, use ESP32 WiFi to prevent portals.
    WiFi.begin(); 
    
    int attempts = 0;
    // Wait up to 10 seconds
    while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nBackground WiFi failed. Aborting silently.");
      return false;
    }

    Serial.println("\nWiFi connected via Background Mode!");
    return true;
  }
}

void disconnectWiFi() {
  Serial.println("Turning off WiFi and Bluetooth to save power...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();  // Turn off bluetooth since its never used
  delay(100);
}

//---------- Time Function ----------
void syncTimeFromInternet(bool forcePortal) {

  if (!connectWiFi(forcePortal)) {
    Serial.println("attemptTimeSync: WiFi connect failed - abort");
    disconnectWiFi(); 
    return;
  }

  // 1. Force the ESP32 to reset new time
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

  // 2. Reset to 1970
  // This prevents the ESP32 from tricking itself into thinking it already has the time
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);

  // 3. Request the new time from the internet
  configTime(4 * 3600, 0, "pool.ntp.org"); 
  struct tm timeinfo;
  Serial.println("attemptTimeSync: waiting for NTP...");

  // 4. Wait for the internet reply
  bool synced = false;
  for (int i = 0; i < 15; ++i) { 
    // check the year to make sure the 1970 is gone
    if (getLocalTime(&timeinfo, 1000) && timeinfo.tm_year > 100) { 
        cachedTimeInfo = timeinfo;
        lastTimeSync = time(nullptr);
        Serial.println("attemptTimeSync: time synced & corrected!");
        synced = true;
        break;
    }
    delay(500);
    Serial.print(".");
  }
  
  if (!synced) Serial.println("attemptTimeSync: NTP timed out");

  disconnectWiFi();
}

void getFormattedTime(String &timeString, String &periodString) {

  int hour = cachedTimeInfo.tm_hour;
  int minute = cachedTimeInfo.tm_min;

  if (hour < 0) {
    timeString = "--:--";
    periodString = "--";
    return;
  }

  periodString = "AM";
  if (hour >= 12) periodString = "PM";

  hour = hour % 12;
  if (hour == 0) hour = 12;

  char buffer[6];
  sprintf(buffer, "%d:%02d", hour, minute);

  timeString = buffer;
}

void updateTimeCache() { // Update saved time

  if (millis() - lastTimeUpdate < 1000) return;

  lastTimeUpdate = millis();

  if (getLocalTime(&cachedTimeInfo, 10)) {
    getFormattedTime(cachedTime, cachedPeriod);
  }
}

bool checkTimedEvent(TimedEvent &event) {

  time_t now = mktime(&cachedTimeInfo);

  if (now - event.lastTrigger >= event.interval) {
    event.lastTrigger = now;
    return true;
  }

  return false;
}
void checkNewDay() {
  // Ensure time is available
  if (cachedTimeInfo.tm_year < 100) return; 

  if (cachedTimeInfo.tm_yday != lastDay) {
    if (lastDay != -1) { 
      if (!stepMissionComplete || !sunMissionComplete || !waterMissionComplete) { // If not all daily mission are complete
        Serial.println("Daily Missions Failed! Applying Neglect Penalty...");
        loseXP(25); // Apply Penalty
      }
      // 1. Save data before wiping
      saveDailyAnalytics();

      waterDrank = 0;
      preferences.putInt("water", 0); // Save reset to flash
      pendingWaterReminder = false;
      // 2. Reset daily missions and tracking
      stepCount = 0; 
      sunlightSeconds = 0;
      stepMissionComplete = false;
      sunMissionComplete = false;
      waterMissionComplete = false;
      dailyBonusAwarded = false;
    }
    
    lastDay = cachedTimeInfo.tm_yday;
    preferences.putInt("lastDay", lastDay);
    
    dailySyncDone = false;
    preferences.putBool("syncDone", false);
    morningSyncDone = false;  
    preferences.putBool("morningSync", false);

    Serial.println("New day: Trackers and Syncs reset");
  }
}
//---------- Notification Functions ----------
void updateSound() { // Assign uniqie melodies

  if (!playingSound) return;

  const int* currentNotes;
  const int* currentDurations;
  int currentLen;
  int maxRepeats = 1; 

  if (activeMelody == MELODY_WATER) {
    currentNotes = waterNotes;
    currentDurations = waterDurations;
    currentLen = waterLength;
    maxRepeats = maxWaterRepeats;
  } else if (activeMelody == MELODY_ERROR) {
    currentNotes = errorNotes;
    currentDurations = errorDurations;
    currentLen = errorLength;
  } else if (activeMelody == MELODY_VICTORY) {
    currentNotes = victoryNotes;
    currentDurations = victoryDurations;
    currentLen = victoryLength;
    maxRepeats = maxVictoryRepeats;
  }
  // Check if it's time to change the note
  if (millis() - noteStart > currentDurations[noteIndex]) {    
    // Briefly turn off the buzzer to create a crisp gap between notes
    ledcWriteTone(BUZZER_PIN, 0);  
    // Only advance the note if this isn't the very first forced trigger
    if (noteStart != 0) { 
       noteIndex++;
    }
    // Check if the melody has finished
    if (noteIndex >= currentLen) {
      melodyRepeatCount++;
      if (melodyRepeatCount >= maxRepeats) {
        playingSound = false;
        activeMelody = MELODY_NONE;
        return;
      } else {
        noteIndex = 0; // Loop the melody back to the start
      }
    }
    // Play the current note (or stay silent if it's a '0' rest note)
    if (currentNotes[noteIndex] == 0) {
      ledcWriteTone(BUZZER_PIN, 0);
      ledcWrite(BUZZER_PIN, 0); 
    } else {
      ledcWriteTone(BUZZER_PIN, currentNotes[noteIndex]);
    }  
    noteStart = millis();
  }
}
void playSound(MelodyType type) {
  if (!soundEnabled) return;
  activeMelody = type;
  noteIndex = 0;
  melodyRepeatCount = 0;
  playingSound = true;
  
  // By setting this to 0, updateSound() will trigger the first note
  // the very first time it runs in the main loop, avoiding the "long beep" lag.
  noteStart = 0;
}

void triggerNotification(NotificationType type, bool triggerAudio) {

  notificationActive = true;
  currentNotification = type;
  
  if (type == NOTIF_WATER) {
    waterDrinkAllowed = true; // Unlock the ability to drink
  }

  // Play the correct melody based on the notification type
  if (triggerAudio && soundEnabled) {
    if (type == NOTIF_STEP_GOAL || type == NOTIF_SUNLIGHT_GOAL || 
        type == NOTIF_WATER_GOAL || type == NOTIF_LEVEL_UP) {
      playSound(MELODY_VICTORY);
    } else {
      playSound(MELODY_WATER); // Default for water and generic
    }
  }
}

void drawNotificationScreen() {

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  // Notification bell icon
  display.drawBitmap(54, 10, image_notification_bell_bits, 16, 16, SH110X_WHITE);
  display.setCursor(20, 50);

  switch (currentNotification) {

    case NOTIF_WATER:
      display.println("Drink Water!");
      break;

    case NOTIF_CLOUD_SYNC:
      display.drawBitmap(54, 40, image_cloud_sync_bits, 17, 16, SH110X_WHITE);
      display.println("Syncing...");
      break;

    case NOTIF_STEP_GOAL: 
      display.setCursor(18, 50);
      display.println("Steps Goal Reached!");
      display.setCursor(34, 70);
      display.print(dailyStepGoal); 
      display.println(" Steps");
      break;
    
    case NOTIF_SUNLIGHT_GOAL:   
      display.setCursor(18, 50);
      display.println("Sunlight Goal Reached!");
      display.setCursor(26, 70);
      display.print(sunGoalSeconds / 60);
      display.println(" Minutes Of Sunlight");
      break;
    
    case NOTIF_WATER_GOAL:   
      display.setCursor(18, 50);
      display.println("Water Goal Reached!");
      display.setCursor(26, 70);
      display.print(waterGoal);
      display.println(" Cups of Water");
      break;
    
    case NOTIF_LEVEL_UP:   
      display.setCursor(18, 50);
      display.println("XP Goal Reached!");
      display.setCursor(35, 70);
      display.print("Level Up!");
      display.setCursor(20, 80);
      display.print("Current Level: ");
      display.print(currentLevel);
      break;

    case NOTIF_GENERIC:
      display.println("Notification");
      break;
  }

  display.display();
}

//---------- Water Intake Functions ----------
void drinkWater() {
  if (waterDrinkAllowed && waterDrank < waterGoal) {
    waterDrank++;
    waterDrinkAllowed = false; 
    pendingWaterReminder = false;
    // Save immediately to non-volatile memory
    preferences.putInt("water", waterDrank); 
    
    if (soundEnabled) { // Unique Water intake tone
      // Note 1: Lower pitch, fast
      ledcWriteTone(BUZZER_PIN, 1200); 
      delay(40); 
      ledcWriteTone(BUZZER_PIN, 0);
      ledcWrite(BUZZER_PIN, 0); 
      
      delay(30); // Microscopic pause for crispness
      
      // Note 2: Higher pitch, slightly longer
      ledcWriteTone(BUZZER_PIN, 2000); 
      delay(60); 
      ledcWriteTone(BUZZER_PIN, 0);
      ledcWrite(BUZZER_PIN, 0); // Kill duty cycle
    }
    if (waterDrank >= waterGoal && !waterMissionComplete) {
      waterMissionComplete = true;
      Serial.println("Water Mission Complete!");
      triggerNotification(NOTIF_WATER_GOAL, true);
      gainXP(XP_PER_MISSION);
      checkDailyBonus();
    }

    Serial.print("Water intake: ");
    Serial.print(waterDrank);
    Serial.print("/");
    Serial.println(waterGoal);
  } else {
    playSound(MELODY_ERROR);
    Serial.println("Wait for the next reminder!");
  }
}

void updateWaterReminderInterval(int remindersPerDay) {
  // 1. Divide-by-zero protection
  if (remindersPerDay <= 0) {
    waterReminderEvent.interval = 999999; // Effectively disable reminders
    return;
  }

  // 2. Calculate the dynamic spacing
  int activeHours = reminderEndHour - reminderStartHour;
  int intervalSeconds = (activeHours * 3600) / remindersPerDay;

  waterReminderEvent.interval = intervalSeconds;
  
  Serial.print("Dynamic Water Interval Set: ");
  Serial.print(intervalSeconds / 3600.0);
  Serial.println(" hours between drinks.");
}

