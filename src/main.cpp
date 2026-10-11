#include <build_flags.h>
#include <WiFi.h>
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
    EvenTheRGBLedHasManagers::setColor(RGBColor(255, 255, 255), 250);
    SerialManager::registerJsonCallback(WifiManager::wifiConfigJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::inputsWriteJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::outputsWriteJsonCallback);
    SerialManager::registerCallback(CanningMachine::processCommands);

    if (!WifiManager::setup()) {
        Serial.println("Failed to setup WiFi");
        ESP.restart();
    }

    delay(100);

    MqttManager::setup();

    // static bool shouldUpdateWifi = false;

    MqttManager::registerCallback("config/wifi/set", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing WiFi config json on MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        WifiManager::wifiConfigJsonCallback(doc);
    });

    MqttManager::registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    MqttManager::registerCallback("machine/IO/outputs/write", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing outputs write over MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        CanningMachine::outputsWriteJsonCallback(doc);
    });

#ifdef INPUT_LOGIC_TEST
    MqttManager::registerCallback("machine/IO/inputs/write", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing inputs write over MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        CanningMachine::inputsWriteJsonCallback(doc);
    });
#endif

    MqttManager::registerCallback("machine/command", CanningMachine::processCommands);

    MqttManager::registerPeriodicPublish("config/wifi/state",5000, WifiManager::getState);
    MqttManager::registerPeriodicPublish("machine/state/read",1000, CanningMachine::getSerializedState);


    MqttManager::registerPeriodicPublish("machine/IO/outputs/read",200, CanningMachine::getJsonOutputs);
    MqttManager::registerPeriodicPublish("machine/IO/inputs/read",200, CanningMachine::getJsonInputs);


    MqttManager::begin();

    DashboardServer::setup();

    CanningMachine::setup();
    delay(100);
    EvenTheRGBLedHasManagers::setColor(RGBColor(0, 255,0),1000);
    CanningMachine::begin();

}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
