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
    MqttServer::setup();
    DashboardServer::setup();

    MqttServer::registerCallback("config/wifi/data",WifiManager::wifiConfigCallback);
    MqttServer::registerCallback("config/wifi/get",WifiManager::wifiStateCallback);
    MqttServer::registerCallback("admin/reset/request",[](const char * p) {
        if (String(p) == "1") {
            ESP.restart();
        }
    });

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
            // esp_task_wdt_reset();
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
    SerialQueue::init();
    delay(100);

    SerialQueue::registerCallback(WifiManager::wifiConfigCallback);

    if (!WifiManager::setup()) {
        Serial.println("Failed to setup WiFi");
        ESP.restart();
    }

    delay(100);
    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(serverTask),
        "servers",
        SERVER_STACK_SIZE,
        nullptr,
        1,
        &serverTaskHandle,
        0
    );
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(500));
}