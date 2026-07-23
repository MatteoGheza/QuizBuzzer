#ifndef QUIZ_COMMON_H
#define QUIZ_COMMON_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <esp_now.h>

#include "./config.h"

// --- Game States ---
enum GameState { IDLE, GAME_ARMED, GAME_WON, TEST_MODE };
GameState currentState = IDLE;

enum HostState { HOST_IDLE, HOST_GAME, HOST_TEST };
HostState hostState = HOST_IDLE;

// --- Colors ---
struct RGB { uint8_t r, g, b; };
constexpr RGB COLOR_OFF       = {0, 0, 0};
constexpr RGB COLOR_BOOTING   = {0, 36, 36};
constexpr RGB COLOR_ERROR     = {102, 0, 0};
constexpr RGB COLOR_OTA       = {102, 0, 102};
constexpr RGB COLOR_TEST_MODE = {102, 102, 0};

inline void setRGBLedColor(RGB color) {
  rgbLedWrite(RGB_BUILTIN, color.r, color.g, color.b);
}

const uint8_t macBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- ESP-NOW Protocol Structures ---
enum QuizCommand { CMD_IDLE, CMD_ARM, CMD_BUZZ, CMD_PENALTY, CMD_TEST_ENTER, CMD_ENABLE_OTA };

typedef struct QuizMessage {
  uint8_t senderType; // 0 = Coordinator, 1 = Host, 2 = Contestant
  uint8_t senderId;   // Contestant 1-4 (Ignored for Host/Coord)
  QuizCommand command;
} QuizMessage;

// --- ESP-NOW Helper ---
inline void registerPeer(const uint8_t *mac_addr) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac_addr, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
  }
}

// ==========================================
// --- Power Save & On-Demand OTA Logic ---
// ==========================================
bool otaEnabled = false;

// Call this once in setup() to configure ESP-NOW without connecting to a router
inline void initBaseRadios() {
  btStop(); // Disable Bluetooth to save battery
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Ensure we aren't burning power trying to connect to an AP
}

// Call this dynamically when the boot button or ESP-NOW command is received
inline void startWiFiAndOTA(const char* hostname) {
  if (otaEnabled) return; // Prevent triggering multiple times

  setRGBLedColor(COLOR_OTA);
  
  Serial.println("\n[System] Waking up Wi-Fi and initializing OTA...");
  
  WiFi.setHostname(hostname);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[Wi-Fi] Connecting to %s", WIFI_SSID);
  
  // Non-blocking wait for Wi-Fi (10 second connection timeout)
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Connected!");
    Serial.printf("[Wi-Fi] IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[Wi-Fi] Connection timed out.");
    return;
  }

  // --- Configure ArduinoOTA ---
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() { Serial.println("[OTA] Start updating..."); });
  ArduinoOTA.onEnd([]() { Serial.println("\n[OTA] Update Complete! Rebooting..."); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\n", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  otaEnabled = true;
  Serial.println("[OTA] Service initialized and waiting for uploads.");
}

// Helper to keep OTA handling concise in loop()
inline void handleOTA() {
  if (otaEnabled) {
    ArduinoOTA.handle();
  }
}

// Helper to check the physical BOOT pin for local override
inline void checkBootButtonForOTA(const char* hostname) {
  if (!otaEnabled && digitalRead(PIN_BOOT) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_BOOT) == LOW) {
      startWiFiAndOTA(hostname);
      // Wait for button release so it doesn't spam
      while(digitalRead(PIN_BOOT) == LOW) delay(10);
    }
  }
}

#endif
