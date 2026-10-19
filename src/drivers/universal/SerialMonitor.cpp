//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "SerialMonitor.h"

#include <string>

// std::queue<std::string> SerialQueue::serialQueue = std::queue<std::string>();
// std::vector<std::function<void(const char*)>> SerialQueue::serialCallbacks = std::vector<std::function<void(const char*)>>();

// TaskHandle_t SerialQueue::task;

// uint32_t SerialQueue::period = 10;

void SerialMonitor::init(const uint64_t baudRate,const uint32_t _microsecondsDelay) {
    Serial.begin(baudRate);
    period = _microsecondsDelay;
    if (!queueMutex) {
        queueMutex = xSemaphoreCreateMutex();
    }
    xTaskCreate(reinterpret_cast<TaskFunction_t>(loop), "SerialQueue", 8192, nullptr, 5, &task);
    initialized = true;
}

[[noreturn]] void SerialMonitor::loop() {
    const TickType_t xFrequency = pdMS_TO_TICKS(period);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        if (!initialized) {
            continue;
        }
        run();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void SerialMonitor::run() {

    // 1. Drain output queue safely under mutex protection
    if (queueMutex && xSemaphoreTake(queueMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        while (!serialQueue.empty()) {
            Serial.print(serialQueue.front().c_str());
            serialQueue.pop();
        }
        xSemaphoreGive(queueMutex);
    }

    // 2. Non-blocking line assembly to handle \r, \n, and \r\n without 1000ms timeouts
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            rxBuffer.trim();
            if (!rxBuffer.isEmpty()) {
                const String data = rxBuffer;
                rxBuffer = "";
                Serial.println("Serial data received: \t" + data);

                if (JsonDocument jsonDocument; !deserializeJson(jsonDocument, data)) {
                    Serial.println("Json document received");
                    for (const auto &item: serialJsonCallbacks) {
                        item(jsonDocument);
                    }
                } else {
                    for (const auto &item: serialCallbacks) {
                        item(data.c_str());
                    }
                }
            }
        } else {
            if (rxBuffer.length() < 512) {
                rxBuffer += c;
            }
        }
    }
}

void SerialMonitor::enqueue(const std::string &message) {
    if (!queueMutex) return;
    if (!initialized) {
        return;
    }
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (serialQueue.size() < 100) {
            // Limit queue to prevent unbounded RAM usage
            serialQueue.push(message);
        }
        xSemaphoreGive(queueMutex);
    }
}

void SerialMonitor::enqueue(const String &message) {
    enqueue(std::string(message.c_str()));
}

void SerialMonitor::enqueue(const char *str) {
    if (str) {
        enqueue(std::string(str));
    }
}

void SerialMonitor::enqueueLine(const std::string &message) {
    enqueue(message + "\r\n");
}

void SerialMonitor::enqueueLine(const String &message) {
    enqueue(message + "\r\n");
}

void SerialMonitor::enqueueLine(const char *str) {
    enqueue(str + String("\r\n"));
}

void SerialMonitor::registerCallback(const std::function<void(const char *)> &callback) {
    serialCallbacks.push_back(callback);
}

void SerialMonitor::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    serialJsonCallbacks.push_back(callback);
}
