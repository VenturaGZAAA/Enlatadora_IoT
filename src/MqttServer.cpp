//
// Created by urzu-7 on 8/25/26.
//
#include <PicoMQTT.h>
#include <PicoWebsocket.h>

#include "MqttServer.h"


WiFiServer MqttServer::tcp_server(1883);

WiFiServer MqttServer::websocket_underlying_server(8883);

PicoWebsocket::Server<WiFiServer> MqttServer::websocket_server(websocket_underlying_server);

 PicoMQTT::Server MqttServer::mqtt(tcp_server, websocket_server); // NOTE: this constructor can take any number of server parameters

void MqttServer::setup() {
 mqtt.begin();
}
void MqttServer::loop() {
 mqtt.loop();
}