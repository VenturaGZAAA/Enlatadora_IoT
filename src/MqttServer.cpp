//
// Created by urzu-7 on 8/25/26.
//
#include <Arduino.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>

#include "MqttServer.h"


WiFiServer MqttServer::tcp_server(1883);

WiFiServer MqttServer::websocket_underlying_server(8080);

PicoWebsocket::Server<WiFiServer> MqttServer::websocket_server(websocket_underlying_server);

PicoMQTT::Server MqttServer::mqtt(tcp_server, websocket_server);

long MqttServer::lastTime = 0;
bool MqttServer::led = false;

void MqttServer::setup() {
    mqtt.begin();

    mqtt.subscribe("home/test/led", [](const char *payload) {
        Serial.println("Togglings");
    });
    mqtt.subscribe("#", [](const char *payload) {
        Serial.printf("Received message: %s\n\r", payload);
    });
}

void MqttServer::loop() {
    mqtt.loop();
    if (millis() - lastTime > 1000) {
        lastTime = millis();
        const String payload = "Hello! " + String(random(1000));
        mqtt.publish("home/test/read", payload);
    }
}
