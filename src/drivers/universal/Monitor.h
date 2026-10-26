//
// Created by urzu-7 on 8/31/26.
//

#ifndef ENLATADORA_IOT_SERIALQUEUE_H
#define ENLATADORA_IOT_SERIALQUEUE_H

#include <string>
#include <functional>
#include <ArduinoJson.h>
#include "SerialManager.h"

class Monitor {
    static inline  auto serialInstance = SerialManager::reserve();

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

    static void init();


};


#endif //ENLATADORA_IOT_SERIALQUEUE_H
