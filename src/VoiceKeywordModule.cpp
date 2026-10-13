//
// Created by urzu-7 on 10/11/26.
//

#include "VoiceKeywordModule.h"
#include <secrets.h>
#include <utility>
#include <build_flags.h>
#include "SerialManager.h"

QueueHandle_t VoiceKeywordModule::audioQueue = nullptr;
TaskHandle_t VoiceKeywordModule::captureTaskHandle = nullptr;
TaskHandle_t VoiceKeywordModule::uploadTaskHandle = nullptr;
std::vector<VoiceKeywordModule::KeywordEntry> VoiceKeywordModule::keywordSets;

// ─────────────────────────────────────────────────────────────
// I2S Initialization
// ─────────────────────────────────────────────────────────────
static void initI2S() {
    constexpr i2s_config_t i2sConfig = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // L/R pin → GND
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    constexpr i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_MIC_SCK_PIN,
        .ws_io_num = I2S_MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD_PIN
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2sConfig, 0, nullptr));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT, &pinConfig));
    ESP_ERROR_CHECK(i2s_start(I2S_PORT));
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow mic to stabilise
}

// ─────────────────────────────────────────────────────────────
// Init — call from setup() before WifiManager::setup()
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::init() {
#if defined(DISABLE_AUDIO)
    return;
#endif

    audioQueue = xQueueCreate(AUDIO_QUEUE_LENGTH, AUDIO_QUEUE_ITEM_SIZE);
    if (!audioQueue) {
        SerialManager::enqueueLine("[Voice] Failed to create audio queue");
        return;
    }
    initI2S();
    SerialManager::enqueueLine("[Voice] I2S microphone initialised");
}

void VoiceKeywordModule::setOnStarted(std::function<void()> callback) {
    onStarted = std::move(callback);
}

void VoiceKeywordModule::setOnStopped(std::function<void()> callback) {
    onStopped = std::move(callback);
}

// ─────────────────────────────────────────────────────────────
// Start tasks — call from setup() after WifiManager::setup()
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::begin() {

#if defined(DISABLE_AUDIO)
    SerialManager::enqueueLine("[Voice] Audio disabled");
    return;
#endif

    if (captureTaskHandle != nullptr || uploadTaskHandle != nullptr) {
        SerialManager::enqueueLine("[Voice] Tasks already running");
        return;
    }
    if (!audioQueue) {
        audioQueue = xQueueCreate(AUDIO_QUEUE_LENGTH, AUDIO_QUEUE_ITEM_SIZE);
    }
    i2s_start(I2S_PORT);
    xTaskCreatePinnedToCore(
        captureTask, "VoiceCapture",
        8192, nullptr, 5, &captureTaskHandle, 1 // Core 1 — audio capture
    );
    xTaskCreatePinnedToCore(
        uploadTask, "VoiceUpload",
        16384, nullptr, 4, &uploadTaskHandle, 0 // Core 0 — network upload
    );
    listening = true;

    SerialManager::enqueueLine("[Voice] Voice keyword module started");
    if (onStarted) {
        onStarted();
    }
}

void VoiceKeywordModule::processCommands(const char *command) {
    if (const auto msg = String(command); msg == "voiceOn") {
        begin();
    } else if (msg == "voiceOff") {
        stop();
    }
}

// ─────────────────────────────────────────────────────────────
// Stop tasks — safely terminate capture/upload and release queue buffers
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::stop() {

#if defined(DISABLE_AUDIO)
    SerialManager::enqueueLine("[Voice] Audio disabled - can't stop");
    return;
#endif

    if (captureTaskHandle != nullptr) {
        vTaskDelete(captureTaskHandle);
        captureTaskHandle = nullptr;
    }
    if (uploadTaskHandle != nullptr) {
        vTaskDelete(uploadTaskHandle);
        uploadTaskHandle = nullptr;
    }

    // Drain queue and free pending buffers to prevent memory leaks
    if (audioQueue != nullptr) {
        AudioChunk chunk{};
        while (xQueueReceive(audioQueue, &chunk, 0) == pdPASS) {
            if (chunk.data) {
                free(chunk.data);
            }
        }
    }

    i2s_stop(I2S_PORT);
    listening = false;
    SerialManager::enqueueLine("[Voice] Module stopped safely");
    if (onStopped) {
        onStopped();
    }
}

// ─────────────────────────────────────────────────────────────
// Task 1: Capture 5-second audio chunks
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::captureTask(void *param) {
    constexpr size_t DMA_READ_SAMPLES = 256;
    int32_t rawSamples[DMA_READ_SAMPLES];

    while (true) {
        // Allocate buffer for 5 seconds of 16-bit mono audio
        auto *buffer = static_cast<int16_t *>(heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM));
        if (!buffer) {
            buffer = static_cast<int16_t *>(malloc(BUFFER_SIZE)); // fallback to internal RAM
        }
        if (!buffer) {
            SerialManager::enqueueLine("[Voice] Buffer allocation failed — retrying");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        size_t totalSamples = 0;
        int16_t maxSample = INT16_MIN;
        int16_t minSample = INT16_MAX;
        int64_t sumSamples = 0;

        // Read 32-bit samples from I2S and downscale to 16-bit PCM
        while (totalSamples < NUM_SAMPLES) {
            size_t bytesRead = 0;
            const size_t toReadSamples = std::min(DMA_READ_SAMPLES, NUM_SAMPLES - totalSamples);

            const esp_err_t err = i2s_read(
                I2S_PORT,
                rawSamples,
                toReadSamples * sizeof(int32_t),
                &bytesRead,
                portMAX_DELAY
            );
            if (err != ESP_OK) {
                SerialManager::enqueueLine("[Voice] I2S read error: " + std::to_string(err));
                break;
            }

            const size_t samplesRead = bytesRead / sizeof(int32_t);
            for (size_t i = 0; i < samplesRead; i++) {
                // INMP441 outputs 24-bit MSB-aligned data in 32-bit slot.
                // Shifting right by 14 applies +12dB digital gain to boost typical speech volume.
                int32_t val = rawSamples[i] >> 14;

                // Clamp to 16-bit signed integer range
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;

                const auto sample16 = static_cast<int16_t>(val);
                buffer[totalSamples + i] = sample16;

                if (sample16 > maxSample) maxSample = sample16;
                if (sample16 < minSample) minSample = sample16;
                sumSamples += sample16;
            }
            totalSamples += samplesRead;
        }

        // DC offset compensation and audio level calculation
        int32_t rms = 0;
        if (totalSamples > 0) {
            const auto dcOffset = static_cast<int16_t>(sumSamples / totalSamples);
            int64_t sumSquares = 0;
            for (size_t i = 0; i < totalSamples; i++) {
                buffer[i] -= dcOffset;
                sumSquares += static_cast<int64_t>(buffer[i]) * buffer[i];
            }
            rms = static_cast<int32_t>(sqrt(static_cast<double>(sumSquares) / totalSamples));
            SerialManager::enqueueLine("[Voice] Chunk captured (" + String(totalSamples) + " samples) | Peak: [" +
                                       String(minSample) + ".." + String(maxSample) + "] | RMS: " + String(rms));
        }

        // Avoid uploading silent chunks: save network bandwidth, CPU time, and API usage
        if (rms < RMS_THRESHOLD) {
            SerialManager::enqueueLine(
                "[Voice] RMS below threshold (" + String(rms) + " < " + String(RMS_THRESHOLD) + ") — skipping upload");
            free(buffer);
            continue;
        }

        AudioChunk chunk = {
            .data = buffer,
            .length = totalSamples // number of 16-bit samples
        };

        // Push to queue — blocks if full (backpressure)
        if (xQueueSend(audioQueue, &chunk, pdMS_TO_TICKS(5000)) != pdPASS) {
            SerialManager::enqueueLine("[Voice] Queue full — dropping chunk");
            free(buffer);
        }
        // No delay — immediately begin next 5-second capture
    }
}

// ─────────────────────────────────────────────────────────────
// Task 2: Upload to Groq API (synchronous HTTPClient in task thread)
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::uploadTask(void *param) {
    AudioChunk chunk{};

    while (true) {
        // Wait for a complete audio chunk
        if (xQueueReceive(audioQueue, &chunk, portMAX_DELAY) != pdPASS) {
            continue;
        }

        const size_t audioBytes = chunk.length * sizeof(int16_t);
        const String boundary = "----ESP32Boundary7MA4YWxkTrZu0gW";

        const String headPart = "--" + boundary + "\r\n"
                                "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                                "Content-Type: audio/wav\r\n\r\n";

        const String tailPart = "\r\n--" + boundary + "\r\n"
                                "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                                + String(GROQ_MODEL) +
                                "\r\n--" + boundary + "--\r\n";

        uint8_t wavHeader[44];
        createWavHeader(wavHeader, audioBytes, SAMPLE_RATE, 1, 16);

        const size_t totalPayloadSize = headPart.length() + 44 + audioBytes + tailPart.length();

        auto *payload = static_cast<uint8_t *>(heap_caps_malloc(totalPayloadSize, MALLOC_CAP_SPIRAM));
        if (!payload) {
            payload = static_cast<uint8_t *>(malloc(totalPayloadSize));
        }

        if (!payload) {
            SerialManager::enqueueLine("[Voice] Failed to allocate upload buffer");
            free(chunk.data);
            continue;
        }

        size_t offset = 0;
        memcpy(payload + offset, headPart.c_str(), headPart.length());
        offset += headPart.length();

        memcpy(payload + offset, wavHeader, 44);
        offset += 44;

        memcpy(payload + offset, chunk.data, audioBytes);
        offset += audioBytes;

        memcpy(payload + offset, tailPart.c_str(), tailPart.length());
        offset += tailPart.length();

        // Free the raw audio buffer as soon as payload is assembled
        free(chunk.data);

        WiFiClientSecure client;
        client.setInsecure(); // Skip certificate verification for Groq API endpoint

        if (HTTPClient http; http.begin(client, GROQ_API_URL)) {
            http.setTimeout(30000); // 30 s max for API response
            http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
            http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
            http.addHeader("language", "en");

            if (const int httpCode = http.POST(payload, totalPayloadSize); httpCode == HTTP_CODE_OK) {
                const String responsePayload = http.getString();
                JsonDocument doc;
                const DeserializationError err = deserializeJson(doc, responsePayload);
                if (err) {
                    SerialManager::enqueueLine("[Voice] JSON parse error: " + String(err.c_str()));
                } else {
                    String transcript = doc["text"] | "";
                    transcript.trim();
                    if (transcript.isEmpty()) {
                        SerialManager::enqueueLine("[Voice] Empty transcript (silence)");
                    } else {
                        SerialManager::enqueueLine("[Voice] Transcript: \"" + transcript + "\"");
                        processTranscript(transcript);
                    }
                }
            } else {
                const String errResponse = http.getString();
                SerialManager::enqueueLine("[Voice] Groq API error: " + String(httpCode) + " - " + errResponse);
            }
            http.end();
        } else {
            SerialManager::enqueueLine("[Voice] Failed to begin HTTP request");
        }

        free(payload);
        vTaskDelay(pdMS_TO_TICKS(100)); // brief yield before next chunk
    }
}

// ─────────────────────────────────────────────────────────────
// Build WAV Header (44 bytes canonical PCM)
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::createWavHeader(
    uint8_t *header, const uint32_t dataSize, const uint32_t sampleRate,
    const uint16_t numChannels, const uint16_t bitsPerSample) {
    const uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const uint32_t chunkSize = 36 + dataSize;

    // RIFF Chunk Descriptor
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    header[4] = static_cast<uint8_t>(chunkSize & 0xFF);
    header[5] = static_cast<uint8_t>((chunkSize >> 8) & 0xFF);
    header[6] = static_cast<uint8_t>((chunkSize >> 16) & 0xFF);
    header[7] = static_cast<uint8_t>((chunkSize >> 24) & 0xFF);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';

    // "fmt " Sub-chunk
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    header[16] = 16;
    header[17] = 0;
    header[18] = 0;
    header[19] = 0; // Subchunk1Size (16 for PCM)
    header[20] = 1;
    header[21] = 0; // AudioFormat (1 = PCM)
    header[22] = static_cast<uint8_t>(numChannels & 0xFF);
    header[23] = static_cast<uint8_t>((numChannels >> 8) & 0xFF);
    header[24] = static_cast<uint8_t>(sampleRate & 0xFF);
    header[25] = static_cast<uint8_t>((sampleRate >> 8) & 0xFF);
    header[26] = static_cast<uint8_t>((sampleRate >> 16) & 0xFF);
    header[27] = static_cast<uint8_t>((sampleRate >> 24) & 0xFF);
    header[28] = static_cast<uint8_t>(byteRate & 0xFF);
    header[29] = static_cast<uint8_t>((byteRate >> 8) & 0xFF);
    header[30] = static_cast<uint8_t>((byteRate >> 16) & 0xFF);
    header[31] = static_cast<uint8_t>((byteRate >> 24) & 0xFF);
    header[32] = static_cast<uint8_t>(blockAlign & 0xFF);
    header[33] = static_cast<uint8_t>((blockAlign >> 8) & 0xFF);
    header[34] = static_cast<uint8_t>(bitsPerSample & 0xFF);
    header[35] = static_cast<uint8_t>((bitsPerSample >> 8) & 0xFF);

    // "data" Sub-chunk
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = static_cast<uint8_t>(dataSize & 0xFF);
    header[41] = static_cast<uint8_t>((dataSize >> 8) & 0xFF);
    header[42] = static_cast<uint8_t>((dataSize >> 16) & 0xFF);
    header[43] = static_cast<uint8_t>((dataSize >> 24) & 0xFF);
}

// ─────────────────────────────────────────────────────────────
// Keyword matching — replace with your own event triggers
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::registerKeyword(
    std::vector<String> keywords, std::function<void(String)> callback) {
    keywordSets.push_back({.keywords = std::move(keywords), .callback = std::move(callback)});
}

void VoiceKeywordModule::setOnTranscriptionReceived(std::function<void(String)> callback) {
    onTranscriptionReceived = std::move(callback);
}

void VoiceKeywordModule::processTranscript(const String &transcript) {
    if (onTranscriptionReceived) onTranscriptionReceived(transcript);
    String lower = transcript;
    lower.toLowerCase();

    for (auto &[keywords, callback]: keywordSets) {
        for (auto &keyword : keywords) {

            String kwLower = keyword;
            kwLower.toLowerCase();
            if (lower.indexOf(kwLower) >= 0) {
                SerialManager::enqueueLine("[Voice] Keyword matched: " +
                                           keyword);
                if (callback) callback(keyword);
                break;
            }
        }

    }
}
