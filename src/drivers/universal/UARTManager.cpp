//
// Created by urzu-7 on 10/18/26.
//

#include "UARTManager.h"


UARTManager::UARTManager(const HardwareSerial &uartPort, const uint8_t rxPin, const uint8_t txPin,
                         const uint32_t baudRate, const uint32_t period) : uartPort(uartPort) {
    this->uartPort.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    this->period = period;
    this->uartPort.setTimeout(100);
    this->serialQueue = std::queue<std::string>();
    this->serialCallbacks = std::vector<std::function<void(const char *)> >();
    this->serialJsonCallbacks = std::vector<std::function<void(JsonDocument)> >();
}

void UARTManager::init() {
    xTaskCreate(taskEntry, "SerialQueue", 8192, this, 1, &task);
}

void UARTManager::taskEntry(void *pv) {
    static_cast<UARTManager *>(pv)->loop();
    vTaskDelete(nullptr);
}

[[noreturn]] void UARTManager::loop() {
    const TickType_t xFrequency = pdMS_TO_TICKS(period); // 20 ms period (50 Hz)
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        run();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void UARTManager::run() {
    if (queueMutex && xSemaphoreTake(queueMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        while (!serialQueue.empty()) {
            uartPort.print(serialQueue.front().c_str());
            serialQueue.pop();
        }
        xSemaphoreGive(queueMutex);
    }


    while (uartPort.available() > 0) {
        if (const char c = static_cast<char>(uartPort.read()); c == '\r' || c == '\n') {
            rxBuffer.trim();
            if (!rxBuffer.isEmpty()) {
                const String data = rxBuffer;
                rxBuffer = "";
                if (JsonDocument jsonDocument; !deserializeJson(jsonDocument, data)) {
                    for (const auto &item: serialJsonCallbacks) {
                        item(jsonDocument);
                    }
                } else {
                    for (const auto &item: serialCallbacks) {
                        item(data.c_str());
                    }
                }
            }
        } else {
            if (rxBuffer.length() < 512) {
                rxBuffer += c;
            }
        }
    }
}


void UARTManager::enqueue(const std::string &message) {
    if (!queueMutex) return;
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (serialQueue.size() < 100) {
            // Limit queue to prevent unbounded RAM usage
            serialQueue.push(message);
        }
        xSemaphoreGive(queueMutex);
    }
}

void UARTManager::enqueue(const String &message) {
    enqueue(std::string(message.c_str()));
}

void UARTManager::enqueue(const char *str) {
    if (str) {
        enqueue(std::string(str));
    }
}

void UARTManager::enqueueLine(const std::string &message) {
    enqueue(message + "\r\n");
}

void UARTManager::enqueueLine(const String &message) {
    enqueue(message + "\r\n");
}

void UARTManager::enqueueLine(const char *str) {
    enqueue(str + String("\r\n"));
}

void UARTManager::registerCallback(const std::function<void(const char *)> &callback) {
    serialCallbacks.push_back(callback);
}

void UARTManager::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    serialJsonCallbacks.push_back(callback);
}
