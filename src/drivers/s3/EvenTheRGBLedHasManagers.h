//
// Created by urzu-7 on 10/10/26.
//

#ifndef ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
#define ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cmath>
#include <utility>

class RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

public:
    RGBColor() {
        this->r = 0;
        this->g = 0;
        this->b = 0;
    }

    RGBColor(const uint8_t r, const uint8_t g, const uint8_t b) {
        this->r = r;
        this->g = g;
        this->b = b;
    }

    [[nodiscard]] static RGBColor Off() { return {0, 0, 0}; }
    [[nodiscard]] static RGBColor Red() { return {255, 0, 0}; }
    [[nodiscard]] static RGBColor Green() { return {0, 255, 0}; }
    [[nodiscard]] static RGBColor Blue() { return {0, 0, 255}; }
    [[nodiscard]] static RGBColor Yellow() { return {255, 255, 0}; }
    [[nodiscard]] static RGBColor Magenta() { return {255, 0, 255}; }
    [[nodiscard]] static RGBColor Cyan() { return {0, 255, 255}; }
    [[nodiscard]] static RGBColor White() { return {255, 255, 255}; }

    [[nodiscard]] static RGBColor off() { return Off(); }
    [[nodiscard]] static RGBColor red() { return Red(); }
    [[nodiscard]] static RGBColor green() { return Green(); }
    [[nodiscard]] static RGBColor blue() { return Blue(); }
    [[nodiscard]] static RGBColor yellow() { return Yellow(); }
    [[nodiscard]] static RGBColor magenta() { return Magenta(); }
    [[nodiscard]] static RGBColor cyan() { return Cyan(); }
    [[nodiscard]] static RGBColor white() { return White(); }

    [[nodiscard]] RGBColor withAlphaInt(const uint8_t alpha) const {
        return withAlpha(static_cast<float>(alpha) / 255.0f);
    }

    [[nodiscard]] RGBColor withAlpha(const float alpha) const {
        const float clampedAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
        return {
            static_cast<uint8_t>(roundf(static_cast<float>(this->r) * clampedAlpha)),
            static_cast<uint8_t>(roundf(static_cast<float>(this->g) * clampedAlpha)),
            static_cast<uint8_t>(roundf(static_cast<float>(this->b) * clampedAlpha))
        };
    }

    [[nodiscard]] uint8_t getR() const { return this->r; }
    [[nodiscard]] uint8_t getG() const { return this->g; }
    [[nodiscard]] uint8_t getB() const { return this->b; }

    [[nodiscard]] String getCode() const {
        String code = "[ ";
        code += String(r) + " ";
        code += String(g) + " ";
        code += String(b) + " ";
        return code + "]";
    };
};


class EvenTheRGBLedHasManagers {
    static constexpr uint8_t RGB_PIN = 48;

    static inline TaskHandle_t loopHandle = nullptr;
    static inline SemaphoreHandle_t ledMutex = nullptr;

    [[noreturn]] static void loop();

    static inline auto holdColor = RGBColor();
    static inline unsigned long lastHoldTimestamp = millis();
    static inline unsigned long holdDuration = 0;

    static void writeColor(const RGBColor &color);

    static inline std::function<RGBColor()> colorGenerator = nullptr;

public:
    static void init();

    static void setColor(RGBColor color, unsigned long t);

    static void setColorGenerator(std::function<RGBColor()> generator) {
        colorGenerator = std::move(generator);
    }

    static void stop();
};


#endif //ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
