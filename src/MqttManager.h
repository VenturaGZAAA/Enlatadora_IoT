//
// Created by urzu-7 on 8/25/26.
//

#ifndef ENLATADORA_IOT_MQTTSERVER_H
#define ENLATADORA_IOT_MQTTSERVER_H

#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <MillisData.h>
#include <queue>
#include <vector>
#include <functional>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define SERVER_STACK_SIZE (8192 * 3)

class PeriodicPublish {
    MillisData millisData;
    std::function<String()> callback;
    String topic;

public:
    PeriodicPublish(String topic, const unsigned long delay, std::function<String()> callback) : millisData(delay),
        topic(std::move(topic)) {
        this->callback = std::move(callback);
    }

    [[nodiscard]] bool isTime() const {
        return millisData.isTime();
    }

    void resetTime() {
        millisData.reset();
    }

    [[nodiscard]] String getTopic() const {
        return topic;
    };

    [[nodiscard]] String getPayload() const {
        return callback();
    };
};

struct MqttMessage {
    String topic;
    String payload;
    uint8_t qos = 0;
    bool retain = false;
};

class MqttManager {
    static WiFiServer tcp_server;
    static WiFiServer websocket_underlying_server;
    static PicoWebsocket::Server<WiFiServer> websocket_server;
    static PicoMQTT::Server mqtt;
    static inline TaskHandle_t mqttTaskHandle = nullptr;
    static inline SemaphoreHandle_t publishMutex = nullptr;
    static inline SemaphoreHandle_t periodicMutex = nullptr;
    static inline std::queue<MqttMessage> publishQueue;
    static inline std::vector<PeriodicPublish> periodicPublishes;

    [[noreturn]] static void mqttManagerLoop();
    static void processPublishQueue();

public:
    MqttManager() = delete;

    static void setup();
    static void begin();

    static void publish(const char *topic, const String &payload, uint8_t qos = 0, bool retain = false);
    static void publish(const String &topic, const String &payload, uint8_t qos = 0, bool retain = false);
    static void publish(const char *topic, const char *payload, uint8_t qos = 0, bool retain = false);
    static void publish(const String &topic, const char *payload, uint8_t qos = 0, bool retain = false);

    static void registerCallback(const char *topic, const std::function<void(char *)> &callback);
    static void registerPeriodicPublish(const String &topic, unsigned long delay,
                                        const std::function<String()> &callback);
};

#endif //ENLATADORA_IOT_MQTTSERVER_H
