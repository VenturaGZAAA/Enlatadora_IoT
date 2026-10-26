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


class SerialManager {
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
    bool running = false;

    static void taskEntry(void *pv);

    void loop();

    void run();

    //* THIS CONSTRUCTOR IS PRIVATE TO ENFORCE USAGE THROUGH UNIQUE POINTER CREATION
    explicit SerialManager(HardwareSerial &uartPort, int8_t rxPin = -1, int8_t txPin = -1, uint32_t baudRate = 115200,
                         uint32_t period = 25, uint32_t bufferLength = 1024);
    // THIS CONSTRUCTOR IS PRIVATE TO ENFORCE USAGE THROUGH UNIQUE POINTER CREATION *//
public:

    ~SerialManager();

    static std::unique_ptr<SerialManager> reserve() { return {}; }

    static std::unique_ptr<SerialManager> create(HardwareSerial &uartPort, const int8_t rxPin = -1, const int8_t txPin = -1, const uint32_t baudRate = 115200,
               const uint32_t period = 25, const uint32_t bufferLength = 1024) {
        return std::unique_ptr<SerialManager>(new SerialManager(uartPort, rxPin, txPin, baudRate, period, bufferLength));
    }

    void init();

    void init(uint32_t baudRate, uint32_t period = 25);

    void stop();

    [[nodiscard]] bool isRunning() const { return running; }

    void write(const std::string &message);

    void writeLine(const std::string &message);

    void write(const String &message);

    void writeLine(const String &message);

    void write(const char *str);

    void writeLine(const char *str);

    void registerCallback(const std::function<void(const char *)> &callback);

    void registerJsonCallback(const std::function<void(JsonDocument)> &callback);
};


#endif //ENLATADORA_CONTROL_UARTMANAGER_H
