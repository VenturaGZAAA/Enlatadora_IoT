//
// Created by urzu-7 on 10/18/26.
//

#ifndef ENLATADORA_CONTROL_UARTMANAGER_H
#define ENLATADORA_CONTROL_UARTMANAGER_H

#include <queue>
#include <string>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>


class UARTManager {
    HardwareSerial uartPort;
    std::queue<std::string> serialQueue;
    std::vector<std::function<void(const char *)> > serialCallbacks;
    std::vector<std::function<void(JsonDocument)> > serialJsonCallbacks;
    TaskHandle_t task = nullptr;
    SemaphoreHandle_t queueMutex = nullptr;
    String rxBuffer = "";
    uint32_t period = 25;

    static void taskEntry(void *pv);

    [[noreturn]] void loop();

    void run();

public:
    UARTManager(const HardwareSerial &uartPort, uint8_t rxPin, uint8_t txPin, uint32_t baudRate = 115200,
                uint32_t period = 25);

    void init();

    void enqueue(const std::string &message);

    void enqueueLine(const std::string &message);

    void enqueue(const String &message);

    void enqueueLine(const String &message);

    void enqueue(const char *str);

    void enqueueLine(const char *str);

    void registerCallback(const std::function<void(const char *)> &callback);

    void registerJsonCallback(const std::function<void(JsonDocument)> &callback);
};


#endif //ENLATADORA_CONTROL_UARTMANAGER_H
