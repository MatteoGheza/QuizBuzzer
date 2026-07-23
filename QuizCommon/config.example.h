#define PIN_COORDINATOR_RED    2
#define PIN_COORDINATOR_YEL    3
#define PIN_COORDINATOR_GRN    14
#define PIN_COORDINATOR_BLU    23
#define PIN_COORDINATOR_BUZZER 22

#define PIN_BUTTON    12
#define PIN_BUTTON_LED 2

#define PIN_BOOT 9

#define USE_BUZZER 1

#define EARLY_PRESS_WINDOW 5000
#define MAX_EARLY_PRESSES 2
#define PENALTY_DURATION 10000

#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define OTA_PASSWORD "password"

// --- HARD-CODED MAC ADDRESSES ---
const uint8_t macCoordinator[] = {0xFC, 0x01, 0x2C, 0xF2, 0xCA, 0xEC};
const uint8_t macHost[]        = {0xFC, 0x01, 0x2C, 0xF2, 0xD0, 0x3C};

// Contestant MACs (1 = Red, 2 = Yel, 3 = Grn, 4 = Blu)
const uint8_t macContestants[4][6] = {
  {0xFC, 0x01, 0x2C, 0xF2, 0xD0, 0x98}, 
  {0xFC, 0x01, 0x2C, 0xF2, 0xCA, 0x7C}, 
  {0xFC, 0x01, 0x2C, 0xF2, 0xC9, 0x48}, 
  {0xFC, 0x01, 0x2C, 0xF2, 0xCE, 0x84}  
};
