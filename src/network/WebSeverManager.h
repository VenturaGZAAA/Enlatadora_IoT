#ifndef ENLATADORA_IOT_DASHBOARDSERVER_H
#define ENLATADORA_IOT_DASHBOARDSERVER_H

#define SD_CARD_CONNECTED

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

class WebSeverManager {
    static AsyncWebServer server;
#ifdef SD_CARD_CONNECTED
    static const inline String webPath = "/web";
#endif
    static void handleFileRequest(AsyncWebServerRequest *request);
    static void handleFavicon(AsyncWebServerRequest *request);

    static String getContentType(const String &filename);

    static const char *getEncoding(const AsyncWebServerRequest *request);
#ifdef SD_CARD_CONNECTED
    static inline std::function<void()> onRootRequested = nullptr;

#endif

    static inline  std::function<void()> onUpdateSuccessful = nullptr;

public:
    WebSeverManager() = delete;

    static void registerJsonPostEndpoint(const String& path,const std::function<void(const JsonDocument &)> &callback, const char *onError = "undefined");

    static void enableOTASupport(const char *username, const char *password,const char *path = "/firmware");
    static void setup();

    static void setOnUpdateSuccessful(std::function<void()> callback);
#ifdef SD_CARD_CONNECTED
    static void setOnRootRequested(std::function<void()> callback);
#endif
};

#endif
