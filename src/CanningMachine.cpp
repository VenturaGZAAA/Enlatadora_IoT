//
// Created by urzu-7 on 10/6/26.
//

#include "CanningMachine.h"


namespace {
int IN_PINS[] = {40, 39, 38, 37, 36, 35, 0, 45};
constexpr int NUM_ENTRADAS = std::size(IN_PINS);

int ACTUATOR_PINS[] = {
  18, 17, 16, 15, 7, 6, 5, 4, 13, 12,
  11, 10, 9, 46, 3, 8, 41, 42, 2
};
constexpr int NUM_ACTUADORES = std::size(ACTUATOR_PINS);
}

void CanningMachine::begin() {
  // configurePins();
  // allOff();

  stage = Stage::Reposo;
  sistemaAutorizado = false;

  startTask();
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

void CanningMachine::configurePins() {
  for (int i = 0; i < NUM_ENTRADAS; ++i) {
    pinMode(IN_PINS[i], INPUT);
  }

  for (int i = 0; i < NUM_ACTUADORES; ++i) {
    pinMode(ACTUATOR_PINS[i], OUTPUT);
  }
}

void CanningMachine::allOff() {
  for (int i = 0; i < NUM_ACTUADORES; ++i) {
    digitalWrite(ACTUATOR_PINS[i], LOW);
  }
}

void CanningMachine::updateAuthorization() {
  if (digitalRead(IN_PINS[4]) != HIGH) sistemaAutorizado = true;
  if (digitalRead(IN_PINS[5]) != LOW)  sistemaAutorizado = false;
}

[[noreturn]] void CanningMachine::run() {
  for (;;) {
    updateAuthorization();

    switch (stage) {
      case Stage::Reposo:
        allOff();
        if (sistemaAutorizado) {
          stage = Stage::EsperaLata;
        } else {
          delay(1);
        }
        break;

      case Stage::EsperaLata:
        digitalWrite(ACTUATOR_PINS[16], HIGH);

        while (digitalRead(IN_PINS[3]) == HIGH && sistemaAutorizado) {
          delay(1);
          updateAuthorization();
        }

        // Original priority: sensor first, then authorization.
        if (digitalRead(IN_PINS[3]) != HIGH) {
          stage = Stage::TopeActivo;
        } else if (!sistemaAutorizado) {
          stage = Stage::Reposo;
        }
        break;

      case Stage::TopeActivo:
        digitalWrite(ACTUATOR_PINS[18], HIGH);
        delay(2000);
        stage = Stage::FrenadoYBajarTope;
        break;

      case Stage::FrenadoYBajarTope:
        digitalWrite(ACTUATOR_PINS[16], LOW);
        digitalWrite(ACTUATOR_PINS[18], LOW);
        stage = Stage::SubirPlataformaTolva;
        break;

      case Stage::SubirPlataformaTolva:
        digitalWrite(ACTUATOR_PINS[17], HIGH);
        delay(1000);
        stage = Stage::Llenado;
        break;

      case Stage::Llenado:
        digitalWrite(ACTUATOR_PINS[1], HIGH);
        delay(1000);
        digitalWrite(ACTUATOR_PINS[1], LOW);
        stage = Stage::BajarPlataformaTolva;
        break;

      case Stage::BajarPlataformaTolva:
        digitalWrite(ACTUATOR_PINS[17], LOW);
        stage = Stage::TrasladoAPlataformaSalida;
        break;

      case Stage::TrasladoAPlataformaSalida:
        digitalWrite(ACTUATOR_PINS[16], HIGH);

        while (digitalRead(IN_PINS[2]) == HIGH) {
          delay(1);
        }

        stage = Stage::MantenerCadena2s;
        break;

      case Stage::MantenerCadena2s:
        delay(2000);
        digitalWrite(ACTUATOR_PINS[16], LOW);
        digitalWrite(ACTUATOR_PINS[0], HIGH);
        stage = Stage::CadenaSalida10s;
        break;

      case Stage::CadenaSalida10s:
        digitalWrite(ACTUATOR_PINS[11], HIGH);
        digitalWrite(ACTUATOR_PINS[15], HIGH);
        digitalWrite(ACTUATOR_PINS[12], HIGH);
        digitalWrite(ACTUATOR_PINS[2], HIGH);

        delay(10000);

        digitalWrite(ACTUATOR_PINS[11], LOW);
        digitalWrite(ACTUATOR_PINS[15], LOW);
        digitalWrite(ACTUATOR_PINS[12], LOW);
        digitalWrite(ACTUATOR_PINS[0], LOW);

        stage = Stage::ExtenderPiston;
        break;

      case Stage::ExtenderPiston:
        digitalWrite(ACTUATOR_PINS[4], HIGH);
        delay(2000);
        stage = Stage::BajarVentosaYTapas;
        break;

      case Stage::BajarVentosaYTapas:
        digitalWrite(ACTUATOR_PINS[5], HIGH);
        digitalWrite(ACTUATOR_PINS[6], HIGH);
        digitalWrite(ACTUATOR_PINS[3], HIGH);

        // Original: waits 1000 ms OR until IN_PINS[7] goes LOW.
        // Poll at 1 ms to preserve the sensor override.
        for (int i = 0; i < 1000; ++i) {
          if (!digitalRead(IN_PINS[7])) {
            digitalWrite(ACTUATOR_PINS[4], LOW);
            digitalWrite(ACTUATOR_PINS[3], LOW);
            digitalWrite(ACTUATOR_PINS[6], LOW);
            digitalWrite(ACTUATOR_PINS[5], LOW);
            digitalWrite(ACTUATOR_PINS[2], LOW);

            stage = Stage::ActivarBanda3;
            break;
          }
          delay(1);
        }

        if (stage == Stage::BajarVentosaYTapas) {
          stage = Stage::RetraerPiston;
        }
        break;

      case Stage::RetraerPiston:
        digitalWrite(ACTUATOR_PINS[4], LOW);
        digitalWrite(ACTUATOR_PINS[3], LOW);
        delay(2000);
        stage = Stage::SoltarTapa;
        break;

      case Stage::SoltarTapa:
        digitalWrite(ACTUATOR_PINS[6], LOW);
        delay(500);
        stage = Stage::RetraerVentosaYBajarPlataforma;
        break;

      case Stage::RetraerVentosaYBajarPlataforma:
        digitalWrite(ACTUATOR_PINS[5], LOW);
        digitalWrite(ACTUATOR_PINS[2], LOW);
        delay(500);
        stage = Stage::BandaTransportadora3;
        break;

      case Stage::BandaTransportadora3:
        digitalWrite(ACTUATOR_PINS[14], HIGH);
        digitalWrite(ACTUATOR_PINS[7], HIGH);
        delay(5000);
        stage = Stage::DesactivarBanda3;
        break;

      case Stage::DesactivarBanda3:
        digitalWrite(ACTUATOR_PINS[14], LOW);
        delay(1000);
        stage = Stage::AsegurarPaletYSoltarTope;
        break;

      case Stage::AsegurarPaletYSoltarTope:
        digitalWrite(ACTUATOR_PINS[8], HIGH);
        digitalWrite(ACTUATOR_PINS[7], LOW);
        delay(1000);
        stage = Stage::LevantarLata;
        break;

      case Stage::LevantarLata:
        digitalWrite(ACTUATOR_PINS[9], HIGH);
        delay(500);
        stage = Stage::VerificacionEstadoLata;
        break;

      case Stage::VerificacionEstadoLata:
        if (digitalRead(IN_PINS[0])) {
          stage = Stage::BajarLata;
        } else {
          stage = Stage::Sellar;
        }
        break;

      case Stage::Sellar:
        digitalWrite(ACTUATOR_PINS[13], HIGH);
        delay(1000);
        stage = Stage::ApagarMotorSellado;
        break;

      case Stage::ApagarMotorSellado:
        digitalWrite(ACTUATOR_PINS[13], LOW);
        delay(500);
        stage = Stage::BajarLata;
        break;

      case Stage::BajarLata:
        digitalWrite(ACTUATOR_PINS[9], LOW);
        delay(500);
        stage = Stage::DesasegurarPalet;
        break;

      case Stage::DesasegurarPalet:
        digitalWrite(ACTUATOR_PINS[8], LOW);
        delay(500);
        stage = Stage::ActivarBanda3;
        break;

      case Stage::ActivarBanda3:
        digitalWrite(ACTUATOR_PINS[14], HIGH);

        while (digitalRead(IN_PINS[1]) == HIGH) {
          delay(1);
        }

        stage = Stage::ApagarBanda3YRepetir;
        break;

      case Stage::ApagarBanda3YRepetir:
        digitalWrite(ACTUATOR_PINS[14], LOW);

        if (sistemaAutorizado) {
          stage = Stage::EsperaLata;
        } else {
          stage = Stage::Reposo;
        }
        break;
    }
  }
}
