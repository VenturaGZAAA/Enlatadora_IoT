#include <build_flags.h>
#include <WiFi.h>
#include <SPI.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>
#include "MqttServer.h"
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include "SerialManager.h"
#include "WifiManager.h"
#include "CanningMachine.h"

static TaskHandle_t serverTaskHandle;

#define SERVER_STACK_SIZE (8192 * 2)

[[noreturn]] static void serverTask();

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

    DashboardServer::setup();


    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(serverTask),
        "servers",
        SERVER_STACK_SIZE,
        nullptr,
        1,
        &serverTaskHandle,
        0
    );
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


[[noreturn]] static void serverTask() {
    MqttServer::setup();

    static bool shouldUpdateWifi = false;

    MqttServer::registerCallback("config/wifi/set", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing WiFi config json on MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        WifiManager::wifiConfigJsonCallback(doc);
    });
    MqttServer::registerCallback("config/wifi/get", [](const char *p) {
        shouldUpdateWifi = true;
    });
    MqttServer::registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    MqttServer::registerCallback("machine/IO/outputs/write", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing inputs write over MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        CanningMachine::outputsWriteJsonCallback(doc);
    });

#ifdef INPUT_LOGIC_TEST
    MqttServer::registerCallback("machine/IO/inputs/write", [](const char *payload) {
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

    unsigned long lastWatchdogFeed = millis();
    unsigned long lastWifiUpdate = millis();
    unsigned long lastStateUpdate = millis();
    unsigned long lastIOUpdate = millis();
    while (true) {
        MqttServer::loop();

        if (millis() - lastWatchdogFeed > 100) {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }
        if (millis() - lastWifiUpdate > 5000 || shouldUpdateWifi) {
            MqttServer::publish("config/wifi/state", WifiManager::getState());
            shouldUpdateWifi = false;
            lastWifiUpdate = millis();
        }

        if (millis() - lastStateUpdate > 1000 ) {
            MqttServer::publish("machine/state/read",CanningMachine::getSerializedState());
            lastStateUpdate = millis();
        }

        if (millis() - lastIOUpdate > 200) {
            // MqttServer::publish("machine/stage", CanningMachine::getStage());
            MqttServer::publish("machine/IO/outputs/read", CanningMachine::getJsonOutputs());
            MqttServer::publish("machine/IO/inputs/read", CanningMachine::getJsonInputs());
            lastIOUpdate = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
