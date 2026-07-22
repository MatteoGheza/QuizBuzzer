#ifndef ZIGBEE_MODE_ED
#error "Select 'Zigbee ED (End Device)' in Tools -> Zigbee mode"
#endif

#include <WiFi.h>
#include <..\QuizCommon\QuizCommon.h>

// Set to 1 (Red), 2 (Yellow), 3 (Green), or 4 (Blue)
#define CONTESTANT_ID 4

ZigbeeLight internalLedEP(1); // Listens on EP 1 for broadcast commands

// --- Game Logic Variables ---
bool isRoundActive = false;
bool isPenalized = false;
uint8_t earlyPressCount = 0;

// --- Blinking & Timing Variables ---
uint32_t lastBlinkTime = 0;
uint32_t penaltyStartTime = 0; 
uint32_t lastEarlyPressTime = 0; // Tracks the last time they pressed early
bool blinkState = false;

void setup() {
  Serial.begin(115200);
  setRGBLedColor(COLOR_BOOTING);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_LED, LOW);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  // The Coordinator turning this ON means the round has started
  internalLedEP.onLightChange([](bool state){
    isRoundActive = state;
    
    if (state) {
      // Round is starting! Evaluate penalties.
      
      // Forgiveness timer: If they haven't touched the button in the last 3 seconds, 
      // they were likely just testing it earlier, not actively spamming the start.
      if (millis() - lastEarlyPressTime > 3000) {
        earlyPressCount = 0;
      }

      if (earlyPressCount >= 2) {
        isPenalized = true;
        penaltyStartTime = millis(); 
        Serial.println("Contestant penalized for early spamming!");
      } else {
        isPenalized = false;
        digitalWrite(PIN_BUTTON_LED, HIGH);
      }
    } else {
      // Round ended / Returned to IDLE
      isPenalized = false;
      earlyPressCount = 0;
      digitalWrite(PIN_BUTTON_LED, LOW);
    }
  });

  Zigbee.addEndpoint(&internalLedEP);

  connectZigbeeED();
  
  Serial.printf("Contestant %d Ready.\n", CONTESTANT_ID);
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  // --- Hardware Debounce Variables ---
  static bool lastReading = HIGH;
  static bool buttonState = HIGH;
  static uint32_t lastDebounceTime = 0;
  
  bool reading = digitalRead(PIN_BUTTON);

  // Reset debounce timer if the physical signal bounces
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  // Once the signal is stable for 50ms, evaluate it
  if ((millis() - lastDebounceTime) > 50) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // Execute logic on the falling edge (button pressed)
      if (buttonState == LOW) {
        
        if (!isRoundActive) {
          // Spamming before the round started (or pressing during Test Mode)
          earlyPressCount++;
          lastEarlyPressTime = millis();
          Serial.printf("Early press! Count: %d\n", earlyPressCount);
          
          // FIX: We must STILL send the command so Test Mode works!
          // The Coordinator natively ignores this if it is in IDLE mode.
          sendQuizCommand(0x0000, CONTESTANT_ID, 1, ESP_ZB_ZCL_CMD_ON_OFF_ON_ID);
        } 
        else if (isRoundActive && !isPenalized) {
          // Valid, legal buzz
          digitalWrite(PIN_BUTTON_LED, LOW);
          
          // Lock out locally immediately
          isRoundActive = false; 
          
          sendQuizCommand(0x0000, CONTESTANT_ID, 1, ESP_ZB_ZCL_CMD_ON_OFF_ON_ID);
          Serial.printf("Contestant %d Buzzed!\n", CONTESTANT_ID);
        }
      }
    }
  }
  lastReading = reading;

  // --- Non-Blocking Penalty Blink ---
  if (isPenalized && isRoundActive) {
    if (millis() - penaltyStartTime < 10000) {
      // Blink fast for the first 10 seconds
      if (millis() - lastBlinkTime > 100) { 
        lastBlinkTime = millis();
        blinkState = !blinkState;
        digitalWrite(PIN_BUTTON_LED, blinkState);
      }
    } else {
      // After 10 seconds, force the LED off
      digitalWrite(PIN_BUTTON_LED, LOW);
    }
  }

  checkFactoryResetButton();
  delay(10);
}
