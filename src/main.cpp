#include <build_flags.h>
#include <WiFi.h>
#include <SPI.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>
#include "MqttManager.h"
#include <nvs_flash.h>
#include "SerialManager.h"
#include "WifiManager.h"
#include "CanningMachine.h"

void setup() {
    nvs_flash_init();
    Serial.begin(115200);
    SerialManager::init();
    delay(100);

    SerialManager::registerJsonCallback(WifiManager::wifiConfigJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::inputsWriteJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::outputsWriteJsonCallback);

    if (!WifiManager::setup()) {
        Serial.println("Failed to setup WiFi");
        ESP.restart();
    }

    delay(100);

    MqttManager::setup();

    DashboardServer::setup();


    CanningMachine::begin();
}

#define RGB_BUILTIN 48
#define RGB_BRIGHTNESS 64
#define BLINK_DELAY 750

void loop() {
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN, 0,RGB_BRIGHTNESS, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN, 0, 0,RGB_BRIGHTNESS);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
}
