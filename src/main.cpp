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
#include "EvenTheRGBLedHasManagers.h"

void setup() {
    nvs_flash_init();
    Serial.begin(115200);
    SerialManager::init();
    EvenTheRGBLedHasManagers::init();
    delay(100);
    EvenTheRGBLedHasManagers::setColor(RGBColor(255, 255, 255),250);
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

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
