//
// Created by urzu-7 on 10/11/26.
//

#include "VoiceKeywordModule.h"
#include <secrets.h>
#include <utility>

#include "SerialManager.h"

QueueHandle_t VoiceKeywordModule::audioQueue = nullptr;
TaskHandle_t VoiceKeywordModule::captureTaskHandle = nullptr;
TaskHandle_t VoiceKeywordModule::uploadTaskHandle = nullptr;
AsyncHTTPRequest VoiceKeywordModule::httpRequest;
std::vector<VoiceKeywordModule::KeywordEntry> VoiceKeywordModule::keywords;

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
    audioQueue = xQueueCreate(AUDIO_QUEUE_LENGTH, AUDIO_QUEUE_ITEM_SIZE);
    if (!audioQueue) {
        SerialManager::enqueueLine("[Voice] Failed to create audio queue");
        return;
    }
    initI2S();
    SerialManager::enqueueLine("[Voice] I2S microphone initialised");
}

// ─────────────────────────────────────────────────────────────
// Start tasks — call from setup() after WifiManager::setup()
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::begin() {
    xTaskCreatePinnedToCore(
        captureTask, "VoiceCapture",
        8192, nullptr, 5, &captureTaskHandle, 1 // Core 1 — audio capture
    );
    xTaskCreatePinnedToCore(
        uploadTask, "VoiceUpload",
        16384, nullptr, 4, &uploadTaskHandle, 0 // Core 0 — network upload
    );
}

// ─────────────────────────────────────────────────────────────
// Task 1: Capture 6-second audio chunks
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::captureTask(void *param) {
    size_t bytesRead = 0;

    while (true) {
        // Allocate buffer for 6 seconds of 16-bit mono audio
        auto *buffer = static_cast<int16_t *>(heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM));
        if (!buffer) {
            buffer = static_cast<int16_t *>(malloc(BUFFER_SIZE)); // fallback to internal RAM
        }
        if (!buffer) {
            SerialManager::enqueueLine("[Voice] Buffer allocation failed — retrying");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        size_t totalRead = 0;
        constexpr size_t targetBytes = BUFFER_SIZE;

        // Read continuously for 6 seconds
        while (totalRead < targetBytes) {
            const esp_err_t err = i2s_read(
                I2S_PORT,
                reinterpret_cast<uint8_t *>(buffer) + totalRead,
                targetBytes - totalRead,
                &bytesRead,
                portMAX_DELAY
            );
            if (err != ESP_OK) {
                SerialManager::enqueueLine("[Voice] I2S read error: " + std::to_string(err));
                break;
            }
            totalRead += bytesRead;
        }

        AudioChunk chunk = {
            .data = buffer,
            .length = totalRead / sizeof(int16_t) // samples, not bytes
        };

        // Push to queue — blocks if full (backpressure)
        if (xQueueSend(audioQueue, &chunk, pdMS_TO_TICKS(5000)) != pdPASS) {
            SerialManager::enqueueLine("[Voice] Queue full — dropping chunk");
            free(buffer);
        }
        // No delay — immediately begin next 6-second capture
    }
}

// ─────────────────────────────────────────────────────────────
// Task 2: Upload to Groq API (non-blocking async HTTP)
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::uploadTask(void *param) {
    AudioChunk chunk{};

    while (true) {
        // Wait for a complete audio chunk
        if (xQueueReceive(audioQueue, &chunk, portMAX_DELAY) != pdPASS) {
            continue;
        }

        // ── Build multipart/form-data body ──
        String boundary = "----ESP32Boundary7MA4YWxkTrZu0gW";
        String body = buildMultipartBody(reinterpret_cast<uint8_t *>(chunk.data),
                                         chunk.length * sizeof(int16_t));

        // Free the audio buffer as soon as the body is built
        free(chunk.data);

        // ── Configure async HTTP request ──
        httpRequest.setDebug(false);
        httpRequest.setTimeout(30000); // 30 s max for API response

        if (!httpRequest.open("POST", GROQ_API_URL)) {
            SerialManager::enqueueLine("[Voice] Failed to open HTTP request");
            continue;
        }

        httpRequest.setReqHeader("Authorization", (String("Bearer ") + GROQ_API_KEY).c_str());
        httpRequest.setReqHeader("Content-Type",
                                 ("multipart/form-data; boundary=" + boundary).c_str());

        // ── Send asynchronously with callback ──
        // The callback fires when the full response arrives.
        // This call returns immediately — no blocking.
        httpRequest.send(reinterpret_cast<const uint8_t *>(body.c_str()), body.length());

        // Wait for the async request to complete via callback
        // (a semaphore or event group works here; simplified polling shown)
        // In production, use an EventGroup bit set inside onTranscriptionResponse.
        vTaskDelay(pdMS_TO_TICKS(100)); // brief yield before next chunk
    }
}

// ─────────────────────────────────────────────────────────────
// Async response callback — called when Groq replies
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::onTranscriptionResponse(
    AsyncHTTPRequest *request, const int readyState) {
    if (readyState != readyStateDone) return;

    const int statusCode = request->responseHTTPcode();
    String payload = request->responseText();

    if (statusCode != 200) {
        const String msg = "[Voice] Groq API error: " + String(statusCode) + " - " + payload;
        SerialManager::enqueueLine(msg);
        return;
    }

    // ── Parse JSON response ──
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        SerialManager::enqueueLine("[Voice] JSON parse error:" + String(err.c_str()));
        return;
    }

    String transcript = doc["text"] | "";
    transcript.trim();

    if (transcript.isEmpty()) {
        SerialManager::enqueueLine("[Voice] Empty transcript (silence)");
        return;
    }

    SerialManager::enqueueLine("[Voice] Transcript: " + transcript);
    processTranscript(transcript);
}

// ─────────────────────────────────────────────────────────────
// Build multipart/form-data body for Groq Whisper endpoint
// ─────────────────────────────────────────────────────────────
String VoiceKeywordModule::buildMultipartBody(
    const uint8_t *audioData, const size_t audioLen) {
    const String boundary = "----ESP32Boundary7MA4YWxkTrZu0gW";
    String body;

    // Part 1: file
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; "
            "filename=\"audio.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";

    // Minimal WAV header (44 bytes) so Groq recognises the format
    uint8_t wavHeader[44] = {
        'R', 'I', 'F', 'F', // ChunkID
        0, 0, 0, 0, // ChunkSize (patched below)
        'W', 'A', 'V', 'E', // Format
        'f', 'm', 't', ' ', // Subchunk1ID
        16, 0, 0, 0, // Subchunk1Size (16 for PCM)
        1, 0, // AudioFormat (1 = PCM)
        1, 0, // NumChannels (1 = mono)
        0x80, 0x3E, 0, 0, // SampleRate (16000 = 0x3E80)
        0x00, 0x7D, 0, 0, // ByteRate (16000 * 1 * 2 = 32000 = 0x7D00)
        2, 0, // BlockAlign (1 * 2 = 2)
        16, 0, // BitsPerSample (16)
        'd', 'a', 't', 'a', // Subchunk2ID
        0, 0, 0, 0 // Subchunk2Size (patched below)
    };

    const uint32_t dataSize = audioLen;
    const uint32_t chunkSize = 36 + dataSize;
    memcpy(&wavHeader[4], &chunkSize, 4);
    memcpy(&wavHeader[40], &dataSize, 4);

    // Append WAV header + raw PCM
    body.concat(reinterpret_cast<const char *>(wavHeader), 44);
    body.concat(reinterpret_cast<const char *>(audioData), audioLen);

    // Part 2: model
    body += "\r\n--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    body += GROQ_MODEL;
    body += "\r\n--" + boundary + "--\r\n";

    return body;
}

// ─────────────────────────────────────────────────────────────
// Keyword matching — replace with your own event triggers
// ─────────────────────────────────────────────────────────────
void VoiceKeywordModule::registerKeyword(
    const String &keyword, std::function<void()> callback) {
    keywords.push_back({.keyword = keyword, .callback = std::move(callback)});
}

void VoiceKeywordModule::processTranscript(const String &transcript) {
    String lower = transcript;
    lower.toLowerCase();

    for (auto &entry: keywords) {
        String kwLower = entry.keyword;
        kwLower.toLowerCase();
        if (lower.indexOf(kwLower) >= 0) {
            SerialManager::enqueueLine("[Voice] Keyword matched: " +
                                       entry.keyword);
            if (entry.callback) entry.callback();
        }
    }
}
