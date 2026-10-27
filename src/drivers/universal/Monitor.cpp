//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "Monitor.h"

void Monitor::useSerialManager(std::unique_ptr<SerialManager> serialManager) {
    serialInstance = std::move(serialManager);
    if (serialInstance) {
        for (const auto &cb : callbacks) {
            serialInstance->registerCallback(cb);
        }
        for (const auto &jcb : jsonCallbacks) {
            serialInstance->registerJsonCallback(jcb);
        }
    }
}

#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER

AsyncWebSocket &Monitor::enableAsyncWebSocketHandler(const char *path) {
    if (enabledAsyncWebSocketServer && ws)
        return *ws;

    enabledAsyncWebSocketServer = true;
    ws = std::make_unique<AsyncWebSocket>(path);
    ws->onEvent(onWsEvent);
    startWsTask();
    return *ws;
}

void Monitor::startWsTask() {
    if (!wsQueueMutex) {
        wsQueueMutex = xSemaphoreCreateMutex();
    }
    if (!wsTaskHandle) {
        xTaskCreate(wsTaskEntry, "MonitorWsTask", 8192, nullptr, 1, &wsTaskHandle);
    }
}

void Monitor::enqueueWsMessage(const std::string &message) {
    if (!wsQueueMutex) return;
    if (xSemaphoreTake(wsQueueMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (wsMessageQueue.size() < 50) {
            wsMessageQueue.push(message);
        }
        xSemaphoreGive(wsQueueMutex);
        if (wsTaskHandle) {
            xTaskNotifyGive(wsTaskHandle);
        }
    }
}

void Monitor::processWsQueue() {
    std::string msg;
    bool hasMsg = false;

    do {
        hasMsg = false;
        if (wsQueueMutex && xSemaphoreTake(wsQueueMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (!wsMessageQueue.empty()) {
                msg = std::move(wsMessageQueue.front());
                wsMessageQueue.pop();
                hasMsg = true;
            }
            xSemaphoreGive(wsQueueMutex);
        }

        if (hasMsg) {
            dispatchMessage(msg);
        }
    } while (hasMsg);
}

void Monitor::wsTaskEntry(void *pv) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        processWsQueue();
    }
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
            if (const auto *info = static_cast<AwsFrameInfo *>(arg);
                info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                const std::string rawMessage(reinterpret_cast<const char *>(data), len);
                enqueueWsMessage(rawMessage);
            }
            break;
        }
    }
}

#endif

void Monitor::dispatchMessage(const std::string &rawMsg) {
    String data = rawMsg.c_str();
    data.trim();
    if (data.isEmpty()) {
        return;
    }

    if (JsonDocument jsonDocument; !deserializeJson(jsonDocument, data)) {
        for (const auto &item : jsonCallbacks) {
            if (item) {
                item(jsonDocument);
            }
        }
    } else {
        for (const auto &item : callbacks) {
            if (item) {
                item(data.c_str());
            }
        }
    }
}

void Monitor::init() {
    if (serialInstance) {
        serialInstance->init();
    }
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    if (enabledAsyncWebSocketServer) {
        startWsTask();
    }
#endif
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

void Monitor::registerCallback(const std::function<void(const char *)> &callback) {
    callbacks.push_back(callback);
    if (serialInstance) {
        serialInstance->registerCallback(callback);
    }
}

void Monitor::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    jsonCallbacks.push_back(callback);
    if (serialInstance) {
        serialInstance->registerJsonCallback(callback);
    }
}
