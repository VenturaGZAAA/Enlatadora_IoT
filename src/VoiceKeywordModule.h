//
// Created by urzu-7 on 10/11/26.
//

#ifndef ENLATADORA_IOT_VOICEKEYWORDMODULE_H
#define ENLATADORA_IOT_VOICEKEYWORDMODULE_H

#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- INMP441 I2S Pins (ESP32-S3) ---
#define I2S_MIC_SD_PIN  41
#define I2S_MIC_WS_PIN  40
#define I2S_MIC_SCK_PIN 42
#define I2S_PORT        I2S_NUM_0

// --- Audio Parameters ---
#define SAMPLE_RATE     16000       // 16 kHz — optimal for speech/Whisper
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT // 32-bit slot required for INMP441
#define RECORD_SECONDS  5
#define NUM_SAMPLES     (SAMPLE_RATE * RECORD_SECONDS)  // 80,000 samples
#define BUFFER_SIZE     (NUM_SAMPLES * sizeof(int16_t)) // 160,000 bytes (16-bit PCM)
#define RMS_THRESHOLD   500

// --- Groq API ---
#define GROQ_API_URL    "https://api.groq.com/openai/v1/audio/transcriptions"
#define GROQ_MODEL      "whisper-large-v3-turbo"

// --- Queue ---
#define AUDIO_QUEUE_LENGTH 2
#define AUDIO_QUEUE_ITEM_SIZE sizeof(AudioChunk)

struct AudioChunk {
    int16_t *data;
    size_t length; // number of samples
};

class VoiceKeywordModule {
    static void captureTask(void *param);

    static void uploadTask(void *param);

    static void createWavHeader(uint8_t *header, uint32_t dataSize, uint32_t sampleRate, uint16_t numChannels,
                                uint16_t bitsPerSample);

    static void processTranscript(const String &transcript);

    static QueueHandle_t audioQueue;
    static TaskHandle_t captureTaskHandle;
    static TaskHandle_t uploadTaskHandle;
    static inline bool listening = false;

    struct KeywordEntry {
        String keyword;
        std::function<void()> callback;
    };

    static std::vector<KeywordEntry> keywords;

public:
    static void init();

    static void begin();

    static void registerKeyword(const String &keyword, std::function<void()> callback);

    static bool isListening() { return listening; };

    static void stop();
};

#endif //ENLATADORA_IOT_VOICEKEYWORDMODULE_H
