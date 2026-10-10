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
    static const char* stageName(Stage s);
    static constexpr int NUM_INPUTS  = 8;
    static constexpr int NUM_OUTPUTS = 19;

    static constexpr int IN_PINS[NUM_INPUTS] = {
        40, 39, 38, 37, 36, 35, 0, 45
    };

    static constexpr int ACTUATOR_PINS[NUM_OUTPUTS] = {
        18, 17, 16, 15, 7, 6, 5, 4, 13, 12,
        11, 10, 9, 46, 3, 8, 41, 42, 2
    };


    static inline bool inputState[NUM_INPUTS]   = {};
    static inline bool outputState[NUM_OUTPUTS] = {};


    static inline auto stage = Stage::Reposo;
    static inline bool sistemaAutorizado = false;

    static inline bool machineRunning = false;
    static inline bool machinePaused = false;

    static inline bool allowOutputOverride = false;
    static inline TaskHandle_t taskHandle = nullptr;

    [[noreturn]] static  void run();

    static void configurePins();

    static void updateAuthorization();

    static  void startTask(UBaseType_t priority = 1,
                   uint32_t stackSize = 4096,
                   BaseType_t core = 1);

    static void writeOutput(int index, bool value);


    static bool readInput(int index);

public:
    CanningMachine() = delete;

    static void begin();
    static String getJsonOutputs();
    static String getJsonInputs();
    static String getStage();
    static String getSerializedState();
    static void inputsWriteJsonCallback(const JsonDocument &doc);
    static void outputsWriteJsonCallback(const JsonDocument &doc);
    static void allOff();

};
#endif //ENLATADORA_IOT_CANNINGMACHINE_H
