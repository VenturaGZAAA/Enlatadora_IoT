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
#ifdef  ENABLE_USE_ASYNC_WEBSOCKET_SERVER

AsyncWebSocket &Monitor::enableAsyncWebSocketHandler(const char *path) {
    if (enabledAsyncWebSocketServer)
        return *ws;

    enabledAsyncWebSocketServer = true;
    ws = std::make_unique<AsyncWebSocket>(path);
    ws->onEvent(onWsEvent);
    return *ws;
}


void Monitor::onWsEvent(AsyncWebSocket *server, const AsyncWebSocketClient *client, AwsEventType type, void *arg,
                        uint8_t *data, const size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            if (serialInstance) {
                serialInstance->write("Ws client connected: " + client->remoteIP().toString());
            }
            break;
        case WS_EVT_DISCONNECT:
            if (serialInstance) {
                serialInstance->writeLine("Ws client disconnected: " + client->remoteIP().toString());
            }
            break;
        case WS_EVT_PING:
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
        case WS_EVT_DATA:
            // the registered callbacks are not going to be called yet in this test.
            const auto *info = static_cast<AwsFrameInfo *>(arg);
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = '\0';
                const String message = reinterpret_cast<char *>(data);
                if (serialInstance) {
                    serialInstance->writeLine(message);
                }
            }
            break;
    }
}


#endif


void Monitor::init() {
    if (serialInstance) {
        serialInstance->init();
    }
}

void Monitor::write(const std::string &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(message.c_str());
    }
}

void Monitor::write(const String &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(message);
    }
}

void Monitor::write(const char *str) {
    if (serialInstance) {
        serialInstance->write(str);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(str);
    }
}

void Monitor::writeLine(const std::string &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(message.c_str());
    }
}

void Monitor::writeLine(const String &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(message);
    }
}

void Monitor::writeLine(const char *str) {
    if (serialInstance) {
        serialInstance->writeLine(str);
    }
    if (enabledAsyncWebSocketServer) {
        ws->textAll(str);
    }
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
