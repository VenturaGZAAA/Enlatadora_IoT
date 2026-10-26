//
// Created by urzu-7 on 10/18/26.
//

#include "SerialManager.h"


SerialManager::SerialManager(HardwareSerial &uartPort, const int8_t rxPin, const int8_t txPin,
                             const uint32_t baudRate, const uint32_t period, const uint32_t bufferLength)
    : uartPort(uartPort), rxPin(rxPin), txPin(txPin), baudRate(baudRate), period(period), bufferLength(bufferLength) {
}

SerialManager::~SerialManager() {
    stop();
    if (queueMutex) {
        vSemaphoreDelete(queueMutex);
        queueMutex = nullptr;
    }
}

void SerialManager::init() {
    if (running) {
        return;
    }
    uartPort.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    uartPort.setTimeout(1000);
    if (!queueMutex) {
        queueMutex = xSemaphoreCreateMutex();
    }
    running = true;
    if (!task) {
        xTaskCreate(taskEntry, "SerialQueue", 8192, this, 1, &task);
    }
}

void SerialManager::init(const uint32_t newBaudRate, const uint32_t newPeriod) {
    this->baudRate = newBaudRate;
    this->period = newPeriod;
    init();
}

void SerialManager::stop() {
    if (!running && !task) {
        return;
    }
    running = false;

    if (task != nullptr) {
        if (xTaskGetCurrentTaskHandle() != task) {
            for (int i = 0; i < 20 && task != nullptr; ++i) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            if (task != nullptr) {
                TaskHandle_t taskToDelete = task;
                task = nullptr;
                vTaskDelete(taskToDelete);
            }
        }
    }

    if (queueMutex && xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        while (!serialQueue.empty()) {
            uartPort.print(serialQueue.front().c_str());
            serialQueue.pop();
        }
        xSemaphoreGive(queueMutex);
    }
    uartPort.flush();
    uartPort.end();
}

void SerialManager::taskEntry(void *pv) {
    auto *manager = static_cast<SerialManager *>(pv);
    manager->loop();
    manager->task = nullptr;
    vTaskDelete(nullptr);
}

void SerialManager::loop() {
    const TickType_t xFrequency = pdMS_TO_TICKS(period); // 20 ms period (50 Hz)
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (running) {
        run();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void SerialManager::run() {
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


void SerialManager::write(const std::string &message) {
    if (!running || !queueMutex) return;
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (serialQueue.size() < 100) {
            // Limit queue to prevent unbounded RAM usage
            serialQueue.push(message);
        }
        xSemaphoreGive(queueMutex);
    }
}

void SerialManager::write(const String &message) {
    write(std::string(message.c_str()));
}

void SerialManager::write(const char *str) {
    if (str) {
        write(std::string(str));
    }
}

void SerialManager::writeLine(const std::string &message) {
    write(message + "\r\n");
}

void SerialManager::writeLine(const String &message) {
    write(message + "\r\n");
}

void SerialManager::writeLine(const char *str) {
    write(str + String("\r\n"));
}

void SerialManager::registerCallback(const std::function<void(const char *)> &callback) {
    serialCallbacks.push_back(callback);
}

void SerialManager::registerJsonCallback(const std::function<void(JsonDocument)> &callback) {
    serialJsonCallbacks.push_back(callback);
}
