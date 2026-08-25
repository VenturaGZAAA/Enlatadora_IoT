#include <WiFi.h>
#include <SPI.h>
#include <secrets.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>

#include "MqttServer.h"

static TaskHandle_t serverTaskHandle;

[[noreturn]] static void serverTask() {
  MqttServer::setup();
  DashboardServer::setup();
  while (true) {
    MqttServer::loop();
    DashboardServer::loop();
    vTaskDelay(pdMS_TO_TICKS(0.1));
  }
}


void setup()
{
  Serial.begin(115200);
  // --- Connect to Wi-Fi ---
  Serial.print("📶 Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📡 IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n❌ WiFi connection failed!");
    return;
  }

  delay(100);
  xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(serverTask),"servers",10240,nullptr,1,&serverTaskHandle,0);
}


void loop()
{
  delay(500);
}