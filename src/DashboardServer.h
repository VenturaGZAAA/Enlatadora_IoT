//
// Created by urzu-7 on 8/25/26.
//

#ifndef ENLATADORA_IOT_DASHBOARDSERVER_H
#define ENLATADORA_IOT_DASHBOARDSERVER_H

#include <WiFi.h>
#define HTTP_HEADER_BUFFER_SIZE 2048   // or 2048
#define HTTP_HEADER_COUNT 20
#include <WebServer.h>
#include <SdFat.h>

class DashboardServer {
    static WebServer server;
    static SdFat sd;
    static SdFile filesy;
    static bool success;
    static const char* getEncoding();
    static void handleFileRequest();
    static void handleFavicon();
    static String getContentType(const String &filename);
    static void streamFile(SdFile &streamed_file, const String &contentType);
    static void streamCompressedFile(SdFile &fileTarget, const String &contentType, uint32_t originalSize);
public:
    static void setup();
    static void loop();

};

#endif //ENLATADORA_IOT_DASHBOARDSERVER_H
