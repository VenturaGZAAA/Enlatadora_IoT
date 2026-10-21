//
// Created by urzu-7 on 10/6/26.
//

#include "MachineMaster.h"
#include <ArduinoJson.h>
#include <map>

#include "drivers/universal/Monitor.h"
#include "config/build_flags.h"

#include <utility>

void MachineMaster::setup() {
    controller = UARTManager::create(Serial1, 18, 17);
    controller->registerJsonCallback(processMachineState);
    setUp = true;
}


void MachineMaster::begin() {
    if (!setUp) {
        Monitor::writeLine("Canning machine has not been set up");
        return;
    }
    controller->init();
    startTask();
}

void MachineMaster::processMachineState(const JsonDocument &json) {
    if (json["running"].is<bool>()) {
        machineRunning = json["running"];
    }
    if (json["emergencyStop"].is<bool>()) {
        emergencyStop = json["emergencyStop"];
    }
    if (json["paused"].is<bool>()) {
        machinePaused = json["paused"].as<bool>();
    }

    if (json["authorizedSystem"].is<bool>()) {
        authorizedSystem = json["authorizedSystem"].as<bool>();
    }

    if (json["forcedAuthorization"].is<bool>()) {
        forcedAuthorization = json["forcedAuthorization"].as<bool>();
    }

    if (json["stage_name"].is<String>()) {
        stageName = json["stage_name"].as<String>();
    }
    if (json["stage_num"].is<int>()) {
        stageIndex = json["stage_num"].as<int>();
    }

    if (json["inputs"].is<JsonObjectConst>()) {
        deserializeInputs(json["inputs"].as<JsonObjectConst>());
    } else if (json["inputs"].is<String>()) {
        String inputs = json["inputs"];
        JsonDocument inputsDoc;
        if (const DeserializationError err = deserializeJson(inputsDoc, inputs)) {
            Monitor::writeLine("Error deserializing JSON inputs! -> " + String(err.c_str()));
        } else {
            deserializeInputs(inputsDoc.as<JsonObjectConst>());
        }
    }

    if (json["outputs"].is<JsonObjectConst>()) {
        deserializeOutputs(json["outputs"].as<JsonObjectConst>());
    } else if (json["outputs"].is<String>()) {
        String outputs = json["outputs"];
        JsonDocument outputsDoc;
        if (const DeserializationError err = deserializeJson(outputsDoc, outputs)) {
            Monitor::writeLine("Error deserializing JSON outputs! -> " + String(err.c_str()));
        } else {
            deserializeOutputs(outputsDoc.as<JsonObjectConst>());
        }
    }
}

void MachineMaster::deserializeInputs(const JsonObjectConst json) {
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (json["I" + String(i)].is<bool>()) {
            inputState[i] = json["I" + String(i)].as<bool>();
        }
    }
}

void MachineMaster::deserializeOutputs(const JsonObjectConst json) {
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (json["Q" + String(i)].is<bool>()) {
            outputState[i] = json["Q" + String(i)].as<bool>();
        }
    }
}

void MachineMaster::startTask(const UBaseType_t priority,
                               const uint32_t stackSize,
                               const BaseType_t core) {
    xTaskCreatePinnedToCore(
        reinterpret_cast<TaskFunction_t>(run),
        "CanningMachine",
        stackSize,
        nullptr,
        priority,
        &taskHandle,
        core
    );
}

String MachineMaster::getStage() {
    String result;
    result.reserve(40);
    result += String(stageIndex);
    result += "-";
    result += stageName;
    return result;
}

String MachineMaster::getSerializedState() {
    JsonDocument jsonDoc;
    jsonDoc["running"] = machineRunning;
    jsonDoc["paused"] = machinePaused;
    jsonDoc["authorizedSystem"] = authorizedSystem;
    jsonDoc["forcedAuthorization"] = forcedAuthorization;
    jsonDoc["stage"] = getStage();
    jsonDoc["output_override"] = allowOutputOverride;
    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}

String MachineMaster::getJsonOutputs() {
    JsonDocument jsonDoc;

    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        jsonDoc["Q" + String(i)] = outputState[i];
    }

    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}


String MachineMaster::getJsonInputs() {
    JsonDocument jsonDoc;

    for (int i = 0; i < NUM_INPUTS; ++i) {
        jsonDoc["I" + String(i)] = inputState[i];
    }

    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}

void MachineMaster::outputsWriteJsonCallback(const JsonDocument &doc) {
    bool authorized = false;
    std::map<uint8_t, bool> outputsMap;
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        if (doc["Q" + String(i)].is<bool>()) {
            if (!authorized) {
                if (emergencyStop) {
                    Monitor::writeLine("Emergency stop is active");
                    return;
                }
                if (!allowOutputOverride) {
                    Monitor::writeLine("Output override not enabled");
                    return;
                }
                if (machineRunning && !machinePaused) {
                    Monitor::writeLine(
                        "Output override while the machine is running and not paused is not allowed");
                    return;
                }
                authorized = true;
            }
            outputsMap[i] = doc["Q" + String(i)].as<bool>();
        }
    }
    if (!outputsMap.empty()) {
        JsonDocument json;
        for (const auto &[key, value]: outputsMap) {
            json["Q" + String(key)] = value;
        }
        String msg;
        serializeJson(json, msg);
        controller->writeLine(msg);
    }
}


void MachineMaster::processCommands(const char *command) {
    if (taskHandle == nullptr) return;

    const auto cmd = String(command);

    const String msg = "Command received: " + cmd;
    Monitor::writeLine(msg);

    if (cmd == "AuthorizeOutputOverride") {
        allowOutputOverride = true;
    } else if (cmd == "DisableOutputOverride") {
        allowOutputOverride = false;
    } else if (std::find(allCom.begin(), allCom.end(), cmd) != allCom.end()) {
        processCommand(commandFromString(cmd));
    }
}

MachineMaster::Commands MachineMaster::commandFromString(const String &str) {
    if (std::find(stopCom.begin(), stopCom.end(), str) != stopCom.end()) {
        return Commands::Stop;
    }
    if (std::find(pauseCom.begin(), pauseCom.end(), str) != pauseCom.end()) {
        return Commands::Pause;
    }
    if (std::find(startCom.begin(), startCom.end(), str) != startCom.end()) {
        return Commands::Start;
    }
    if (std::find(resumeCom.begin(), resumeCom.end(), str) != resumeCom.end()) {
        return Commands::Resume;
    }
    return Commands::Unknown;
}

void MachineMaster::processCommand(const Commands command) {
    switch (command) {
        case Commands::Stop:
            controller->writeLine("Stop");
            if (onStopCommand)
                onStopCommand();
            break;
        case Commands::Pause:
            controller->writeLine("Pause");
            if (onPauseCommand)
                onPauseCommand();
            break;
        case Commands::Start:
            controller->writeLine("Start");
            if (onStartCommand)
                onStartCommand();
            break;
        case Commands::Resume:
            controller->writeLine("Resume");
            if (onResumeCommand)
                onResumeCommand();
            break;
        case Commands::Unknown:
            Monitor::writeLine("Unknown command received");
            break;
    }
}

void MachineMaster::setOnStopCallback(std::function<void()> callback) {
    onStopCommand = std::move(callback);
}

void MachineMaster::setOnStartCallback(std::function<void()> callback) {
    onStartCommand = std::move(callback);
}

void MachineMaster::setOnPauseCallback(std::function<void()> callback) {
    onPauseCommand = std::move(callback);
}

void MachineMaster::setOnResumeCallback(std::function<void()> callback) {
    onResumeCommand = std::move(callback);
}

[[noreturn]] void MachineMaster::run() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
