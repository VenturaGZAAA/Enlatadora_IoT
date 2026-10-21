//
// Created by urzu-7 on 10/18/26.
//

#include "UARTManager.h"


UARTManager::UARTManager(HardwareSerial &uartPort, const int8_t rxPin, const int8_t txPin,
                         const uint32_t baudRate, const uint32_t period, const uint32_t bufferLength)
    : uartPort(uartPort), rxPin(rxPin), txPin(txPin), baudRate(baudRate), period(period), bufferLength(bufferLength) {
}

void UARTManager::init() {
    uartPort.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    uartPort.setTimeout(1000);
    if (!queueMutex) {
        queueMutex = xSemaphoreCreateMutex();
    }
    if (!task) {
        xTaskCreate(taskEntry, "SerialQueue", 8192, this, 1, &task);
    }
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
            if (rxBuffer.length() < bufferLength) {
                rxBuffer += c;
            }
        }
    }
}


void UARTManager::write(const std::string &message) {
    if (!queueMutex) return;
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (serialQueue.size() < 100) {
            // Limit queue to prevent unbounded RAM usage
            serialQueue.push(message);
        }
        xSemaphoreGive(queueMutex);
    }
}

void UARTManager::write(const String &message) {
    write(std::string(message.c_str()));
}

void UARTManager::write(const char *str) {
    if (str) {
        write(std::string(str));
    }
}

void UARTManager::writeLine(const std::string &message) {
    write(message + "\r\n");
}

void UARTManager::writeLine(const String &message) {
    write(message + "\r\n");
}

void UARTManager::writeLine(const char *str) {
    write(str + String("\r\n"));
}

void UARTManager::registerCallback(const std::function<void(const char *)> &callback) {
    serialCallbacks.push_back(callback);
}

void UARTManager::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    serialJsonCallbacks.push_back(callback);
}
