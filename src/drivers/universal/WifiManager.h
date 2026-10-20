//
// Created by urzu-7 on 9/1/26.
//

#ifndef ENLATADORA_IOT_WIFIMANAGER_H
#define ENLATADORA_IOT_WIFIMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

class WifiManager {
    static inline Preferences prefs;
    static bool start_ap;
    static inline bool forced_ap = false;
    static inline bool forced_sta = false;
    static inline String forced_sta_ssid = "";
    static inline String forced_sta_password = "";
    static bool sta_connected;
    static const char *hostname;
    static IPAddress server_ip;

    static bool connectToNetwork();

    static bool startAccessPoint(const String& ap_ssid, const String& ap_password);

    static void setupMdns();

public:
    WifiManager() = delete;

    static void addService(const char *serviceName, const char *serviceType, size_t port);

    static void wifiConfigJsonCallback(const JsonDocument &doc);

    static void forceAp() {
        forced_ap = true;
        forced_sta = false;
    }
    static void forceSTA(const String& ssid, const String& password) {
        forced_sta = true;
        forced_ap = false;
        forced_sta_ssid = ssid;
        forced_sta_password = password;
    }
    static bool setup(const String& ap_ssid= "", const String& ap_password = "");

    static bool isConnected() { return sta_connected; }
    static bool isFailedToConnect() { return !start_ap && !sta_connected; }


    static IPAddress getServerIP();

    static String getSerializedState();
};


#endif //ENLATADORA_IOT_WIFIMANAGER_H
