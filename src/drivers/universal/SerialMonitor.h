//
// Created by urzu-7 on 8/31/26.
//

#ifndef ENLATADORA_IOT_SERIALQUEUE_H
#define ENLATADORA_IOT_SERIALQUEUE_H
#include <queue>
#include <string>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class SerialMonitor {
    static inline std::queue<std::string> serialQueue;
    static inline std::vector<std::function<void(const char *)> > serialCallbacks;
    static inline std::vector<std::function<void(JsonDocument)> > serialJsonCallbacks;
    static inline TaskHandle_t task = nullptr;
    static inline SemaphoreHandle_t queueMutex = nullptr;
    static inline String rxBuffer = "";
    static inline uint32_t period = 25;

    static inline  bool initialized = false;
    [[noreturn]] static void loop();
    static void run();

public:
    SerialMonitor() = delete;

    static void write(const std::string &message);

    static void writeLine(const std::string &message);

    static void write(const String &message);

    static void writeLine(const String &message);

    static void write(const char *str);

    static void writeLine(const char *str);

    static void registerCallback(const std::function<void(const char *)> &callback);

    static void registerJsonCallback(const std::function<void(JsonDocument)> &callback);


    static void init(uint64_t baudRate = 115200,uint32_t _microsecondsDelay = 25);
};


#endif //ENLATADORA_IOT_SERIALQUEUE_H
