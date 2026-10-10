//
// Created by urzu-7 on 10/10/26.
//

#include "EvenTheRGBLedHasManagers.h"

#include "SerialManager.h"

void EvenTheRGBLedHasManagers::init() {
    xTaskCreate(reinterpret_cast<TaskFunction_t>(loop), "RGB-Loop", 2048, nullptr, 3, &loopHandle);
}

void EvenTheRGBLedHasManagers::setColor(const RGBColor color, const unsigned long t) {
    holdDuration = t;
    if (t>0) {
        holdColor = color;
        lastHoldTimestamp = millis();
    }
    else {
        persistentColor = color;
    }
}

void EvenTheRGBLedHasManagers::writeColor(const RGBColor &color) {
    neopixelWrite(RGB_PIN,color.getR(),color.getG(),color.getB());
}

[[noreturn]] void EvenTheRGBLedHasManagers::loop() {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (lastHoldTimestamp + holdDuration > millis()) {
            writeColor(holdColor);
        }
        else {
            writeColor(persistentColor);
        }
    }
}