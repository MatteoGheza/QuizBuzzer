#include <..\QuizCommon\QuizCommon.h>

bool pendingOTA = false; // Flag to safely trigger Wi-Fi in the main loop

unsigned long gameStartTime = 0;

bool contestantsWithPenalty[4] = {false, false, false, false};

void clearCentralLeds() {
  digitalWrite(PIN_COORDINATOR_RED, LOW);
  digitalWrite(PIN_COORDINATOR_YEL, LOW);
  digitalWrite(PIN_COORDINATOR_GRN, LOW);
  digitalWrite(PIN_COORDINATOR_BLU, LOW);
}

void handleContestantPress(uint8_t contestantId, uint8_t pin) {
  if (contestantsWithPenalty[contestantId-1]) {
    Serial.printf("GAME: Ignoring contestant %d (penalty).\n", contestantId);
  } else {
    if (currentState != IDLE) {
      unsigned long endTime = micros();
      float durationMs = (endTime - gameStartTime) / 1000.0f;
      Serial.printf("GAME: Contestant %d responded in %.4f ms.\n", contestantId, durationMs);
    }
    if (currentState == GAME_ARMED) {
      currentState = GAME_WON; 
      
      digitalWrite(pin, HIGH);
      #if USE_BUZZER
      tone(PIN_COORDINATOR_BUZZER, 2000, 400); 
      #endif

      // Broadcast IDLE command to instantly turn off all contestant LEDs
      QuizMessage outMsg = {0, 0, CMD_IDLE};
      esp_now_send(macBroadcast, (uint8_t *) &outMsg, sizeof(outMsg));

      Serial.printf("GAME: Winner is Contestant %d!\n", contestantId);

    }
  }
  if (currentState == TEST_MODE) {
    digitalWrite(pin, HIGH);
    
    #if USE_BUZZER
    tone(PIN_COORDINATOR_BUZZER, 1200, 100);
    delay(120);
    tone(PIN_COORDINATOR_BUZZER, 1200, 100);
    #endif
    
    Serial.printf("TEST: Button %d pressed.\n", contestantId);
  }
}

// Callback for incoming messages (from Host or Contestants)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  QuizMessage msg;
  memcpy(&msg, incomingData, sizeof(msg));

  // --- From Host ---
  if (msg.senderType == 1) { 
    if (msg.command == CMD_ARM) {
      currentState = GAME_ARMED;
      clearCentralLeds();
      #if USE_BUZZER
      tone(PIN_COORDINATOR_BUZZER, 1500, 100); 
      #endif

      QuizMessage outMsg = {0, 0, CMD_ARM};
      esp_now_send(macBroadcast, (uint8_t *) &outMsg, sizeof(outMsg));
      gameStartTime = micros();
      Serial.println("MODE: Game Armed - Waiting for buzzers...");
    } 
    else if (msg.command == CMD_IDLE) {
      currentState = IDLE;
      clearCentralLeds();
      setRGBLedColor(COLOR_OFF);
      QuizMessage outMsg = {0, 0, CMD_IDLE};
      esp_now_send(macBroadcast, (uint8_t *) &outMsg, sizeof(outMsg));
      Serial.println("MODE: Reset to IDLE");

      contestantsWithPenalty[0] = false;
      contestantsWithPenalty[1] = false;
      contestantsWithPenalty[2] = false;
      contestantsWithPenalty[3] = false;
    } 
    else if (msg.command == CMD_TEST_ENTER) {
      currentState = TEST_MODE;
      clearCentralLeds();
      setRGBLedColor(COLOR_TEST_MODE);
      QuizMessage outMsg = {0, 0, CMD_IDLE}; // Keep contestant lights off in test
      esp_now_send(macBroadcast, (uint8_t *) &outMsg, sizeof(outMsg));
      #if USE_BUZZER
      tone(PIN_COORDINATOR_BUZZER, 800, 150);
      delay(200);
      tone(PIN_COORDINATOR_BUZZER, 800, 150);
      #endif
      Serial.println("MODE: Test Mode Active");
    }
    else if (msg.command == CMD_ENABLE_OTA) {
      // Forward command to all contestants
      QuizMessage outMsg = {0, 0, CMD_ENABLE_OTA};
      esp_now_send(macBroadcast, (uint8_t *) &outMsg, sizeof(outMsg));
      
      // Defer local Wi-Fi startup to the main loop to prevent crashing the Wi-Fi task
      pendingOTA = true;
    }
  } 
  // --- From Contestant ---
  else if (msg.senderType == 2 && msg.command == CMD_BUZZ) {
    uint8_t pin = 0;
    if (msg.senderId == 1) pin = PIN_COORDINATOR_RED;
    else if (msg.senderId == 2) pin = PIN_COORDINATOR_YEL;
    else if (msg.senderId == 3) pin = PIN_COORDINATOR_GRN;
    else if (msg.senderId == 4) pin = PIN_COORDINATOR_BLU;

    if (pin != 0) {
      handleContestantPress(msg.senderId, pin);
    }
  }
  else if (msg.senderType == 2 && msg.command == CMD_PENALTY) {
    contestantsWithPenalty[msg.senderId-1] = true;
    Serial.printf("GAME: Contestant %d received penalty.\n", msg.senderId);
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

  initBaseRadios();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    setRGBLedColor(COLOR_ERROR);
    while(1);
  }

  esp_now_register_recv_cb(OnDataRecv);
  registerPeer(macBroadcast); 

  Serial.println("Coordinator Ready.");
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  // Process deferred OTA startup safely outside the ESP-NOW callback
  if (pendingOTA) {
    pendingOTA = false;
    delay(100); // Give the ESP-NOW broadcast a tiny bit of time to physically transmit
    startWiFiAndOTA("Quiz-Coordinator");
  }

  checkBootButtonForOTA("Quiz-Coordinator");
  handleOTA();
  delay(50);
}
