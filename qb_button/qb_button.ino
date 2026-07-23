#include <..\QuizCommon\QuizCommon.h>

// --- Game Logic Variables ---
bool isRoundActive = false;
bool isPenalized = false;
uint8_t earlyPressCount = 0;
uint8_t contestantId = 0; // Dynamically resolved from MAC address

// --- Blinking & Timing Variables ---
uint32_t lastBlinkTime = 0;
uint32_t penaltyStartTime = 0; 
uint32_t lastEarlyPressTime = 0;
bool blinkState = false;
String hostNameStr;
bool pendingOTA = false; // Flag to safely trigger Wi-Fi in the main loop

// Callback for incoming commands from Coordinator
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  QuizMessage msg;
  memcpy(&msg, incomingData, sizeof(msg));

  if (msg.senderType == 0) { // Message from Coordinator
    if (msg.command == CMD_ARM) {
      isRoundActive = true;
      if (millis() - lastEarlyPressTime > 3000) earlyPressCount = 0;
      
      if (earlyPressCount >= 2) {
        isPenalized = true;
        penaltyStartTime = millis();
        Serial.println("Contestant penalized for early spamming!");
      } else {
        isPenalized = false;
        digitalWrite(PIN_BUTTON_LED, HIGH);
      }
    } 
    else if (msg.command == CMD_IDLE) {
      isRoundActive = false;
      isPenalized = false;
      earlyPressCount = 0;
      digitalWrite(PIN_BUTTON_LED, LOW);
    }
    else if (msg.command == CMD_ENABLE_OTA) {
      // Defer local Wi-Fi startup to the main loop
      pendingOTA = true;
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

  // --- Auto-detect Contestant ID from MAC Address ---
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  Serial.print("Board MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X%s", mac[i], (i < 5) ? ":" : "\n");
  }

  for (int i = 0; i < 4; i++) {
    if (memcmp(mac, macContestants[i], 6) == 0) {
      contestantId = i + 1; // Maps array index 0-3 to Contestant ID 1-4
      break;
    }
  }

  if (contestantId == 0) {
    Serial.println("ERROR: This board's MAC address is NOT found in macContestants!");
    setRGBLedColor(COLOR_ERROR);
    while (1) { delay(100); } // Halt execution if unassigned
  }

  hostNameStr = "Quiz-Button-" + String(contestantId);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    setRGBLedColor(COLOR_ERROR);
    while(1);
  }

  esp_now_register_recv_cb(OnDataRecv);
  registerPeer(macCoordinator);
  
  Serial.printf("Contestant %d Ready.\n", contestantId);
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  static bool lastReading = HIGH;
  static bool buttonState = HIGH;
  static uint32_t lastDebounceTime = 0;

  // Process deferred OTA startup safely outside the ESP-NOW callback
  if (pendingOTA) {
    pendingOTA = false;
    startWiFiAndOTA(hostNameStr.c_str());
  }
  
  checkBootButtonForOTA(hostNameStr.c_str());
  
  bool reading = digitalRead(PIN_BUTTON);
  if (reading != lastReading) lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > 50) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {
        if (!isRoundActive) {
          earlyPressCount++;
          lastEarlyPressTime = millis();
          QuizMessage outMsg = {2, contestantId, CMD_BUZZ};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
        } 
        else if (isRoundActive && !isPenalized) {
          digitalWrite(PIN_BUTTON_LED, LOW);
          isRoundActive = false; 
          QuizMessage outMsg = {2, contestantId, CMD_BUZZ};
          esp_now_send(macCoordinator, (uint8_t *) &outMsg, sizeof(outMsg));
        }
      }
    }
  }
  lastReading = reading;

  // Non-Blocking Penalty Blink
  if (isPenalized && isRoundActive) {
    if (millis() - penaltyStartTime < 10000) {
      if (millis() - lastBlinkTime > 100) { 
        lastBlinkTime = millis();
        blinkState = !blinkState;
        digitalWrite(PIN_BUTTON_LED, blinkState);
      }
    } else {
      digitalWrite(PIN_BUTTON_LED, LOW);
    }
  }
  
  handleOTA();
  delay(10);
}
