//
// Created by urzu-7 on 9/1/26.
//

#ifndef ENLATADORA_IOT_WIFIMANAGER_H
#define ENLATADORA_IOT_WIFIMANAGER_H

#include <Arduino.h>
#include <Preferences.h>

static Preferences prefs;

class WifiManager {
    static bool start_ap;
    static bool sta_connected;
    static const char *hostname;

    static bool connectToNetwork();
    static bool startAccessPoint();
    static void setupMdns();

    public:
    WifiManager() = delete;
    static void wifiConfigCallback(const char *payload);
    static bool setup();
    static void setAPMode(bool enable);
    static String getState();
};


#endif //ENLATADORA_IOT_WIFIMANAGER_H
