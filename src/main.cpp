#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <secrets.h>
#include <DashboardServer.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>

// regular tcp server on port 1883
static WiFiServer tcp_server(1883);

// websocket server on tcp port 80
static WiFiServer websocket_underlying_server(8883);

static PicoWebsocket::Server websocket_server(websocket_underlying_server);

static PicoMQTT::Server mqtt(tcp_server, websocket_server); // NOTE: this constructor can take any number of server parameters

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

  mqtt.subscribe("#", [](const char *topic, const char *payload)
                 {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        Serial.printf("Received message in topic '%s': %s\n\r", topic, payload); });

  delay(100);
  mqtt.begin();
  DashboardServer::setup();
}

unsigned long last_publish_time = 0;
int greeting_number = 1;

void loop()
{
  DashboardServer::loop();
  mqtt.loop();
  if (millis() - last_publish_time >= 3000)
  {
    // We're publishing to a topic, which we're subscribed too, but these message will *not* be delivered locally.
    const String topic = "picomqtt/esp-32";
    const String message = "Hello #" + String(greeting_number++);
    Serial.printf("Publishing message in topic '%s': %s\n\r", topic.c_str(), message.c_str());
    mqtt.publish(topic, message);
    last_publish_time = millis();
  }
  // delay(1); // Small delay to prevent watchdog issues
}