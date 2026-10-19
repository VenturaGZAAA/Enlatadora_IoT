//
// Created by urzu-7 on 10/6/26.
//

#include "CanningMachine.h"
#include <ArduinoJson.h>
#include <map>

#include "drivers/universal/SerialMonitor.h"
#include "config/build_flags.h"

#include <utility>

void CanningMachine::setup() {
    controller = UARTManager::create(Serial1, 18, 17);
    controller->registerJsonCallback(processMachineState);
    setUp = true;
}


void CanningMachine::begin() {
    if (!setUp) {
        SerialMonitor::enqueueLine("Canning machine has not been set up");
        return;
    }
    controller->init();
    startTask();
}

void CanningMachine::processMachineState(const JsonDocument &json) {
    if (json["running"].is<bool>()) {
        machineRunning = json["running"];
    }
    if (json["emergencyStop"].is<bool>()) {
        emergencyStop = json["emergencyStop"];
    }
    if (json["paused"].is<bool>()) {
        machinePaused = json["paused"].as<bool>();
    }
    if (json["stage_name"].is<String>()) {
        stageName = json["stage_name"].as<String>();
    }
    if (json["stage_num"].is<int>()) {
        stageIndex = json["stage_num"].as<int>();
    }

    if (json["inputs"].is<String>()) {
        String inputs = json["inputs"];
        JsonDocument inputsDoc;
        if (const DeserializationError err = deserializeJson(inputsDoc, inputs)) {
            SerialMonitor::enqueueLine("Error deserializing JSON inputs! -> " + String(err.c_str()));
        } else {
            deserializeInputs(inputsDoc);
        }
    }
    if (json["outputs"].is<String>()) {
        String outputs = json["outputs"];
        JsonDocument outputsDoc;
        if (const DeserializationError err = deserializeJson(outputsDoc, outputs)) {
            SerialMonitor::enqueueLine("Error deserializing JSON outputs! -> " + String(err.c_str()));
        } else {
            deserializeOutputs(outputsDoc);
        }
    }
}

void CanningMachine::deserializeInputs(const JsonDocument &json) {
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (json["I" + String(i)].is<bool>()) {
            inputState[i] = json["I" + String(i)].as<bool>();
        }
    }
}

void CanningMachine::deserializeOutputs(const JsonDocument &json) {
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (json["Q" + String(i)].is<bool>()) {
            outputState[i] = json["Q" + String(i)].as<bool>();
        }
    }
}

void CanningMachine::startTask(const UBaseType_t priority,
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

String CanningMachine::getStage() {
    String result;
    result.reserve(40);
    result += String(stageIndex);
    result += "-";
    result += stageName;
    return result;
}

String CanningMachine::getSerializedState() {
    JsonDocument jsonDoc;
    jsonDoc["running"] = machineRunning;
    jsonDoc["paused"] = machinePaused;
    jsonDoc["stage"] = getStage();
    jsonDoc["output_override"] = allowOutputOverride;
    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}

String CanningMachine::getJsonOutputs() {
    JsonDocument jsonDoc;

    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        jsonDoc["Q" + String(i)] = outputState[i];
    }

    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}


String CanningMachine::getJsonInputs() {
    JsonDocument jsonDoc;

    for (int i = 0; i < NUM_INPUTS; ++i) {
        jsonDoc["I" + String(i)] = inputState[i];
    }

    String jsonOutput;
    serializeJson(jsonDoc, jsonOutput);
    return jsonOutput;
}

void CanningMachine::outputsWriteJsonCallback(const JsonDocument &doc) {
    bool authorized = false;
    std::map<uint8_t, bool> outputsMap;
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        if (doc["Q" + String(i)].is<bool>()) {
            if (!authorized) {
                if (emergencyStop) {
                    SerialMonitor::enqueueLine("Emergency stop is active");
                    return;
                }
                if (!allowOutputOverride) {
                    SerialMonitor::enqueueLine("Output override not enabled");
                    return;
                }
                if (machineRunning && !machinePaused) {
                    SerialMonitor::enqueueLine(
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
        controller->enqueueLine(msg);
    }
}


void CanningMachine::processCommands(const char *command) {
    if (taskHandle == nullptr) return;

    const auto cmd = String(command);

    const String msg = "Command received: " + cmd;
    SerialMonitor::enqueueLine(msg);

    if (cmd == "AuthorizeOutputOverride") {
        allowOutputOverride = true;
    } else if (cmd == "DisableOutputOverride") {
        allowOutputOverride = false;
    } else if (std::find(allCom.begin(), allCom.end(), cmd) != allCom.end()) {
        processCommand(commandFromString(cmd));
    }
}

CanningMachine::Commands CanningMachine::commandFromString(const String &str) {
    if (std::find(stopCom.begin(), stopCom.end(), str) != stopCom.end()) {
        return Commands::Stop;
    }
    if (std::find(pauseCom.begin(), pauseCom.end(), str) != pauseCom.end()) {
        return Commands::Pause;
    }
    if (std::find(startCom.begin(), startCom.end(), str) != startCom.end()) {
        return Commands::Start;
    }
    return Commands::Unknown;
}

void CanningMachine::processCommand(const Commands command) {
    switch (command) {
        case Commands::Stop:
            controller->enqueueLine("Stop");
            if (onStopCommand)
                onStopCommand();
            break;
        case Commands::Pause:
            controller->enqueueLine("Pause");
            if (onPauseCommand)
                onPauseCommand();
            break;
        case Commands::Start:
            controller->enqueueLine("Start");
            if (onStartCommand)
                onStartCommand();
            break;
        case Commands::Resume:
            controller->enqueueLine("Resume");
            if (onResumeCommand)
                onResumeCommand();
            break;
        case Commands::Unknown:
            break;
    }
}

void CanningMachine::setOnStopCallback(std::function<void()> callback) {
    onStopCommand = std::move(callback);
}

void CanningMachine::setOnStartCallback(std::function<void()> callback) {
    onStartCommand = std::move(callback);
}

void CanningMachine::setOnPauseCallback(std::function<void()> callback) {
    onPauseCommand = std::move(callback);
}

void CanningMachine::setOnResumeCallback(std::function<void()> callback) {
    onResumeCommand = std::move(callback);
}

[[noreturn]] void CanningMachine::run() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CanningMachine::stop() {
    if (taskHandle != nullptr) {
        SerialMonitor::enqueueLine("Stopping canning machine");
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
}
