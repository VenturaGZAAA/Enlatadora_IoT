//
// Created by urzu-7 on 9/1/26.
//

#ifndef ENLATADORA_IOT_WIFIMANAGER_H
#define ENLATADORA_IOT_WIFIMANAGER_H


class WifiManager {
    static bool start_ap;
    static bool sta_connected;
    static const char *hostname;

    static bool connectToNetwork();
    static bool startAccessPoint();
    static void setupMdns();

    public:
    WifiManager() = delete;
    static bool setup();
    static String getState();
};


#endif //ENLATADORA_IOT_WIFIMANAGER_H
