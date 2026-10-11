/**
 * @file main.cpp
 * @brief Application entry point for the IoT Canning Machine controller.
 * 
 * This file coordinates system initialization and high-level communications:
 * 1. Hardware & Core Subsystems: Non-volatile storage, serial communication, and status LED.
 * 2. Communication Interfaces: Serial command handling, Wi-Fi networking, Web Dashboard, and MQTT.
 * 3. Machine Control: Registers remote and local control endpoints, telemetry publishers,
 *    and launches the canning state machine.
 */

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
#include "VoiceKeywordModule.h"

void setup() {
    // --- 1. Core Hardware & Storage Initialization ---
    // Initialize Non-Volatile Storage (NVS) for persistent configuration storage (e.g., Wi-Fi credentials).
    nvs_flash_init();

    // Initialize serial communication and serial command dispatcher.
    Serial.begin(115200);
    SerialManager::init();

    // Initialize status RGB LED and set to White during early boot sequence.
    EvenTheRGBLedHasManagers::init();
    VoiceKeywordModule::init();   // I2S + queue (no network needed)

    delay(100);
    EvenTheRGBLedHasManagers::setColor(RGBColor(255, 255, 255), 250);

    // --- 2. Serial Interface Callbacks ---
    // Register handlers for incoming JSON payloads and commands received over Serial.
    SerialManager::registerJsonCallback(WifiManager::wifiConfigJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::inputsWriteJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::outputsWriteJsonCallback);
    SerialManager::registerCallback(CanningMachine::processCommands);

    // --- 3. Network Connection (Wi-Fi) ---
    // Connect to configured Wi-Fi network; restart system if configuration/connection fails.
    if (!WifiManager::setup()) {
        Serial.println("Failed to setup WiFi");
        ESP.restart();
    }

    delay(150);

    // --- 4. MQTT Broker & Communication Setup ---
    MqttManager::setup();

    // MQTT Subscription: Remote Wi-Fi configuration update via JSON.
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

    // MQTT Subscription: Administrative reboot request (safely shuts down actuators first).
    MqttManager::registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    // MQTT Subscription: Direct remote actuation / manual output control via JSON.
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
    // MQTT Subscription: Input simulation for automated testing and validation.
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

    // MQTT Subscription: High-level operational commands (Start, Stop, Pause, etc.).
    MqttManager::registerCallback("machine/command", CanningMachine::processCommands);

    // --- 5. Periodic Telemetry Publishing ---
    // Publish Wi-Fi status every 5 seconds.
    MqttManager::registerPeriodicPublish("config/wifi/state", 5000, WifiManager::getState);
    // Publish overall canning machine state every 1 second.
    MqttManager::registerPeriodicPublish("machine/state/read", 1000, CanningMachine::getSerializedState);
    // Stream real-time output and input pin states every 200 ms.
    MqttManager::registerPeriodicPublish("machine/IO/outputs/read", 200, CanningMachine::getJsonOutputs);
    MqttManager::registerPeriodicPublish("machine/IO/inputs/read", 200, CanningMachine::getJsonInputs);

    // Start background MQTT networking tasks.
    MqttManager::begin();

    // --- 6. Web Dashboard Server ---
    // Start local HTTP dashboard server for local monitoring and control.
    DashboardServer::setup();
    // Register keyword → event handlers
    VoiceKeywordModule::registerKeyword("start", []() {
        CanningMachine::processCommands("start");
        EvenTheRGBLedHasManagers::setColor(RGBColor(0, 255, 0), 500);
    });

    VoiceKeywordModule::registerKeyword("stop", []() {
        CanningMachine::processCommands("stop");
        EvenTheRGBLedHasManagers::setColor(RGBColor(255, 0,255), 500);
    });

    VoiceKeywordModule::begin();

    // --- 7. Canning Machine Logic & Startup Completion ---
    // Initialize hardware pins, sensors, and actuators for the canning machine.
    CanningMachine::setup();
    delay(100);

    // Indicate successful initialization and operational readiness with Green LED.
    EvenTheRGBLedHasManagers::setColor(RGBColor(0, 255, 0), 1000);

    // Launch asynchronous state machine tasks for canning operations.
    CanningMachine::begin();
}

void loop() {
    // Primary application workflows run in asynchronous FreeRTOS tasks and callbacks.
    // Yield CPU time here to maximize efficiency and prevent watchdog resets.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
