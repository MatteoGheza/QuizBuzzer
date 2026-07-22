#include "WiFi.h"

void setup(){
    Serial.begin(115200);
    WiFi.mode(WIFI_MODE_STA);
    Serial.println("indirizzo MAC:"); 
    Serial.println(WiFi.macAddress());
}

void loop(){ 
}
