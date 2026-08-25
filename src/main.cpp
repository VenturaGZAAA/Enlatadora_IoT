#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include  <secrets.h>
#include <DashboardServer.h>


void setup() {
  Serial.begin(115200);
  // --- Connect to Wi-Fi ---
  Serial.print("📶 Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📡 IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi connection failed!");
    return;
  }

  delay(100);
  DashboardServer::setup();
}

void loop() {
  DashboardServer::loop();
  delay(1); // Small delay to prevent watchdog issues
}