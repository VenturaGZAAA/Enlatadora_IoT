//
// Created by urzu-7 on 10/10/26.
//

#include "MillisData.h"
#include <Arduino.h>
void MillisData::reset() {
    lastTime = millis();
}

bool MillisData::isTime() const {
    return millis() - lastTime >= delay;
}

