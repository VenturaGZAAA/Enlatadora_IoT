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
#include <memory>


class UARTManager {
    HardwareSerial &uartPort;
    int8_t rxPin;
    int8_t txPin;
    uint32_t baudRate;
    uint32_t period;
    uint32_t bufferLength;
    std::queue<std::string> serialQueue;
    std::vector<std::function<void(const char *)> > serialCallbacks;
    std::vector<std::function<void(JsonDocument)> > serialJsonCallbacks;
    TaskHandle_t task = nullptr;
    SemaphoreHandle_t queueMutex = nullptr;
    String rxBuffer = "";

    static void taskEntry(void *pv);

    [[noreturn]] void loop();

    void run();

    UARTManager(HardwareSerial &uartPort, int8_t rxPin, int8_t txPin, uint32_t baudRate,
               uint32_t period, uint32_t bufferLength);


public:
    static std::unique_ptr<UARTManager> reserve() { return {}; }

    static std::unique_ptr<UARTManager> create(HardwareSerial &uartPort, const int8_t rxPin, const int8_t txPin, const uint32_t baudRate = 115200,
               const uint32_t period = 25,const uint32_t bufferLength = 1024) {
        return std::unique_ptr<UARTManager>(new UARTManager(uartPort, rxPin, txPin, baudRate, period, bufferLength));
    }

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
