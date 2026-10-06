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



void setup()
{
    nvs_flash_init();
    Serial.begin(115200);
    SerialManager::init();
    delay(100);

    SerialManager::registerCallback(WifiManager::wifiConfigCallback);

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

void loop()
{
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,0,0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
    neopixelWrite(RGB_BUILTIN,0,0,0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY));
}


[[noreturn]] static void serverTask()
{
    MqttServer::setup();

    static bool shouldUpdateWifi = false;

    MqttServer::registerCallback("config/wifi/data",WifiManager::wifiConfigCallback);
    MqttServer::registerCallback("config/wifi/get",[](const char * p) {
        shouldUpdateWifi = true;
    });
    MqttServer::registerCallback("admin/reset/request",[](const char * p) {
        if (String(p) == "1") {
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    unsigned long lastWatchdogFeed = millis();
    unsigned long lastWifiUpdate = millis();

    while (true)
    {
        MqttServer::loop();

        if (millis() - lastWatchdogFeed > 100)
        {
            esp_task_wdt_reset();
            lastWatchdogFeed = millis();
        }
        if (millis() - lastWifiUpdate > 5000 || shouldUpdateWifi) {
            MqttServer::publish("config/wifi/state",WifiManager::getState());
            SerialManager::enqueueLine("Wifi state published!");
            shouldUpdateWifi = false;
            lastWifiUpdate = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
