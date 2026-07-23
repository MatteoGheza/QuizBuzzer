#include <..\QuizCommon\QuizCommon.h>

bool pendingOTA = false; // Flag to safely trigger Wi-Fi in the main loop

// Callback for incoming commands from Coordinator
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  // 1. Peek at the base message structure first
  QuizMessage msg;
  if (len >= sizeof(QuizMessage)) {
    memcpy(&msg, incomingData, sizeof(msg));
  } else return;

  if (msg.senderType == 0) { // Message from Coordinator
    
    if (msg.command == CMD_ENABLE_OTA) {
      pendingOTA = true;
    }
    else if (msg.command == CMD_IDLE) {
      Serial.println("#{\"type\":\"mode\",\"value\":\"idle\"}");
    }
    else if (msg.command == CMD_TEST_ENTER) {
      Serial.println("#{\"type\":\"mode\",\"value\":\"test\"}");
    }
    else if (msg.command == CMD_ARM) {
      Serial.println("#{\"type\":\"mode\",\"value\":\"arm\"}");
    }
    else if (msg.command == CMD_ROUND_SUMMARY) {
      if (len == sizeof(QuizSummaryMessage)) {
        QuizSummaryMessage summary;
        memcpy(&summary, incomingData, sizeof(summary));
        
        // Output a structured JSON string prefixed with '#' for Web Serial parsing
        Serial.print("#");
        Serial.printf("{\"type\":\"summary\",\"winnerId\":%d,\"results\":[", summary.winnerId);
        
        for (int i = 0; i < 4; i++) {
          Serial.printf("{\"id\":%d,\"time\":%.4f,\"penalty\":%s}%s", 
                         i + 1, 
                         summary.responseTimes[i], 
                         summary.penalties[i] ? "true" : "false",
                         (i < 3) ? "," : "");
        }
        Serial.println("]}");
        Serial.println("\n--- ROUND SUMMARY ---");
        Serial.printf("WINNER: Contestant %d\n", summary.winnerId);
        
        for (int i = 0; i < 4; i++) {
          Serial.printf("Contestant %d | Time: %.4f ms | Penalty: %s\n", 
                         i + 1, 
                         summary.responseTimes[i], 
                         summary.penalties[i] ? "YES" : "NO");
        }
        Serial.println("---------------------\n");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  setRGBLedColor(COLOR_BOOTING);

  initBaseRadios();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    setRGBLedColor(COLOR_ERROR);
    while(1);
  }

  esp_now_register_recv_cb(OnDataRecv);
  registerPeer(macHost);
  registerPeer(macCoordinator);
  
  digitalWrite(PIN_BUTTON_LED, HIGH);
  Serial.println("Bridge ready."); // Standard log
  Serial.println("#{\"type\":\"ready\"}"); // Event for JS web serial
  
  setRGBLedColor(COLOR_OFF);
}

void loop() {
  // Process deferred OTA startup safely outside the ESP-NOW callback
  if (pendingOTA) {
    pendingOTA = false;
    delay(100); // Give the ESP-NOW broadcast a tiny bit of time to physically transmit
    startWiFiAndOTA("Quiz-Bridge");
  }

  checkBootButtonForOTA("Quiz-Bridge");
  handleOTA();
  delay(50);
}
