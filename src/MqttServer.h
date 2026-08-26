//
// Created by urzu-7 on 8/25/26.
//

#ifndef ENLATADORA_IOT_MQTTSERVER_H
#define ENLATADORA_IOT_MQTTSERVER_H

#include <PicoMQTT.h>
#include <PicoWebsocket.h>


class MqttServer {
    static WiFiServer tcp_server;
    static WiFiServer websocket_underlying_server;
    static PicoWebsocket::Server<WiFiServer> websocket_server;
    static PicoMQTT::Server mqtt;
    static long lastTime;
    static bool led;

public:
    MqttServer() = delete;

    static void setup();

    static void loop();
};


#endif //ENLATADORA_IOT_MQTTSERVER_H
