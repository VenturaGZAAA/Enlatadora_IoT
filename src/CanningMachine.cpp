//
// Created by urzu-7 on 10/6/26.
//

#include "CanningMachine.h"

void CanningMachine::begin() {
  configurePins();
  allOff();

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
  for (int i = 0; i < NUM_INPUTS; ++i) {
    pinMode(IN_PINS[i], INPUT);
    inputState[i] = (digitalRead(IN_PINS[i]) == HIGH);
  }

  for (int i = 0; i < NUM_OUTPUTS; ++i) {
    pinMode(ACTUATOR_PINS[i], OUTPUT);
    outputState[i] = false;
  }
}

void CanningMachine::writeOutput(const int index, const bool value) {
  outputState[index] = value;
  digitalWrite(ACTUATOR_PINS[index], value ? HIGH : LOW);
}

bool CanningMachine::readInput(const int index) {
  inputState[index] = (digitalRead(IN_PINS[index]) == HIGH);
  return inputState[index];
}

void CanningMachine::allOff() {
  for (int i = 0; i < NUM_OUTPUTS; ++i) {
    writeOutput(i, false);
  }
}

void CanningMachine::updateAuthorization() {
  if (!readInput(4)) sistemaAutorizado = true;
  if ( readInput(5)) sistemaAutorizado = false;
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
        writeOutput(16, true);

        while (readInput(3) && sistemaAutorizado) {
          delay(1);
          updateAuthorization();
        }

        // Original priority: sensor first, then authorization.
        if (!readInput(3)) {
          stage = Stage::TopeActivo;
        } else if (!sistemaAutorizado) {
          stage = Stage::Reposo;
        }
        break;

      case Stage::TopeActivo:
        writeOutput(18, true);
        delay(2000);
        stage = Stage::FrenadoYBajarTope;
        break;

      case Stage::FrenadoYBajarTope:
        writeOutput(16, false);
        writeOutput(18, false);
        stage = Stage::SubirPlataformaTolva;
        break;

      case Stage::SubirPlataformaTolva:
        writeOutput(17, true);
        delay(1000);
        stage = Stage::Llenado;
        break;

      case Stage::Llenado:
        writeOutput(1, true);
        delay(1000);
        writeOutput(1, false);
        stage = Stage::BajarPlataformaTolva;
        break;

      case Stage::BajarPlataformaTolva:
        writeOutput(17, false);
        stage = Stage::TrasladoAPlataformaSalida;
        break;

      case Stage::TrasladoAPlataformaSalida:
        writeOutput(16, true);

        while (readInput(2)) {
          delay(1);
        }

        stage = Stage::MantenerCadena2s;
        break;

      case Stage::MantenerCadena2s:
        delay(2000);
        writeOutput(16, false);
        writeOutput(0, true);
        stage = Stage::CadenaSalida10s;
        break;

      case Stage::CadenaSalida10s:
        writeOutput(11, true);
        writeOutput(15, true);
        writeOutput(12, true);
        writeOutput(2, true);

        delay(10000);

        writeOutput(11, false);
        writeOutput(15, false);
        writeOutput(12, false);
        writeOutput(0, false);

        stage = Stage::ExtenderPiston;
        break;

      case Stage::ExtenderPiston:
        writeOutput(4, true);
        delay(2000);
        stage = Stage::BajarVentosaYTapas;
        break;

      case Stage::BajarVentosaYTapas:
        writeOutput(5, true);
        writeOutput(6, true);
        writeOutput(3, true);

        // Original: waits 1000 ms OR until IN_PINS[7] goes LOW.
        // Poll at 1 ms to preserve the sensor override.
        for (int i = 0; i < 1000; ++i) {
          if (!readInput(7)) {
            writeOutput(4, false);
            writeOutput(3, false);
            writeOutput(6, false);
            writeOutput(5, false);
            writeOutput(2, false);

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
        writeOutput(4, false);
        writeOutput(3, false);
        delay(2000);
        stage = Stage::SoltarTapa;
        break;

      case Stage::SoltarTapa:
        writeOutput(6, false);
        delay(500);
        stage = Stage::RetraerVentosaYBajarPlataforma;
        break;

      case Stage::RetraerVentosaYBajarPlataforma:
        writeOutput(5, false);
        writeOutput(2, false);
        delay(500);
        stage = Stage::BandaTransportadora3;
        break;

      case Stage::BandaTransportadora3:
        writeOutput(14, true);
        writeOutput(7, true);
        delay(5000);
        stage = Stage::DesactivarBanda3;
        break;

      case Stage::DesactivarBanda3:
        writeOutput(14, false);
        delay(1000);
        stage = Stage::AsegurarPaletYSoltarTope;
        break;

      case Stage::AsegurarPaletYSoltarTope:
        writeOutput(8, true);
        writeOutput(7, false);
        delay(1000);
        stage = Stage::LevantarLata;
        break;

      case Stage::LevantarLata:
        writeOutput(9, true);
        delay(500);
        stage = Stage::VerificacionEstadoLata;
        break;

      case Stage::VerificacionEstadoLata:
        if (readInput(0)) {
          stage = Stage::BajarLata;
        } else {
          stage = Stage::Sellar;
        }
        break;

      case Stage::Sellar:
        writeOutput(13, true);
        delay(1000);
        stage = Stage::ApagarMotorSellado;
        break;

      case Stage::ApagarMotorSellado:
        writeOutput(13, false);
        delay(500);
        stage = Stage::BajarLata;
        break;

      case Stage::BajarLata:
        writeOutput(9, false);
        delay(500);
        stage = Stage::DesasegurarPalet;
        break;

      case Stage::DesasegurarPalet:
        writeOutput(8, false);
        delay(500);
        stage = Stage::ActivarBanda3;
        break;

      case Stage::ActivarBanda3:
        writeOutput(14, true);

        while (readInput(1)) {
          delay(1);
        }

        stage = Stage::ApagarBanda3YRepetir;
        break;

      case Stage::ApagarBanda3YRepetir:
        writeOutput(14, false);

        if (sistemaAutorizado) {
          stage = Stage::EsperaLata;
        } else {
          stage = Stage::Reposo;
        }
        break;
    }
  }
}