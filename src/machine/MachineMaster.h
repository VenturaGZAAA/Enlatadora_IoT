//
// Created by urzu-7 on 10/6/26.
//

#ifndef ENLATADORA_IOT_CANNINGMACHINE_H
#define ENLATADORA_IOT_CANNINGMACHINE_H

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>
#include <drivers/universal/SerialManager.h>


class MachineMaster {
    enum class Stage : uint8_t {
        Reposo = 0,
        EsperaLata,
        TopeActivo,
        FrenadoYBajarTope,
        SubirPlataformaTolva,
        Llenado,
        BajarPlataformaTolva,
        TrasladoAPlataformaSalida,
        MantenerCadena2s,
        CadenaSalida10s,
        ExtenderPiston,
        BajarVentosaYTapas,
        RetraerPiston,
        SoltarTapa,
        RetraerVentosaYBajarPlataforma,
        BandaTransportadora3,
        DesactivarBanda3,
        AsegurarPaletYSoltarTope,
        LevantarLata,
        VerificacionEstadoLata,
        Sellar,
        ApagarMotorSellado,
        BajarLata,
        DesasegurarPalet,
        ActivarBanda3,
        ApagarBanda3YRepetir
    };

    enum class Commands : uint8_t {
        Stop = 0,
        Start,
        Pause,
        Resume,
        Unknown
    };

    static Commands commandFromString(const String &str);

    static inline auto controller = SerialManager::reserve();

    static constexpr int NUM_INPUTS = 8;
    static constexpr int NUM_OUTPUTS = 19;


    static inline bool inputState[NUM_INPUTS] = {};
    static inline bool outputState[NUM_OUTPUTS] = {};
    static inline String stageName = "";
    static inline uint8_t stageIndex = 0;

    static inline bool setUp = false;

    static inline bool machineRunning = false;
    static inline bool machinePaused = false;
    static inline bool emergencyStop = false;
    static inline bool authorizedSystem = false;
    static inline bool forcedAuthorization = false;

    static inline bool allowOutputOverride = true;
    static inline TaskHandle_t taskHandle = nullptr;

    [[noreturn]] static void run();


    static void startTask(UBaseType_t priority = 1,
                          uint32_t stackSize = 4096,
                          BaseType_t core = 1);

    static void processMachineState(const JsonDocument &json);

    static void deserializeInputs(JsonObjectConst json);

    static void deserializeOutputs(JsonObjectConst json);

    static void processCommand(Commands command);

    static inline std::function<void()> onStopCommand = nullptr;
    static inline std::function<void()> onStartCommand = nullptr;
    static inline std::function<void()> onPauseCommand = nullptr;
    static inline std::function<void()> onResumeCommand = nullptr;
    static inline std::function<void()> onControllerUpdated = nullptr;

public:
    MachineMaster() = delete;

    static const inline std::vector<String> stopCom = {"stop", "disable", "detener", "desactivar"};
    static const inline std::vector<String> startCom = {"start", "enable", "inicia", "arranca"};
    static const inline std::vector<String> pauseCom = {"pause", "emergency", "emergencia", "pausa"};
    static const inline std::vector<String> resumeCom = {"resume", "continue", "continua", "reanuda"};
    static const inline std::vector<String> allCom = [] {
        std::vector v(stopCom);
        v.insert(v.end(), startCom.begin(), startCom.end());
        v.insert(v.end(), pauseCom.begin(), pauseCom.end());
        v.insert(v.end(), resumeCom.begin(), resumeCom.end());
        return v;
    }();


    static void setup();

    static void begin();

    static String getJsonOutputs();

    static String getJsonInputs();

    static String getStage();

    static bool isRunning() { return machineRunning; }
    static bool isPaused() { return machinePaused || emergencyStop; }

    static String getSerializedState();

    static void outputsWriteJsonCallback(const JsonDocument &doc);

    static void processCommands(const char *command);

    static void setOnStopCallback(std::function<void()> callback);

    static void setOnStartCallback(std::function<void()> callback);

    static void setOnPauseCallback(std::function<void()> callback);

    static void setOnResumeCallback(std::function<void()> callback);
};
#endif //ENLATADORA_IOT_CANNINGMACHINE_H
