//
// Created by urzu-7 on 10/10/26.
//

#include "EvenTheRGBLedHasManagers.h"


void EvenTheRGBLedHasManagers::init() {
    if (!ledMutex) {
        ledMutex = xSemaphoreCreateMutex();
    }
    if (loopHandle != nullptr) {
        return;
    }
    xTaskCreate(reinterpret_cast<TaskFunction_t>(loop), "RGB-Loop", 2048, nullptr, 3, &loopHandle);
}

void EvenTheRGBLedHasManagers::setColor(const RGBColor color, const unsigned long t) {
    if (!ledMutex) {
        ledMutex = xSemaphoreCreateMutex();
    }
    if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (t > 0) {
            holdColor = color;
            holdDuration = t;
            lastHoldTimestamp = millis();
        }
        xSemaphoreGive(ledMutex);
    } else {
        if (t > 0) {
            holdColor = color;
            holdDuration = t;
            lastHoldTimestamp = millis();
        }
    }
}

void EvenTheRGBLedHasManagers::writeColor(const RGBColor &color) {
    neopixelWrite(RGB_PIN, color.getR(), color.getG(), color.getB());
}

[[noreturn]] void EvenTheRGBLedHasManagers::loop() {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(50));
        RGBColor activeColor;
        if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (lastHoldTimestamp + holdDuration > millis()) {
                activeColor = holdColor;
            } else if (colorGenerator != nullptr) {
                activeColor = colorGenerator();
            }
            xSemaphoreGive(ledMutex);
        } else {
            if (lastHoldTimestamp + holdDuration > millis()) {
                activeColor = holdColor;
            } else if (colorGenerator != nullptr) {
                activeColor = colorGenerator();
            }
        }
        writeColor(activeColor);
    }
}

void EvenTheRGBLedHasManagers::stop() {
    if (!ledMutex) {
        ledMutex = xSemaphoreCreateMutex();
    }
    if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (loopHandle != nullptr) {
            vTaskDelete(loopHandle);
            loopHandle = nullptr;
        }
        holdColor = RGBColor();
        holdDuration = 0;
        xSemaphoreGive(ledMutex);
    } else {
        if (loopHandle != nullptr) {
            vTaskDelete(loopHandle);
            loopHandle = nullptr;
        }
        holdColor = RGBColor();
        holdDuration = 0;
    }
    writeColor(RGBColor(0, 0, 0));
}
