//
// Created by urzu-7 on 8/31/26.
//

#ifndef ENLATADORA_IOT_SERIALQUEUE_H
#define ENLATADORA_IOT_SERIALQUEUE_H

#include <string>
#include <functional>
#include <ArduinoJson.h>
#include "UARTManager.h"

class Monitor {
    static inline  auto instance = UARTManager::reserve();


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

    static void init(uint64_t baudRate = 115200, uint32_t _microsecondsDelay = 25);

    static void stop();

};


#endif //ENLATADORA_IOT_SERIALQUEUE_H
