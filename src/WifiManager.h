//
// Created by urzu-7 on 9/1/26.
//

#ifndef ENLATADORA_IOT_WIFIMANAGER_H
#define ENLATADORA_IOT_WIFIMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
static Preferences prefs;

class WifiManager {
    static bool start_ap;
    static bool sta_connected;
    static const char *hostname;
    static IPAddress server_ip;

    static bool connectToNetwork();

    static bool startAccessPoint();

    static void setupMdns();

public:
    WifiManager() = delete;

    static void addService(const char *serviceName, const char *serviceType, size_t port);

    static void wifiConfigJsonCallback(const JsonDocument &doc);

    static bool setup();

    static bool isConnected() { return sta_connected; }
    static bool isFailedToConnect() { return !start_ap && !sta_connected; }


    static IPAddress getServerIP();

    static String getSerializedState();
};


#endif //ENLATADORA_IOT_WIFIMANAGER_H
