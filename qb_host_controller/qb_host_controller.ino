#include <..\QuizCommon\QuizCommon.h>

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  QuizMessage msg;
  if (len >= sizeof(QuizMessage)) {
    memcpy(&msg, incomingData, sizeof(msg));
    
    if (msg.command == CMD_SIMULATE_CLICK) {
      if (hostState == HOST_IDLE) {
          hostState = HOST_GAME;
          digitalWrite(PIN_BUTTON_LED, LOW); 
          QuizMessage outMsg = {1, 0, CMD_ARM};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Entering Game Mode (Remote Sim)");
      } 
      else if (hostState == HOST_GAME || hostState == HOST_TEST) {
          hostState = HOST_IDLE;
          digitalWrite(PIN_BUTTON_LED, HIGH); 
          QuizMessage outMsg = {1, 0, CMD_IDLE};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Returning to IDLE (Remote Sim)");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  setRGBLedColor(COLOR_BOOTING);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_LED, LOW);

  initBaseRadios();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    setRGBLedColor(COLOR_ERROR);
    while(1);
  }

  esp_now_register_recv_cb(OnDataRecv);
  registerPeer(macCoordinator);
  
  digitalWrite(PIN_BUTTON_LED, HIGH);
  Serial.println("Host Controller Ready.");
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  static bool lastState = HIGH;
  static uint32_t pressStart = 0;
  bool currentStateBtn = digitalRead(PIN_BUTTON);

  // Check for Boot Button override
  checkBootButtonForOTA("Quiz-Host-Controller");

  if (currentStateBtn == LOW && lastState == HIGH) {
    pressStart = millis(); // Button pressed
  } 
  else if (currentStateBtn == HIGH && lastState == LOW) {
    uint32_t duration = millis() - pressStart; // Button released
    
    if (duration >= 50 && duration < 6000) { 
      if (hostState == HOST_IDLE) {
        if (duration >= 1000) {
          hostState = HOST_TEST;
          digitalWrite(PIN_BUTTON_LED, HIGH); 
          QuizMessage outMsg = {1, 0, CMD_TEST_ENTER};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Entering Test Mode");
        } else {
          hostState = HOST_GAME;
          digitalWrite(PIN_BUTTON_LED, LOW); 
          QuizMessage outMsg = {1, 0, CMD_ARM};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Entering Game Mode");
        }
      } 
      else if (hostState == HOST_GAME) {
        if (duration < 1000) {
          hostState = HOST_IDLE;
          digitalWrite(PIN_BUTTON_LED, HIGH); 
          QuizMessage outMsg = {1, 0, CMD_IDLE};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Returning to IDLE from Game");
        }
      } 
      else if (hostState == HOST_TEST) {
        if (duration < 1000) {
          hostState = HOST_IDLE;
          digitalWrite(PIN_BUTTON_LED, HIGH); 
          QuizMessage outMsg = {1, 0, CMD_IDLE};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
          Serial.println("Host: Returning to IDLE from Test");
        }
      }
    }
  }
  // If button is currently being held down, check if it's been held for 6 seconds
  else if (currentStateBtn == LOW && lastState == LOW) {
    if (millis() - pressStart >= 6000) {
      Serial.println("Host: 6-Second Hold Detected! Triggering Global OTA...");
      QuizMessage outMsg = {1, 0, CMD_ENABLE_OTA};
      esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
      
      for(int i=0; i<3; i++) { 
        digitalWrite(PIN_BUTTON_LED, LOW); delay(150); 
        digitalWrite(PIN_BUTTON_LED, HIGH); delay(150); 
      }
      
      if (!otaEnabled) {
        // Now it is safe to reset the radio and connect to Wi-Fi
        startWiFiAndOTA("Quiz-Host-Controller");
      }
      
      // Wait for user to release the button
      while(digitalRead(PIN_BUTTON) == LOW) delay(10);
      currentStateBtn = HIGH; // Force state reset
    }
  }
  
  lastState = currentStateBtn;
  handleOTA();
  delay(10);
}
