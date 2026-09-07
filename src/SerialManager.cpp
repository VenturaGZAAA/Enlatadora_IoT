//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "SerialManager.h"

#include <string>

// std::queue<std::string> SerialQueue::serialQueue = std::queue<std::string>();
// std::vector<std::function<void(const char*)>> SerialQueue::serialCallbacks = std::vector<std::function<void(const char*)>>();

// TaskHandle_t SerialQueue::task;

// uint32_t SerialQueue::period = 10;

void SerialManager::init(const uint32_t _microsecondsDelay) {
    period = _microsecondsDelay;
    Serial.println("\n=== Serial Queue Initializing ===");
    xTaskCreate(reinterpret_cast<TaskFunction_t>(loop), "SerialQueue", 2048, nullptr, 5, &task);
    enqueueLine("=== Serial Queue Initialized ===");
}

[[noreturn]] void SerialManager::loop() {
    while (true) {
        run();
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

void SerialManager::run() {
   while (!serialQueue.empty()) {
       Serial.print(serialQueue.front().data());
       serialQueue.pop();
   }
    if (Serial.available() > 0) {
        const auto data = Serial.readStringUntil('\r');
        for (const auto &item : serialCallbacks) {
            item(data.c_str());
        }
    }
}

void SerialManager::enqueue(const std::string &message) {
    serialQueue.push(message);
}

void SerialManager::enqueue(const String &message) {
    // std::string stdStr;
   serialQueue.emplace(message.c_str());
}

void SerialManager::enqueue(const char *str) {
    serialQueue.emplace(str);
}

void SerialManager::enqueueLine(const std::string &message) {
    enqueue(message + "\r\n");
}

void SerialManager::enqueueLine(const String &message) {
    enqueue(message + "\r\n");
}

void SerialManager::enqueueLine(const char * str) {
    enqueue(str + String("\r\n"));
}

void SerialManager::registerCallback(const std::function<void(const char*)>& callback) {
    serialCallbacks.push_back(callback);
}
