
#include <WiFi.h>
#include <PicoMQTT.h>
#include <nvs_flash.h>
#if __has_include("config/build_flags.h")
#include "config/build_flags.h"
#endif
#include "config/secrets.h"
#include "audio/VoiceKeywordModule.h"
#include "drivers/s3/EvenTheRGBLedHasManagers.h"
#include "drivers/specific/SdCardManager.h"
#include "drivers/universal/Monitor.h"
#include "drivers/universal/SerialManager.h"
#include "drivers/universal/WifiManager.h"
#include "machine/MachineMaster.h"
#include "network/MqttManager.h"
#include "network/WebSeverManager.h"

/*  ====== POWER CONSUMPTION IN STA MODE =====
 *  current hardware: ESP32-S3 (this), ESP32-S3 (empty), microphone, sd card reader.
 *  idle: ~130 mA
 *  on root sent: ~210 mA
 *  while mqtt client connected: ~130 mA to ~150 mA
 *  on IO page: ~150 mA
 *  while audio capture: ~130 mA to ~150 mA
 *  on audio/http transaction: ~220 mA
 */


void setup() {
    // --- 1. Core Hardware & Storage Initialization ---
    // Initialize Non-Volatile Storage (NVS) for persistent configuration storage (e.g., Wi-Fi credentials).
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    Monitor::useSerialManager(SerialManager::create(Serial,-1,-1,115200,20));

    /* Other debug interfaces for Monitor might be attached here in the future.
     *
     *
     */

    Serial.printf("PSRAM found: %d\n", psramFound());
    Serial.printf("PSRAM size:  %u bytes\n", static_cast<unsigned int>(ESP.getPsramSize()));
    Serial.printf("Free PSRAM:  %u bytes\n", static_cast<unsigned int>(ESP.getFreePsram()));
    Serial.printf("Free Heap:   %u bytes\n", static_cast<unsigned int>(ESP.getFreeHeap()));
    Serial.printf("Largest PSRAM block: %u bytes\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    Serial.printf("Largest HEAP  block: %u bytes\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    Monitor::init();

    SdCardManager::init("/CanningMachine");

    EvenTheRGBLedHasManagers::init();

    VoiceKeywordModule::setGroqApiKey(GROQ_API_KEY);
    VoiceKeywordModule::init();

    delay(100);
    EvenTheRGBLedHasManagers::setColor(RGBColor::White(), 250);

    // --- 2. Monitor Interface Callbacks ---
    // Register handlers for incoming JSON payloads and commands received over Monitor interfaces (Serial, WebSocket, etc.).
    Monitor::registerJsonCallback(WifiManager::wifiConfigJsonCallback);
    Monitor::registerJsonCallback(MachineMaster::outputsWriteJsonCallback);
    Monitor::registerCallback(MachineMaster::processCommands);
    Monitor::registerCallback(VoiceKeywordModule::processCommands);

    // --- 3. Network Connection (Wi-Fi) ---
    // Connect to configured Wi-Fi network; restart system if configuration/connection fails.
    WifiManager::setHostname("enlatadora-master");
    if (!WifiManager::setup("Enlatadora-s3", "12345678")) {
        Serial.println("Failed to setup WiFi");
        delay(1000);
        ESP.restart();
    }
    if (WifiManager::isFailedToConnect()) {
        Monitor::writeLine("Failed to connect to WiFi");
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
            Monitor::write("Error deserializing WiFi config json on MQTT: ");
            Monitor::writeLine(error.c_str());
            return;
        }
        WifiManager::wifiConfigJsonCallback(doc);
        EvenTheRGBLedHasManagers::setColor(RGBColor::Green().withAlpha(0.4f), 500);
    });

    // MQTT Subscription: Administrative reboot request (safely shuts down actuators first).
    MqttManager::registerCallback("admin/reset/request", [](const char *p) {
        if (String(p) == "1") {
            EvenTheRGBLedHasManagers::setColor(RGBColor::Yellow(), 500);

            ESP.restart();
        }
    });

    MqttManager::registerCallback("voice/keyword/command", VoiceKeywordModule::processCommands);

    // MQTT Subscription: Direct remote actuation / manual output control via JSON.
    MqttManager::registerCallback("machine/IO/outputs/write", [](const char *payload) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            Monitor::write("Error deserializing outputs write over MQTT: ");
            Monitor::writeLine(error.c_str());
            return;
        }
        MachineMaster::outputsWriteJsonCallback(doc);
    });


    // MQTT Subscription: High-level operational commands (Start, Stop, Pause, etc.).
    MqttManager::registerCallback("machine/command", MachineMaster::processCommands);

    // --- 5. Periodic Telemetry Publishing ---
    // Publish Wi-Fi status every 3 seconds.
    MqttManager::registerPeriodicPublish("config/wifi/state", 3000, WifiManager::getSerializedState);
    // Publish overall canning machine state every 1 second.
    MqttManager::registerPeriodicPublish("machine/state/read", 1000, MachineMaster::getSerializedState);
    // Stream real-time output and input pin states every 200 ms.
    MqttManager::registerPeriodicPublish("machine/IO/outputs/read", 200, MachineMaster::getJsonOutputs);
    MqttManager::registerPeriodicPublish("machine/IO/inputs/read", 200, MachineMaster::getJsonInputs);
    MqttManager::registerPeriodicPublish("voice/keyword/listening", 200, [] {
        return String(VoiceKeywordModule::isListening());
    });
    // Start background MQTT networking tasks.
    MqttManager::begin();

    // --- 6. Web Dashboard Server & Debug WebSocket ---
    // Register WebSocket handler for Monitor and setup endpoints before starting HTTP server.
#ifdef ENABLE_USE_ASYNC_WEBSOCKET_SERVER
    WebSeverManager::addHandler(&Monitor::enableAsyncWebSocketHandler());
#endif
    WebSeverManager::enableWifiPostEndpoint("/wifi/set");
    WebSeverManager::enableOTASupport("admin", "admin");
    WebSeverManager::setup();

    WebSeverManager::setOnRootRequested([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Yellow().withAlphaInt(200), 250);
    });


    // Register keyword → event handlers
    VoiceKeywordModule::registerKeyword(MachineMaster::allCom, [](const String &cmd) {
        MachineMaster::processCommands(cmd.c_str());
        MqttManager::publish("voice/keyword/match", cmd);
    }, true);


    EvenTheRGBLedHasManagers::setColorGenerator([] {
        if (VoiceKeywordModule::isListening()) {
            return RGBColor::Blue().withAlpha(0.5f);
        }

        if (MachineMaster::isPaused()) {
            return RGBColor::Magenta().withAlpha(0.5f);
        }

        if (MachineMaster::isRunning()) {
            return RGBColor::Green().withAlpha(0.5f);
        }

        return RGBColor();
    });

    VoiceKeywordModule::setOnTranscriptionReceived([](const String &transcription) {
        MqttManager::publish("voice/keyword/transcription", transcription);
    });

    // --- 7. Canning Machine Logic & Startup Completion ---
    // Initialize hardware pins, sensors, and actuators for the canning machine.
    MachineMaster::setup();

    MachineMaster::setOnStopCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Red(), 1000);
    });
    MachineMaster::setOnStartCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Green(), 1000);
    });
    MachineMaster::setOnPauseCallback([] {
        EvenTheRGBLedHasManagers::setColor(RGBColor::Magenta(), 1000);
    });

    delay(100);

    // Indicate successful initialization and operational readiness with Green LED.
    EvenTheRGBLedHasManagers::setColor(RGBColor::Green(), 1000);

    // Launch asynchronous state machine tasks for canning operations.
    MachineMaster::begin();
}

// #define RGB_TEST
void loop() {
#ifndef RGB_TEST
    vTaskDelay(pdMS_TO_TICKS(1000));
#else

    constexpr float alpha = 0.5f;
    const RGBColor color1 = RGBColor::red().withAlpha(alpha);
    constexpr uint32_t delay1 = 750;
    const RGBColor color2 = RGBColor::white().withAlpha(alpha);
    constexpr uint32_t delay2 = 500;

    EvenTheRGBLedHasManagers::setColor(color1, delay1);
    vTaskDelay(pdMS_TO_TICKS(delay1));
    EvenTheRGBLedHasManagers::setColor(RGBColor(), 100);
    vTaskDelay(pdMS_TO_TICKS(100));
    EvenTheRGBLedHasManagers::setColor(color2, delay2);
    vTaskDelay(pdMS_TO_TICKS(delay2));
    EvenTheRGBLedHasManagers::setColor(RGBColor(), 100);
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
}
