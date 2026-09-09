#ifndef ENLATADORA_IOT_DASHBOARDSERVER_H
#define ENLATADORA_IOT_DASHBOARDSERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SdFat.h>

class DashboardServer {
public:
    static void setup();

private:
    static AsyncWebServer server;
    static SdFat sd;

    static void handleFileRequest(AsyncWebServerRequest *request);
    static void handleFavicon(AsyncWebServerRequest *request);
    static String getContentType(const String &filename);
    static const char* getEncoding(const AsyncWebServerRequest *request);
};

#endif