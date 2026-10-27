//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "Monitor.h"

void Monitor::useSerialManager(std::unique_ptr<SerialManager> serialManager) {
    if (!serialInstance) {
        serialInstance = std::move(serialManager);
    }
}

#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER

AsyncWebSocket &Monitor::enableAsyncWebSocketHandler(const char *path) {
    if (enabledAsyncWebSocketServer && ws)
        return *ws;

    enabledAsyncWebSocketServer = true;
    ws = std::make_unique<AsyncWebSocket>(path);
    ws->onEvent(onWsEvent);
    return *ws;
}

void Monitor::onWsEvent(AsyncWebSocket *server, const AsyncWebSocketClient *client, AwsEventType type, void *arg,
                        uint8_t *data, const size_t len) {
    if (!isInitialized.load())
        return;

    switch (type) {
        case WS_EVT_CONNECT:
            if (serialInstance && client) {
                serialInstance->writeLine("Ws client connected: " + client->remoteIP().toString());
            }
            break;
        case WS_EVT_DISCONNECT:
            if (serialInstance && client) {
                serialInstance->writeLine("Ws client disconnected: " + client->remoteIP().toString());
            }
            break;
        case WS_EVT_PING:
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
        case WS_EVT_DATA: {
            // the registered callbacks are not going to be called yet in this test.
            // we are only going to echo them to the serial instance and the clients.
            if (const auto *info = static_cast<AwsFrameInfo *>(arg); info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                const std::string rawMessage(reinterpret_cast<const char *>(data), len);
                const String message = rawMessage.c_str();
                if (ws) {
                    ws->textAll("Test echo: " + message);
                }
                if (serialInstance) {
                    serialInstance->writeLine("WS test echo: " + message);
                }
            }
            break;
        }
    }
}

#endif

void Monitor::init() {
    if (serialInstance) {
        serialInstance->init();
    }
    isInitialized = true;
}

void Monitor::write(const std::string &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(message.c_str());
    }
#endif
}

void Monitor::write(const String &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(message);
    }
#endif
}

void Monitor::write(const char *str) {
    if (serialInstance) {
        serialInstance->write(str);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(str);
    }
#endif
}

void Monitor::writeLine(const std::string &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(message.c_str());
    }
#endif
}

void Monitor::writeLine(const String &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(message);
    }
#endif
}

void Monitor::writeLine(const char *str) {
    if (serialInstance) {
        serialInstance->writeLine(str);
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer && ws) {
        ws->textAll(str);
    }
#endif
}

// We are not implementing WebSockets callbacks yet
void Monitor::registerCallback(const std::function<void(const char *)> &callback) {
    if (serialInstance) {
        serialInstance->registerCallback(callback);
    }
}

void Monitor::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    if (serialInstance) {
        serialInstance->registerJsonCallback(callback);
    }
}
