//
// Created by urzu-7 on 8/25/26.
//

#ifndef ENLATADORA_IOT_MQTTSERVER_H
#define ENLATADORA_IOT_MQTTSERVER_H

#include <PicoMQTT.h>
#include <PicoWebsocket.h>

#define SERVER_STACK_SIZE (8192 * 2)

class MqttManager {
    static WiFiServer tcp_server;
    static WiFiServer websocket_underlying_server;
    static PicoWebsocket::Server<WiFiServer> websocket_server;
    static PicoMQTT::Server mqtt;
    static inline TaskHandle_t mqttTaskHandle;
    [[noreturn]] static void mqttManagerLoop();


public:
    MqttManager() = delete;

    static void setup();
    static void registerCallback(const char *topic, const std::function<void(char *)>& callback);
    static void publish(const char *topic, const String& payload,uint8_t qos = 0);

};


#endif //ENLATADORA_IOT_MQTTSERVER_H
