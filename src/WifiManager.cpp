//
// Created by urzu-7 on 9/1/26.
//

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>
// #include <SerialQueue.h>
#include "WifiManager.h"

bool WifiManager::start_ap = false;
bool WifiManager::sta_connected = false;

bool WifiManager::connectToNetwork() {
    // --- Connect to Wi-Fi ---
    Serial.println("📶 Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFiClass::status() != WL_CONNECTED && attempts < 30)
    {
        delay(500);
        Serial.println(".");
        attempts++;
    }
    if (WiFiClass::status() == WL_CONNECTED)
    {
        Serial.println("\n✅ WiFi connected!");
        Serial.println("📡 IP address: ");
        Serial.println(WiFi.localIP().toString());
        return true;
    }
    return false;
}
bool WifiManager::startAccessPoint() {
    const auto localIP =  IPAddress(192, 168, 4, 1);
    const auto gateway = IPAddress(192, 168,4,1);
    const auto subnet = IPAddress(255, 255, 255, 0);

    WiFi.softAPConfig(localIP, gateway, subnet);

    if (WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4)) {
        Serial.println("AP started");
        Serial.print("Network name: ");
        Serial.println(AP_SSID);
        Serial.print("Password: ");
        Serial.println(AP_PASSWORD);
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
        return true;
    }
    Serial.println("AP failed to start");
    return false;
}

bool WifiManager::setup() {
    start_ap = false;
    sta_connected = false;
    if (!start_ap) {
        sta_connected = connectToNetwork();
    }
   if (sta_connected) {
       return true;
   }
    return startAccessPoint();
}