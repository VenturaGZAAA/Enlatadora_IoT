//
// Created by urzu-7 on 8/25/26.
//

#ifndef ENLATADORA_IOT_DASHBOARDSERVER_H
#define ENLATADORA_IOT_DASHBOARDSERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <SdFat.h>

class DashboardServer {
    static WebServer server;
    static SdFat sd;
    static SdFile file;
    static bool success;

    static String getContentType(const String& filename);
    static void streamFile(SdFile& streamed_file, const String& contentType);
    static void handleFileRequest();
    static void handleFavicon();
public:
    static void setup();
    static void loop();
};


#endif //ENLATADORA_IOT_DASHBOARDSERVER_H
