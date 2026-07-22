#ifndef ZIGBEE_MODE_ED
#error "Select 'Zigbee ED (End Device)' in Tools -> Zigbee mode"
#endif

#include <..\QuizCommon\QuizCommon.h>

ZigbeeLight whiteLedEP(2);

void setup() {
  Serial.begin(115200);
  setRGBLedColor(COLOR_BOOTING);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_LED, LOW);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  Zigbee.addEndpoint(&whiteLedEP);

  connectZigbeeED();
  
  // Ready to play: Turn White LED ON
  digitalWrite(PIN_BUTTON_LED, HIGH);
  Serial.println("Host Controller Ready.");
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  static bool lastState = HIGH;
  static uint32_t pressStart = 0;
  bool currentState = digitalRead(PIN_BUTTON);

  if (currentState == LOW && lastState == HIGH) {
    pressStart = millis();
  } 
  else if (currentState == HIGH && lastState == LOW) {
    uint32_t duration = millis() - pressStart;
    
    if (duration >= 50) { // Debounce threshold
      if (hostState == HOST_IDLE) {
        if (duration >= 2000) {
          // Long Press -> Enter TEST MODE
          hostState = HOST_TEST;
          digitalWrite(PIN_BUTTON_LED, HIGH); 
          sendQuizCommand(0x0000, 6, 2, ESP_ZB_ZCL_CMD_ON_OFF_ON_ID);
          Serial.println("Host: Entering Test Mode");
        } else {
          // Single Press -> Enter GAME MODE
          hostState = HOST_GAME;
          digitalWrite(PIN_BUTTON_LED, LOW); // Turn off white LED while game round is active
          sendQuizCommand(0x0000, 5, 2, ESP_ZB_ZCL_CMD_ON_OFF_ON_ID);
          Serial.println("Host: Entering Game Mode");
        }
      } 
      else if (hostState == HOST_GAME) {
        // Single Press from Game Mode -> Return to IDLE
        if (duration < 2000) {
          hostState = HOST_IDLE;
          digitalWrite(PIN_BUTTON_LED, HIGH); // Ready to play -> ON
          sendQuizCommand(0x0000, 5, 2, ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);
          Serial.println("Host: Returning to IDLE from Game");
        }
      } 
      else if (hostState == HOST_TEST) {
        // Single Press from Test Mode -> Return to IDLE
        if (duration < 2000) {
          hostState = HOST_IDLE;
          digitalWrite(PIN_BUTTON_LED, HIGH); // Ready to play -> ON
          sendQuizCommand(0x0000, 6, 2, ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);
          Serial.println("Host: Returning to IDLE from Test");
        }
      }
    }
  }
  
  lastState = currentState;
  checkFactoryResetButton();
  delay(10);
}
