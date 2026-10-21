//
// Created by urzu-7 on 8/31/26.
//
#include <Arduino.h>
#include "Monitor.h"

void Monitor::init(const uint64_t baudRate, const uint32_t _microsecondsDelay) {
    if (!instance) {
        instance = UARTManager::create(Serial, -1, -1, static_cast<uint32_t>(baudRate), _microsecondsDelay);
    }
    instance->init(static_cast<uint32_t>(baudRate), _microsecondsDelay);
}

void Monitor::stop() {
    if (instance) {
        instance->stop();
    }
}

void Monitor::write(const std::string &message) {
    if (instance) {
        instance->write(message);
    }
}

void Monitor::write(const String &message) {
    if (instance) {
        instance->write(message);
    }
}

void Monitor::write(const char *str) {
    if (instance) {
        instance->write(str);
    }
}

void Monitor::writeLine(const std::string &message) {
    if (instance) {
        instance->writeLine(message);
    }
}

void Monitor::writeLine(const String &message) {
    if (instance) {
        instance->writeLine(message);
    }
}

void Monitor::writeLine(const char *str) {
    if (instance) {
        instance->writeLine(str);
    }
}

void Monitor::registerCallback(const std::function<void(const char *)> &callback) {
    if (!instance) {
        instance = UARTManager::create(Serial);
    }
    instance->registerCallback(callback);
}

void Monitor::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    if (!instance) {
        instance = UARTManager::create(Serial);
    }
    instance->registerJsonCallback(callback);
}
