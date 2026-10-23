//
// Created by urzu-7 on 8/25/26.
//
#include <Arduino.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "MqttManager.h"
#if __has_include("config/build_flags.h")
#include "config/build_flags.h"
#endif
#include "drivers/universal/Monitor.h"
#include <esp_task_wdt.h>

WiFiServer MqttManager::tcp_server(1883);

WiFiServer MqttManager::websocket_underlying_server(8080);

PicoWebsocket::Server<WiFiServer> MqttManager::websocket_server(websocket_underlying_server);

PicoMQTT::Server MqttManager::mqtt(tcp_server, websocket_server);


void MqttManager::registerCallback(const char *topic, const std::function<void(char *)> &callback) {
    mqtt.subscribe(topic, callback);
}

void MqttManager::registerPeriodicPublish(const String &topic, const unsigned long delay,
                                          const std::function<String()> &callback) {
    if (!periodicMutex) {
        periodicMutex = xSemaphoreCreateMutex();
    }
    if (periodicMutex && xSemaphoreTake(periodicMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        periodicPublishes.emplace_back(topic, delay, callback);
        xSemaphoreGive(periodicMutex);
    } else {
        periodicPublishes.emplace_back(topic, delay, callback);
    }
}

void MqttManager::publish(const String &topic, const String &payload, const uint8_t qos, const bool retain) {
    if (!publishMutex) {
        publishMutex = xSemaphoreCreateMutex();
    }
    if (publishMutex && xSemaphoreTake(publishMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (publishQueue.size() < 100) {
            publishQueue.push({topic, payload, qos, retain});
        }
        xSemaphoreGive(publishMutex);
    }
}

void MqttManager::publish(const char *topic, const String &payload, const uint8_t qos, const bool retain) {
    publish(String(topic), payload, qos, retain);
}

void MqttManager::publish(const char *topic, const char *payload, const uint8_t qos, const bool retain) {
    publish(String(topic), String(payload), qos, retain);
}

void MqttManager::publish(const String &topic, const char *payload, const uint8_t qos, const bool retain) {
    publish(topic, String(payload), qos, retain);
}

void MqttManager::processPublishQueue() {
    if (!publishMutex) return;
    if (xSemaphoreTake(publishMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        while (!publishQueue.empty()) {
            const auto &[topic, payload, qos, retain] = publishQueue.front();
            mqtt.publish(topic, payload, qos, retain);
            publishQueue.pop();
        }
        xSemaphoreGive(publishMutex);
    }
}

void MqttManager::setup() {
    if (!publishMutex) {
        publishMutex = xSemaphoreCreateMutex();
    }
    if (!periodicMutex) {
        periodicMutex = xSemaphoreCreateMutex();
    }
    mqtt.begin();
}

void MqttManager::begin() {
    if (mqttTaskHandle != nullptr) {
        return;
    }
    if (!publishMutex) {
        publishMutex = xSemaphoreCreateMutex();
    }
    if (!periodicMutex) {
        periodicMutex = xSemaphoreCreateMutex();
    }
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
    esp_task_wdt_add(nullptr);
    while (true) {
        mqtt.loop();

        // Process queued dynamic publish requests
        processPublishQueue();

        // Process periodic telemetry publishers
        if (periodicMutex && xSemaphoreTake(periodicMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (auto &publisher: periodicPublishes) {
                if (publisher.isTime()) {
                    mqtt.publish(publisher.getTopic(), publisher.getPayload());
                    publisher.resetTime();
                }
            }
            xSemaphoreGive(periodicMutex);
        }

        if (millis() - lastWatchdogFeed > 100) {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
