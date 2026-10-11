//
// Created by urzu-7 on 8/25/26.
//
#include <Arduino.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "MqttManager.h"
#include <build_flags.h>
#include "SerialManager.h"
#include <esp_task_wdt.h>

WiFiServer MqttManager::tcp_server(1883);

WiFiServer MqttManager::websocket_underlying_server(8080);

PicoWebsocket::Server<WiFiServer> MqttManager::websocket_server(websocket_underlying_server);

PicoMQTT::Server MqttManager::mqtt(tcp_server, websocket_server);


void MqttManager::registerCallback(const char *topic, const std::function<void(char *)> &callback) {
    mqtt.subscribe(topic, callback);
}

void MqttManager::registerPeriodicPublish(const unsigned long delay, const std::function<void()> &callback) {
    periodicPublishes.emplace_back(PeriodicPublish(delay, callback));
}


void MqttManager::publish(const char *topic, const String &payload, const uint8_t qos) {
    mqtt.publish(topic, payload, qos);
}

void MqttManager::setup() {
    mqtt.begin();
}

void MqttManager::begin() {
    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(mqttManagerLoop),
        "MQTT_manager_loop",
        SERVER_STACK_SIZE,
        nullptr,
        1,
        &mqttTaskHandle,
        0
    );
}

[[noreturn]] void MqttManager::mqttManagerLoop() {


    unsigned long lastWatchdogFeed = millis();


    while (true) {
        mqtt.loop();

        for (auto &publisher : periodicPublishes) {
            if (publisher.isTime()) {
                publisher.execute();
                publisher.resetTime();
            }
        }
        if (millis() - lastWatchdogFeed > 100) {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
