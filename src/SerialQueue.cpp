//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "SerialQueue.h"

#include <string>

std::queue<std::string> SerialQueue::serialQueue = std::queue<std::string>();

TaskHandle_t SerialQueue::task;

uint32_t SerialQueue::period = 10;

void SerialQueue::init(const uint32_t _microsecondsDelay) {
    period = _microsecondsDelay;
    Serial.println("\n=== Serial Queue Initializing ===");
    xTaskCreate(reinterpret_cast<TaskFunction_t>(loop), "SerialQueue", 2048, nullptr, 5, &task);
    Serial.println("Whaaaaat");
    enqueueLine("=== Serial Queue Initialized ===");
}

[[noreturn]] void SerialQueue::loop() {
    while (true) {
        run();
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

void SerialQueue::run() {
    if (serialQueue.empty()) {
        return;
    }
    Serial.print(serialQueue.front().data());
    serialQueue.pop();
}

void SerialQueue::enqueue(const std::string &message) {
    serialQueue.push(message);
}

void SerialQueue::enqueue(const String &message) {
    // std::string stdStr;
   serialQueue.push(message.c_str());
}

void SerialQueue::enqueue(const char *str) {
    serialQueue.push(str);
}

void SerialQueue::enqueueLine(const std::string &message) {
    enqueue(message + "\r\n");
}

void SerialQueue::enqueueLine(const String &message) {
    enqueue(message + "\r\n");
}

void SerialQueue::enqueueLine(const char * str) {
    enqueue(str + String("\r\n"));
}
