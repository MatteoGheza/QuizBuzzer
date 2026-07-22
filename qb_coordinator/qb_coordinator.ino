#ifndef ZIGBEE_MODE_ZCZR
#error "Select 'Zigbee ZCZR (Coordinator/Router)' in Tools -> Zigbee mode"
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>

#include <..\QuizCommon\QuizCommon.h>

ZigbeeLight epRed(1), epYel(2), epGrn(3), epBlu(4), epGameMode(5), epTestMode(6);

void clearCentralLeds() {
  digitalWrite(PIN_COORDINATOR_RED, LOW);
  digitalWrite(PIN_COORDINATOR_YEL, LOW);
  digitalWrite(PIN_COORDINATOR_GRN, LOW);
  digitalWrite(PIN_COORDINATOR_BLU, LOW);
}

void handleContestantPress(uint8_t contestantId, uint8_t pin, ZigbeeLight &ep) {
  if (currentState == GAME_ARMED) {
    currentState = GAME_WON; // First responder wins! Lock out other contestants.
    
    digitalWrite(pin, HIGH);
    #if USE_BUZZER
    tone(PIN_COORDINATOR_BUZZER, 2000, 400); // Winning chime
    #endif

    // Turn off all contestant button LEDs
    sendQuizCommand(0xFFFF, 1, 1, ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);

    Serial.printf("GAME: Winner is Contestant %d!\n", contestantId);

  } else if (currentState == TEST_MODE) {
    digitalWrite(pin, HIGH);
    
    #if USE_BUZZER
    // Distinct test mode button-press chime (two quick 1200Hz beeps)
    tone(PIN_COORDINATOR_BUZZER, 1200, 100);
    delay(120);
    tone(PIN_COORDINATOR_BUZZER, 1200, 100);
    #endif
    
    Serial.printf("TEST: Button %d pressed.\n", contestantId);

    // Reset this specific endpoint so it can be pressed repeatedly in test mode
    ep.setLight(false);

    setRGBLedColor(COLOR_OFF);
  }
}

void setup() {
  Serial.begin(115200);
  setRGBLedColor(COLOR_BOOTING);
  pinMode(PIN_COORDINATOR_RED, OUTPUT);
  pinMode(PIN_COORDINATOR_YEL, OUTPUT);
  pinMode(PIN_COORDINATOR_GRN, OUTPUT);
  pinMode(PIN_COORDINATOR_BLU, OUTPUT);
  pinMode(PIN_COORDINATOR_BUZZER, OUTPUT);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  clearCentralLeds();

  // Contestant buzz inputs mapped with reference to their respective endpoint
  epRed.onLightChange([](bool s){ if(s) handleContestantPress(1, PIN_COORDINATOR_RED, epRed); });
  epYel.onLightChange([](bool s){ if(s) handleContestantPress(2, PIN_COORDINATOR_YEL, epYel); });
  epGrn.onLightChange([](bool s){ if(s) handleContestantPress(3, PIN_COORDINATOR_GRN, epGrn); });
  epBlu.onLightChange([](bool s){ if(s) handleContestantPress(4, PIN_COORDINATOR_BLU, epBlu); });

  // Game Mode (EP 5): ON = Arm Game, OFF = Return to IDLE
  epGameMode.onLightChange([](bool s){
    if (s) {
      currentState = GAME_ARMED;
      clearCentralLeds();
      
      // Clear endpoint states so subsequent matches successfully trigger callbacks
      epRed.setLight(false);
      epYel.setLight(false);
      epGrn.setLight(false);
      epBlu.setLight(false);

      #if USE_BUZZER
      tone(PIN_COORDINATOR_BUZZER, 1500, 100); // Game start beepù
      #endif
      sendQuizCommand(0xFFFF, 1, 1, ESP_ZB_ZCL_CMD_ON_OFF_ON_ID);
      Serial.println("MODE: Game Armed - Waiting for buzzers...");
    } else {
      currentState = IDLE;
      clearCentralLeds();
      sendQuizCommand(0xFFFF, 1, 1, ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);
      Serial.println("MODE: Reset to IDLE");
    }
  });

  // Test Mode (EP 6): ON = Enter Test, OFF = Return to IDLE
  epTestMode.onLightChange([](bool s){
    if (s) {
      currentState = TEST_MODE;
      clearCentralLeds();
      
      epRed.setLight(false);
      epYel.setLight(false);
      epGrn.setLight(false);
      epBlu.setLight(false);

      sendQuizCommand(0xFFFF, 1, 1, ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);

      #if USE_BUZZER
      // Distinct test mode entry chime (double low beep)
      tone(PIN_COORDINATOR_BUZZER, 800, 150);
      delay(200);
      tone(PIN_COORDINATOR_BUZZER, 800, 150);
      #endif

      Serial.println("MODE: Test Mode Active");

      setRGBLedColor(COLOR_TEST_MODE);
    } else {
      currentState = IDLE;
      clearCentralLeds();
      Serial.println("MODE: Reset to IDLE from Test");
      setRGBLedColor(COLOR_OFF);
    }
  });

  Zigbee.addEndpoint(&epRed);
  Zigbee.addEndpoint(&epYel);
  Zigbee.addEndpoint(&epGrn);
  Zigbee.addEndpoint(&epBlu);
  Zigbee.addEndpoint(&epGameMode);
  Zigbee.addEndpoint(&epTestMode);

  Serial.println("Starting Zigbee Coordinator...");
  
  // Start the network. This board becomes address 0x0000
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start!");
    setRGBLedColor(COLOR_ERROR);
    while(1);
  }
  Serial.println("Coordinator Ready.");
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  handleCoordinatorButton(); // Listens for short-press (open network) or long-press (reset)
  delay(50);
}
