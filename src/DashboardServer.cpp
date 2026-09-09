#include "DashboardServer.h"
#include "SerialManager.h"
#include <SPI.h>

// --- SD Card Pins for ESP32-S3-DevKitM-1 ---
#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12

// Static definitions
AsyncWebServer DashboardServer::server(80);
SdFat DashboardServer::sd;

// ------------------------------------------------------------------
// Setup
void DashboardServer::setup() {
    SerialManager::enqueueLine("\n=== 🚀 ESP32-S3 Web Server ===");

    // Initialize SD card
    SerialManager::enqueueLine("📀 Initializing SD card...");
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!sd.begin(SD_CS, SD_SCK_MHZ(4))) {
        SerialManager::enqueueLine(" ❌ Card Mount Failed!");
        return;
    }
    SerialManager::enqueueLine("✅ SD card mounted");

    // Configure routes
    server.on("/favicon.ico", HTTP_GET, handleFavicon);
    server.on("/heello", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", (uint8_t *) ("helloo"), strlen_P("helloo"));
    });
    server.onNotFound(handleFileRequest);

    // Start server
    server.begin();
    SerialManager::enqueueLine("🌐 HTTP server started on port 80");
    SerialManager::enqueueLine("📍 Open http://" + WiFi.localIP().toString() + " in your browser");
    SerialManager::enqueueLine("\n========================================\n");
}

// ------------------------------------------------------------------
// Handlers
void DashboardServer::handleFileRequest(AsyncWebServerRequest *request) {
    String path = request->url();
    if (path == "/" || path == "") {
        path = "/web/index.html";
    } else {
        path = "/web" + path;
    }
    // Open the file
    auto *file = new SdFile();
    if (!file->open(path.c_str(), O_READ)) {
        delete file;
        request->send(404, "text/plain", "File not found");
        return;
    }

    // Try compressed versions based on Accept-Encoding
    const char *encoding = getEncoding(request);
    const bool hasEncoding = (encoding != nullptr);

    // Create chunked response with a lambda callback
    AsyncWebServerResponse *sisisi = request->beginChunkedResponse(
        getContentType(path),
        [file](uint8_t *buffer, const size_t maxLen, size_t) -> size_t {
            const size_t bytesRead = file->read(buffer, maxLen);
            if (bytesRead == 0) {
                file->close();
                delete file;
            }
            return bytesRead;
        }
    );

    // Add content length if known
    sisisi->addHeader("Content-Length", String(file->fileSize()));
    request->send(sisisi);
}

void DashboardServer::handleFavicon(AsyncWebServerRequest *request) {
    request->send(204); // No content
}

// ------------------------------------------------------------------
// Helpers
String DashboardServer::getContentType(const String &filename) {
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

const char *DashboardServer::getEncoding(const AsyncWebServerRequest *request) {
    auto *header = request->getHeader("Accept-Encoding");
    if (!header) return nullptr;

    const String value = header->value();
    if (value.indexOf("br") != -1) return "br";
    if (value.indexOf("gzip") != -1) return "gzip";
    return nullptr;
}
