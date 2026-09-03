#include <WiFi.h>
#include <SPI.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>
#include "MqttServer.h"
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include "SerialQueue.h"
#include "WifiManager.h"

static TaskHandle_t serverTaskHandle;

#define SERVER_STACK_SIZE (8192 * 8) // 64KB stack

[[noreturn]] static void serverTask()
{
    // Configure watchdog for ESP32-S3 (new API)
    // Only initialize once - check if already initialized
    // if (esp_task_wdt_status(nullptr) == ESP_ERR_NOT_FOUND)
    // {
    //     constexpr esp_task_wdt_config_t twdt_config = {
    //         .timeout_ms = 10000,                   // 10 second timeout
    //         .idle_core_mask = (1 << 0) | (1 << 1), // Both cores
    //         .trigger_panic = true                  // Panic on timeout
    //     };
    //     esp_task_wdt_init(&twdt_config);
    // }
    // esp_task_wdt_add(nullptr);

    MqttServer::setup();
    DashboardServer::setup();

    MqttServer::registerCallback("config/wifi/data",WifiManager::wifiConfigCallback);

    unsigned long lastWatchdogFeed = millis();

    while (true)
    {
        // Handle MQTT
        MqttServer::loop();

        // Handle HTTP server
        DashboardServer::loop();

        // Feed watchdog every 100ms
        if (millis() - lastWatchdogFeed > 100)
        {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }

        // Don't delay too much, but yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup()
{
    nvs_flash_init();
    Serial.begin(115200);
    // SerialQueue::init();
    delay(100);

    if (!WifiManager::setup()) {
        ESP.restart();
    }

    delay(100);
    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(serverTask),
        "servers",
        SERVER_STACK_SIZE,
        nullptr,
        1, // Priority 1 (was 0)
        &serverTaskHandle,
        0 // Core 0
    );
}

void loop()
{
    SerialQueue::run();
    vTaskDelay(pdMS_TO_TICKS(5));
}