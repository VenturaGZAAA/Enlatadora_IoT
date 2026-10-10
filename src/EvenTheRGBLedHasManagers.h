//
// Created by urzu-7 on 10/10/26.
//

#ifndef ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
#define ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
#include <Arduino.h>


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
    [[nodiscard]] uint8_t getR() const { return this->r; }
    [[nodiscard]] uint8_t getG() const { return this->g; }
    [[nodiscard]] uint8_t getB() const { return this->b; }
    [[nodiscard]] String getCode() const {
        String code ="[ ";
        code += String(r) + " ";
        code += String(g) + " ";
        code += String(b) + " ";
        return  code + "]";
    };
};

#define RGB_BUILTIN 48

class EvenTheRGBLedHasManagers {
    static constexpr uint8_t RGB_PIN = RGB_BUILTIN;

    static inline TaskHandle_t loopHandle = nullptr;
    [[noreturn]] static void loop();

    static inline auto persistentColor = RGBColor();
    static inline auto holdColor = RGBColor();
    static inline unsigned long lastHoldTimestamp = millis();
    static inline unsigned long holdDuration = 0;
    static void writeColor(const RGBColor& color);

public:
    static void init();
    static void setColor(RGBColor color,unsigned long t = 0);

};


#endif //ENLATADORA_IOT_EVENTHERGBLEDHASMANAGERS_H
