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


class CanningMachine {
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
        Unknown
    };

    static Commands commandFromString(const String &str);

    static const char *stageName(Stage s);

    static constexpr int NUM_INPUTS = 8;
    static constexpr int NUM_OUTPUTS = 19;

    static constexpr int IN_PINS[NUM_INPUTS] = {
        40, 39, 38, 37, 36, 35, 0, 45
    };

    static constexpr int ACTUATOR_PINS[NUM_OUTPUTS] = {
        18, 17, 16, 15, 7, 6, 5, 4, 13, 12,
        11, 10, 9, 46, 3, 8, 41, 42, 2
    };


    static inline bool inputState[NUM_INPUTS] = {};
    static inline bool outputState[NUM_OUTPUTS] = {};
    static inline bool setUp = false;

    static inline auto stage = Stage::Reposo;
    static inline bool sistemaAutorizado = false;

    static inline bool machineRunning = false;
    static inline bool machinePaused = false;

    static inline bool allowOutputOverride = true;
    static inline TaskHandle_t taskHandle = nullptr;

    [[noreturn]] static void run();

    static void configurePins();

    static void updateAuthorization();

    static void startTask(UBaseType_t priority = 1,
                          uint32_t stackSize = 4096,
                          BaseType_t core = 1);

    static void writeOutput(int index, bool value);


    static bool readInput(int index, bool update = false);
    static inline uint8_t autoUpdatePeriod = 10;
    static inline TaskHandle_t autoUpdateTaskHandle = nullptr;
    [[noreturn]] static void autoUpdateLoop();

    static void processCommand(Commands command);
    
    static inline std::function<void()> onStopCommand = nullptr;
    static inline std::function<void()> onStartCommand = nullptr;
    static inline std::function<void()> onPauseCommand = nullptr;


public:
    CanningMachine() = delete;

    static const inline std::vector<String> stopCom = { "stop" ,"disable","detener","desactivar"};
    static const inline std::vector<String> startCom = { "start" ,"enable","inicia","arranca"};
    static const inline std::vector<String> pauseCom = { "pause" ,"emergency","emergencia","pausa"};

    static const inline std::vector<String> allCom = [] {
        std::vector v(stopCom);
        v.insert(v.end(), startCom.begin(), startCom.end());
        v.insert(v.end(), pauseCom.begin(), pauseCom.end());
        return v;
    }();


    static void setup(bool startAutoUpdate = false);

    static void begin();

    static String getJsonOutputs();

    static String getJsonInputs();

    static String getStage();

    static String getSerializedState();

    static void inputsWriteJsonCallback(const JsonDocument &doc);

    static void outputsWriteJsonCallback(const JsonDocument &doc);

    static void processCommands(const char *command);

    static void setOnStopCallback(std::function<void()> callback);
    static void setOnStartCallback(std::function<void()> callback);
    static void setOnPauseCallback(std::function<void()> callback);

    static void allOff();

    static void stop();
};
#endif //ENLATADORA_IOT_CANNINGMACHINE_H
