//
// Created by urzu-7 on 8/25/26.
//
#include <Arduino.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>

#include <utility>
#include "MqttManager.h"
#include <build_flags.h>
#include "SerialManager.h"
#include "WifiManager.h"
#include "CanningMachine.h"
#include <esp_task_wdt.h>

WiFiServer MqttManager::tcp_server(1883);

WiFiServer MqttManager::websocket_underlying_server(8080);

PicoWebsocket::Server<WiFiServer> MqttManager::websocket_server(websocket_underlying_server);

PicoMQTT::Server MqttManager::mqtt(tcp_server, websocket_server);



void MqttManager::registerCallback(const char *topic, const std::function<void(char *)>& callback) {
    mqtt.subscribe(topic, callback);
}
void MqttManager::publish(const char *topic,const String& payload, const uint8_t qos) {
    mqtt.publish(topic, payload,qos);
}

void MqttManager::setup() {
    mqtt.begin();

    mqtt.subscribe("home/test/led", [](const char *) {
        SerialManager::enqueueLine("You pressed the button!");
    });

    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(serverTask),
        "servers",
        SERVER_STACK_SIZE,
        nullptr,
        1,
        &mqttTaskHandle,
        0
    );
}



[[noreturn]] void MqttManager::serverTask() {

    static bool shouldUpdateWifi = false;

    registerCallback("config/wifi/set", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            SerialManager::enqueue("Error deserializing WiFi config json on MQTT: ");
            SerialManager::enqueueLine(error.c_str());
            return;
        }
        WifiManager::wifiConfigJsonCallback(doc);
    });
    registerCallback("config/wifi/get", [](const char *p) {
        shouldUpdateWifi = true;
    });
    registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    registerCallback("machine/IO/outputs/write", [](const char *payload) {
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
    registerCallback("machine/IO/inputs/write", [](const char *payload) {
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
        mqtt.loop();


        if (millis() - lastWatchdogFeed > 100) {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }
        if (millis() - lastWifiUpdate > 5000 || shouldUpdateWifi) {
            publish("config/wifi/state", WifiManager::getState());
            shouldUpdateWifi = false;
            lastWifiUpdate = millis();
        }

        if (millis() - lastStateUpdate > 1000 ) {
            publish("machine/state/read",CanningMachine::getSerializedState());
            lastStateUpdate = millis();
        }

        if (millis() - lastIOUpdate > 200) {
            // MqttServer::publish("machine/stage", CanningMachine::getStage());
            publish("machine/IO/outputs/read", CanningMachine::getJsonOutputs());
            publish("machine/IO/inputs/read", CanningMachine::getJsonInputs());
            lastIOUpdate = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
