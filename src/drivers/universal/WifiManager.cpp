//
// Created by urzu-7 on 9/1/26.
//

#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"
#include "config/build_flags.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "drivers/universal/SerialMonitor.h"
#include "WifiManager.h"

bool WifiManager::start_ap = false;
bool WifiManager::sta_connected = false;
const char *WifiManager::hostname = "esp32-unset-host";
IPAddress WifiManager::server_ip = IPAddress(192, 168, 4, 1);

void WifiManager::setupMdns() {
    if (!MDNS.begin(hostname)) {
        Serial.println("MDNS responder not available");
        return;
    }
    MDNS.addService("mqtt", "tcp", 1883);

    Serial.println("MDNS responder started: " + String(hostname) + ".local");
}

void WifiManager::addService(const char *serviceName, const char *serviceType, size_t port) {
    MDNS.addService(serviceName, serviceType, port);
}

bool WifiManager::connectToNetwork() {
    String ssid;
    String pass;
    if (forced_sta) {
        ssid = forced_sta_ssid;
        pass = forced_sta_password;
    } else {
#ifdef SECRET_SSID
        ssid = prefs.getString("ssid", SECRET_SSID);
        pass = prefs.getString("pass", SECRET_PASSWORD);

#else
        ssid = prefs.getString("ssid");
        pass = prefs.getString("pass");
#endif
    }

    if (ssid.length() == 0) {
        return false;
    }

    // --- Connect to Wi-Fi ---
    Serial.print("📶 Connecting to Wi-Fi");
    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFiClass::status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFiClass::status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        server_ip = WiFi.localIP();
        Serial.print("📡 IP address: ");
        Serial.println(server_ip);
        setupMdns();
        return true;
    }
    Serial.println("\n❌ Failed to connect to Wi-Fi");
    Serial.println("SSID: " + ssid);
    Serial.println("PASS: " + pass);
    WiFi.disconnect();
    return false;
}

bool WifiManager::startAccessPoint(const String &ap_ssid, const String &ap_password) {
    const auto gateway = IPAddress(192, 168, 4, 1);
    const auto subnet = IPAddress(255, 255, 255, 0);

    WiFi.softAPConfig(server_ip, gateway, subnet);

    if (WiFi.softAP(ap_ssid, ap_password, 1, 0, 4)) {
        SerialMonitor::writeLine("AP started");
        SerialMonitor::write("Network name: ");
        SerialMonitor::writeLine(ap_ssid);
        SerialMonitor::write("Password: ");
        SerialMonitor::writeLine(ap_password);
        SerialMonitor::write("IP address: ");
        SerialMonitor::writeLine(server_ip.toString());
        setupMdns();
        return true;
    }
    Serial.println("AP failed to start");
    return false;
}

bool WifiManager::setup(const String &ap_ssid, const String &ap_password) {
    sta_connected = false;

    if (forced_ap) {
        start_ap = true;
        return startAccessPoint(ap_ssid, ap_password);
    }

    if (forced_sta) {
        start_ap = false;
        sta_connected = connectToNetwork();
        if (sta_connected) {
            return true;
        }
        return startAccessPoint(ap_ssid, ap_password);
    }

    prefs.begin("wifi", false);
    start_ap = prefs.getBool("start_ap", false);
    if (!start_ap) {
        sta_connected = connectToNetwork();
    }
    prefs.end();

    if (sta_connected) {
        return true;
    }
    return startAccessPoint(ap_ssid, ap_password);
}

String WifiManager::getSerializedState() {
    JsonDocument jsonDoc;
    jsonDoc["start_ap"] = start_ap;
    jsonDoc["sta_connected"] = sta_connected;
    String state;
    serializeJson(jsonDoc, state);
    return state;
}

IPAddress WifiManager::getServerIP() {
    return server_ip;
}


void WifiManager::wifiConfigJsonCallback(const JsonDocument &doc) {
    prefs.begin("wifi", false);
    if (doc["start_ap"].is<bool>()) {
        start_ap = doc["start_ap"];
        prefs.putBool("start_ap", start_ap);
        const auto mode = start_ap ? "AP" : "STA";
        SerialMonitor::writeLine("WiFi starter mode set to: " + String(mode));
        if (start_ap) {
            prefs.end();
            return;
        }
    }

    if (!doc["ssid"].is<const char *>() || !doc["pass"].is<const char *>()) {
        // SerialQueue::enqueueLine("Missing 'ssid' or 'pass' in JSON payload");
        prefs.end();
        return;
    }

    const char *ssid = doc["ssid"];
    const char *pass = doc["pass"];

    if (strlen(ssid) == 0) {
        SerialMonitor::writeLine("SSID is empty");
        prefs.end();
        return;
    }

    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    SerialMonitor::writeLine("WiFi credentials overwritten");
    prefs.end();
}
