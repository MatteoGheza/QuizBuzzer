#ifndef QUIZ_COMMON_H
#define QUIZ_COMMON_H

#include <Arduino.h>
#include "Zigbee.h"
#include "esp_zigbee_core.h"

#include "config.h"


enum GameState { IDLE, GAME_ARMED, GAME_WON, TEST_MODE };
GameState currentState = IDLE;

enum HostState { HOST_IDLE, HOST_GAME, HOST_TEST };
HostState hostState = HOST_IDLE;


struct RGB {
    uint8_t r, g, b;
};

constexpr RGB COLOR_OFF   = {0, 0, 0};

constexpr RGB COLOR_BOOTING    = {0, 36, 36};
constexpr RGB COLOR_ERROR      = {102, 0, 0};
constexpr RGB COLOR_PAIRING    = {102, 0, 102};
constexpr RGB COLOR_RESETTING  = {76, 0, 54};
constexpr RGB COLOR_TEST_MODE  = {102, 102, 0};


// --- Set RGB led color ---
inline void setRGBLedColor(RGB color) {
  rgbLedWrite(RGB_BUILTIN, color.r, color.g, color.b);
}


// --- Shared Zigbee Command Helper ---
inline void sendQuizCommand(uint16_t dst_addr, uint8_t dst_ep, uint8_t src_ep, uint8_t cmd_id) {
  esp_zb_zcl_on_off_cmd_t req;
  memset(&req, 0, sizeof(req));
  req.zcl_basic_cmd.dst_addr_u.addr_short = dst_addr;
  req.zcl_basic_cmd.dst_endpoint = dst_ep;
  req.zcl_basic_cmd.src_endpoint = src_ep;
  req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  req.on_off_cmd_id = cmd_id;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_on_off_cmd_req(&req);
  esp_zb_lock_release();
}


// --- Reset Zigbee credentials and storage (for End Devices: Hold BOOT for 3s) ---
inline void factoryResetZigbee() {
  setRGBLedColor(COLOR_RESETTING);
  Serial.println("\n[RESET] Erasing Zigbee credentials and storage...");
  Zigbee.factoryReset(true); // Clears stack storage and reboots
  Serial.println("[RESET] Erase complete. Rebooting board...");
  delay(1000);
  ESP.restart();
}


// --- Connect to Zigbee Network (for End Devices) ---
inline void connectZigbeeED() {
  Serial.println("Starting Zigbee End Device...");
  if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to start!");
    setRGBLedColor(COLOR_ERROR);
    delay(2000);
    factoryResetZigbee();
    while(1);
  }
  Serial.println("Searching for Coordinator...");
  
  // Wait here until we successfully join the mesh
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("\nSuccessfully joined the network!");
}


// --- Universal Factory Reset Helper (For End Devices: Hold BOOT for 3s) ---
inline void checkFactoryResetButton() {
  if (digitalRead(PIN_BOOT) == LOW) {
    unsigned long startTime = millis();
    while (digitalRead(PIN_BOOT) == LOW) {
      if (millis() - startTime >= 3000) {
        factoryResetZigbee();
      }
      delay(50);
    }
  }
}


// --- Coordinator Button Helper (Short press = Open Network, Long press = Reset) ---
inline void handleCoordinatorButton() {
  if (digitalRead(PIN_BOOT) == LOW) {
    unsigned long startTime = millis();
    while (digitalRead(PIN_BOOT) == LOW) {
      delay(50);
    }
    unsigned long duration = millis() - startTime;

    if (duration >= 3000) {
      // Long Press -> Factory Reset
      factoryResetZigbee();
    } else if (duration >= 50) {
      // Short Press -> Open Zigbee Network for joining (180 seconds)
      Serial.println("\n[ZIGBEE] Short press: Opening network for pairing (180s)...");
      setRGBLedColor(COLOR_PAIRING);
      Zigbee.openNetwork(180);
      #if USE_BUZZER
      tone(PIN_COORDINATOR_BUZZER, 2500, 100); // Quick confirmation chirp
      #endif
    }
  }
}

#endif
