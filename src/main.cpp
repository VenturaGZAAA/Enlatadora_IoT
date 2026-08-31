#include <WiFi.h>
#include <SPI.h>
#include <secrets.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>
#include "MqttServer.h"
#include <esp_task_wdt.h>

static TaskHandle_t serverTaskHandle;

#define SERVER_STACK_SIZE (8192 * 8)  // 64KB stack

[[noreturn]] static void serverTask() {
    // Configure watchdog for ESP32-S3 (new API)
    // Only initialize once - check if already initialized
    if (esp_task_wdt_status(nullptr) == ESP_ERR_NOT_FOUND) {
        constexpr esp_task_wdt_config_t twdt_config = {
            .timeout_ms = 10000,  // 10 second timeout
            .idle_core_mask = (1 << 0) | (1 << 1),  // Both cores
            .trigger_panic = true  // Panic on timeout
        };
        esp_task_wdt_init(&twdt_config);
    }
    esp_task_wdt_add(nullptr);

    MqttServer::setup();
    DashboardServer::setup();

    unsigned long lastWatchdogFeed = millis();

    while (true) {
        // Handle MQTT
        MqttServer::loop();

        // Handle HTTP server
        DashboardServer::loop();

        // Feed watchdog every 100ms
        if (millis() - lastWatchdogFeed > 100) {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }

        // Don't delay too much, but yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    Serial.begin(115200);
    // --- Connect to Wi-Fi ---
    Serial.print("📶 Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("📡 IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ WiFi connection failed!");
        return;
    }

    delay(100);
    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(serverTask),
        "servers",
        SERVER_STACK_SIZE,
        nullptr,
        1,  // Priority 1 (was 0)
        &serverTaskHandle,
        0   // Core 0
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}