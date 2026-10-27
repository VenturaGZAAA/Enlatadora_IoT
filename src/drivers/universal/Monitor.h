//
// Created by urzu-7 on 8/31/26.
//

#ifndef ENLATADORA_IOT_SERIALQUEUE_H
#define ENLATADORA_IOT_SERIALQUEUE_H

#include <string>
#include <functional>
#include <atomic>

#include <ArduinoJson.h>
#include "SerialManager.h"
#if __has_include(<ESPAsyncWebServer.h>)
#include <ESPAsyncWebServer.h>
#define ENABLE_USE_ASYNC_WEBSOCKET_SERVER
#endif


class Monitor {
    static inline bool enabledAsyncWebSocketServer = false;

    static inline auto serialInstance = SerialManager::reserve();
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    static inline std::unique_ptr<AsyncWebSocket> ws;
    static void onWsEvent(AsyncWebSocket *server, const AsyncWebSocketClient *client, AwsEventType type,
                        void *arg, uint8_t *data, size_t len);
#endif

    static inline  std::atomic<bool> isInitialized {false};
public:
    Monitor() = delete;

    static void write(const std::string &message);

    static void writeLine(const std::string &message);

    static void write(const String &message);

    static void writeLine(const String &message);

    static void write(const char *str);

    static void writeLine(const char *str);

    static void registerCallback(const std::function<void(const char *)> &callback);

    static void registerJsonCallback(const std::function<void(JsonDocument)> &callback);

    static void useSerialManager(std::unique_ptr<SerialManager> serialManager);

#ifdef  ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    static AsyncWebSocket &enableAsyncWebSocketHandler(const char * path = "/ws");
#endif


    static void init();
};


#endif //ENLATADORA_IOT_SERIALQUEUE_H
