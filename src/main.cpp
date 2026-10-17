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
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize serial communication and serial command dispatcher.
    Serial.begin(115200);

    Serial.printf("PSRAM found: %d\n", psramFound());
    Serial.printf("PSRAM size:  %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM:  %u bytes\n", ESP.getFreePsram());
    Serial.printf("Free Heap:   %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest PSRAM block: %u bytes\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    Serial.printf("Largest HEAP  block: %u bytes\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    SerialManager::init();

    // Initialize status RGB LED and set to White during early boot sequence.
    EvenTheRGBLedHasManagers::init();
    VoiceKeywordModule::init();

    delay(100);
    EvenTheRGBLedHasManagers::setColor(RGBColor::White(), 250);

    // --- 2. Serial Interface Callbacks ---
    // Register handlers for incoming JSON payloads and commands received over Serial.
    SerialManager::registerJsonCallback(WifiManager::wifiConfigJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::inputsWriteJsonCallback);
    SerialManager::registerJsonCallback(CanningMachine::outputsWriteJsonCallback);
    SerialManager::registerCallback(CanningMachine::processCommands);
    SerialManager::registerCallback(VoiceKeywordModule::processCommands);

    // --- 3. Network Connection (Wi-Fi) ---
    // Connect to configured Wi-Fi network; restart system if configuration/connection fails.
    if (!WifiManager::setup()) {
        Serial.println("Failed to setup WiFi");
        delay(1000);
        ESP.restart();
    }
    if (WifiManager::isFailedToConnect()) {
        SerialManager::enqueueLine("Failed to connect to WiFi");
        EvenTheRGBLedHasManagers::setColor(RGBColor::Red(), 500);
    }
    delay(750);

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
        EvenTheRGBLedHasManagers::setColor(RGBColor::Green().withAlpha(0.4f), 500);
    });

    // MQTT Subscription: Administrative reboot request (safely shuts down actuators first).
    MqttManager::registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            EvenTheRGBLedHasManagers::setColor(RGBColor::Yellow(), 500);
            CanningMachine::allOff();
            ESP.restart();
        }
    });

    MqttManager::registerCallback("voice/keyword/command", VoiceKeywordModule::processCommands);

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
    MqttManager::registerPeriodicPublish("config/wifi/state", 3000, WifiManager::getSerializedState);
    // Publish overall canning machine state every 1 second.
    MqttManager::registerPeriodicPublish("machine/state/read", 1000, CanningMachine::getSerializedState);
    // Stream real-time output and input pin states every 200 ms.
    MqttManager::registerPeriodicPublish("machine/IO/outputs/read", 200, CanningMachine::getJsonOutputs);
    MqttManager::registerPeriodicPublish("machine/IO/inputs/read", 200, CanningMachine::getJsonInputs);
    MqttManager::registerPeriodicPublish("voice/keyword/listening", 200, [] {
        return String(VoiceKeywordModule::isListening());
    });
    // Start background MQTT networking tasks.
    MqttManager::begin();

    // --- 6. Web Dashboard Server ---
    // Start local HTTP dashboard server for local monitoring and control.
    DashboardServer::setup();

    DashboardServer::setOnRootRequested([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Yellow().withAlphaInt(200), 250);
    });

    // Register keyword → event handlers
    VoiceKeywordModule::registerKeyword(CanningMachine::allCom, [](const String &cmd) {
        CanningMachine::processCommands(cmd.c_str());
        MqttManager::publish("voice/keyword/match", cmd);
    },true);


    EvenTheRGBLedHasManagers::setColorGenerator([] {

        if (VoiceKeywordModule::isListening()) {
            return RGBColor::Blue().withAlpha(0.5f);
        }

        if (CanningMachine::isPaused()) {
            return RGBColor::Magenta().withAlpha(0.5f);
        }

        if (CanningMachine::isRunning()) {
            return RGBColor::Green().withAlpha(0.5f);
        }

        return RGBColor();
    });

    VoiceKeywordModule::setOnTranscriptionReceived([](const String &transcription) {
        MqttManager::publish("voice/keyword/transcription", transcription);
    });

    // --- 7. Canning Machine Logic & Startup Completion ---
    // Initialize hardware pins, sensors, and actuators for the canning machine.
    CanningMachine::setup();

    CanningMachine::setOnStopCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Red(),1000);
    });
    CanningMachine::setOnStartCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Green(),1000);
    });
    CanningMachine::setOnPauseCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Magenta(),1000);
    });

    delay(100);

    // Indicate successful initialization and operational readiness with Green LED.
    EvenTheRGBLedHasManagers::setColor(RGBColor::Green(), 1000);

    // Launch asynchronous state machine tasks for canning operations.
    CanningMachine::begin();
}

void loop() {
    // Primary application workflows run in asynchronous FreeRTOS tasks and callbacks.
    // Yield CPU time here to maximize efficiency and prevent watchdog resets.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
