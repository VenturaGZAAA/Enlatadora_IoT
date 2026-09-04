//
// Created by urzu-7 on 9/1/26.
//

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>
// #include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <SerialQueue.h>
#include "WifiManager.h"

#include "MqttServer.h"

bool WifiManager::start_ap = false;
bool WifiManager::sta_connected = false;
const char *WifiManager::hostname = "enlatadora-s3";

// void WifiManager::setupMdns()
// {
//     if (!MDNS.begin(hostname))
//     {
//         Serial.println("MDNS responder not available");
//         return;
//     }
//     MDNS.addService("mqtt", "tcp", 1883);
//     Serial.println("MDNS responder started: " + String(hostname) + ".local");
// }

bool WifiManager::connectToNetwork() {
    const auto ssid = prefs.getString("ssid", secret_ssid);
    const auto pass = prefs.getString("pass", secret_password);

    // --- Connect to Wi-Fi ---
    Serial.println("📶 Connecting to Wi-Fi");
    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("📡 IP address: ");
        Serial.println(WiFi.localIP().toString());
        // setupMdns();
        return true;
    }
    Serial.println("\n❌ Failed to connect to Wi-Fi");
    return false;
}

bool WifiManager::startAccessPoint() {
    const auto localIP = IPAddress(192, 168, 4, 1);
    const auto gateway = IPAddress(192, 168, 4, 1);
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
        // setupMdns();
        return true;
    }
    Serial.println("AP failed to start");
    return false;
}

bool WifiManager::setup() {
    prefs.begin("wifi", false);

    start_ap = prefs.getBool("start_ap", false);
    sta_connected = false;
    if (!start_ap) {
        sta_connected = connectToNetwork();
    }
    prefs.end();
    if (sta_connected) {
        return true;
    }
    return startAccessPoint();
}
String WifiManager::getState() {
    JsonDocument jsonDoc;
    jsonDoc["start_ap"] = start_ap;
    jsonDoc["sta_connected"] = sta_connected;
    String state;
    serializeJson(jsonDoc, state);
    return state;
}

void WifiManager::wifiStateCallback(const char *) {
    MqttServer::publish("config/wifi/state",getState());
}

void WifiManager::wifiConfigCallback(const char *payload) {
    SerialQueue::enqueueLine("Wifi data received: \t" + String(payload));
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        SerialQueue::enqueueLine("Failed to parse JSON payload");
        return;
    }

    prefs.begin("wifi", false);
    if (doc["start_ap"].is<bool>()) {
        start_ap = doc["start_ap"];
        prefs.putBool("start_ap", start_ap);
        const auto mode = start_ap ? "AP" : "STA";
        SerialQueue::enqueueLine("WiFi starter mode set to: " + String(mode));
        if (start_ap) {
            prefs.end();
            return;
        }
    }

    if (!doc["ssid"].is<const char *>() || !doc["pass"].is<const char *>()) {
        SerialQueue::enqueueLine("Missing 'ssid' or 'pass' in JSON payload");
        prefs.end();
        return;
    }

    const char *ssid = doc["ssid"];
    const char *pass = doc["pass"];

    if (strlen(ssid) == 0) {
        SerialQueue::enqueueLine("SSID is empty");
        prefs.end();
        return;
    }

    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    SerialQueue::enqueueLine("WiFi credentials overwritten");
    prefs.end();
}
