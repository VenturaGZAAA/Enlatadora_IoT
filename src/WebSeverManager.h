#ifndef ENLATADORA_IOT_DASHBOARDSERVER_H
#define ENLATADORA_IOT_DASHBOARDSERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
// #include <SdFat.h>

class WebSeverManager {
    static AsyncWebServer server;
    // static SdFat sd;
    static inline bool mounted = false;
    static const inline String webPath = "/web";

    static void handleFileRequest(AsyncWebServerRequest *request);

    static void handleFavicon(AsyncWebServerRequest *request);

    static String getContentType(const String &filename);

    static const char *getEncoding(const AsyncWebServerRequest *request);

    static inline std::function<void()> onRootRequested = nullptr;

public:
    static void setup();

    static void setOnRootRequested(std::function<void()> callback);

    static bool isMounted();
};

#endif
