//
// Created by urzu-7 on 10/6/26.
//

#ifndef ENLATADORA_IOT_CANNINGMACHINE_H
#define ENLATADORA_IOT_CANNINGMACHINE_H

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

    static inline auto stage = Stage::Reposo;
    static inline bool sistemaAutorizado = false;
    static inline TaskHandle_t taskHandle = nullptr;

    [[noreturn]] static  void run();

    static void configurePins();

    static void updateAuthorization();

    static  void startTask(UBaseType_t priority = 1,
                   uint32_t stackSize = 4096,
                   BaseType_t core = 1);

public:
    CanningMachine() = delete;

    static void begin();
    static void allOff();

};
#endif //ENLATADORA_IOT_CANNINGMACHINE_H
