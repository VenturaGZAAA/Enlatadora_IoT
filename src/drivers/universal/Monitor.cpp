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

void Monitor::init() {
    if (serialInstance) {
        serialInstance->init();
    }
}

void Monitor::write(const std::string &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
}

void Monitor::write(const String &message) {
    if (serialInstance) {
        serialInstance->write(message);
    }
}

void Monitor::write(const char *str) {
    if (serialInstance) {
        serialInstance->write(str);
    }
}

void Monitor::writeLine(const std::string &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
}

void Monitor::writeLine(const String &message) {
    if (serialInstance) {
        serialInstance->writeLine(message);
    }
}

void Monitor::writeLine(const char *str) {
    if (serialInstance) {
        serialInstance->writeLine(str);
    }
}

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
