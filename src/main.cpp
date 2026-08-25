#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SdFat.h>
#include  <secrets.h>

// --- SD Card Pins for ESP32-S3-DevKitM-1 ---
#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12

// --- SdFat Configuration ---
SdFat sd;
SdFile file;

// --- Web Server ---
WebServer server(80);

// Helper function to get content type
String getContentType(const String& filename) {
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".svg")) return "image/svg+xml";
  if (filename.endsWith(".txt")) return "text/plain";
  if (filename.endsWith(".xml")) return "text/xml";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".zip")) return "application/zip";
  if (filename.endsWith(".wasm")) return "application/wasm";
  if (filename.endsWith(".webmanifest")) return "application/manifest+json";
  return "application/octet-stream";
}

// Manually stream file from SdFat to web client
void streamFile(SdFile& streamed_file, const String& contentType) {
  // Get file size
  const uint32_t fileSize = streamed_file.fileSize();
  
  // Send headers
  server.sendHeader("Content-Type", contentType);
  server.sendHeader("Content-Length", String(fileSize));
  server.sendHeader("Connection", "close");
  server.send(200, contentType, "");
  
  // Stream file in chunks
  constexpr uint16_t CHUNK_SIZE = 1024; // 1KB chunks
  uint8_t buffer[CHUNK_SIZE];
  
  while (streamed_file.available()) {
    const int bytesRead = streamed_file.read(buffer, CHUNK_SIZE);
    if (bytesRead > 0) {
      server.sendContent(reinterpret_cast<const char *>(buffer), bytesRead);
    }
  }
}

// Serve files from SD card
void handleFileRequest() {
  String path = server.uri();
  
  // Default to index.html for root
  if (path == "/" || path == "") {
    path = "/web/index.html";
  } else {
    path = "/web" + path;  // All files are in /web folder
  }
  
  Serial.print("📂 Requesting: ");
  Serial.println(path);
  
  // Check if file exists
  if (!sd.exists(path.c_str())) {
    Serial.println("❌ File not found");
    server.send(404, "text/plain", "404: File Not Found");
    return;
  }
  
  // Open the file
  if (!file.open(path.c_str(), O_READ)) {
    Serial.println("❌ Failed to open file");
    server.send(500, "text/plain", "500: Failed to open file");
    return;
  }
  
  // Get content type and serve file
  String contentType = getContentType(path);
  Serial.print("📤 Serving: ");
  Serial.print(path);
  Serial.print(" (");
  Serial.print(contentType);
  Serial.println(")");
  
  streamFile(file, contentType);
  file.close();
}

// Handle favicon.ico requests (optional, to avoid 404s)
void handleFavicon() {
  server.send(204, "text/plain", ""); // No content
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== 🚀 ESP32-S3 Web Server ===");
  
  // --- Initialize SD Card with SdFat ---
  Serial.print("📀 Initializing SD card...");
  
  // Configure SPI pins for SdFat
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  // Initialize SdFat with slower speed for stability
  if (!sd.begin(SD_CS, SD_SCK_MHZ(4))) {
    Serial.println(" ❌ Card Mount Failed!");
    Serial.print("⚠️ Error code: ");
    Serial.println(sd.card()->errorCode());
    return;
  }
  
  // Check card type
  uint8_t cardType = sd.card()->type();
  Serial.print(" ✅ Card Type: ");
  if (cardType == SD_CARD_TYPE_SD1) Serial.println("SD1");
  else if (cardType == SD_CARD_TYPE_SD2) Serial.println("SD2");
  else if (cardType == SD_CARD_TYPE_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");
  
  // Print card size
  const uint64_t cardSize = sd.card()->sectorCount() * 512ULL;
  Serial.print("💾 Card Size: ");
  if (cardSize < 1024 * 1024) {
    Serial.print(cardSize / 1024);
    Serial.println(" KB");
  } else if (cardSize < 1024 * 1024 * 1024) {
    Serial.print(cardSize / (1024 * 1024));
    Serial.println(" MB");
  } else {
    Serial.print(cardSize / (1024 * 1024 * 1024));
    Serial.println(" GB");
  }
  
  // List contents of /web folder to verify
  Serial.println("\n📁 Files in /web folder:");
  sd.ls("/web", LS_R | LS_SIZE);
  Serial.println();
  
  // --- Connect to Wi-Fi ---
  Serial.print("📶 Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFiClass::status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFiClass::status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📡 IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi connection failed!");
    return;
  }
  
  // --- Configure Web Server ---
  server.on("/favicon.ico", handleFavicon);
  server.onNotFound(handleFileRequest);
  
  server.begin();
  Serial.println("🌐 HTTP server started on port 80");
  Serial.println("📍 Open http://" + WiFi.localIP().toString() + " in your browser");
  Serial.println("\n========================================\n");
}

void loop() {
  server.handleClient();
  delay(1); // Small delay to prevent watchdog issues
}