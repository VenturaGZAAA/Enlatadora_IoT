#include "DashboardServer.h"
#include "SerialManager.h"
#include <SPI.h>

#include "WifiManager.h"

// --- SD Card Pins for ESP32-S3-DevKitM-1 ---

#define SD_CS   47
#define SD_MOSI 21
#define SD_SCK  20
#define SD_MISO 19

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

   
    if (const SdSpiConfig cfg(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI); !sd.begin(cfg)) {
        SerialManager::enqueueLine(" ❌ Card Mount Failed!");
        SPI.end();
    }
    else {
        mounted = true;
        SerialManager::enqueueLine("✅ SD card mounted");
        // Configure routes
        server.on("/favicon.ico", HTTP_GET, handleFavicon);
        server.onNotFound(handleFileRequest);
    }

    server.on("/config/wifi/set",HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->_tempObject == nullptr) {
            request->send(200, "application/json", R"({"status":"success"})");
        }
        else {
            const String errorMsg = *static_cast<String *>(request->_tempObject);
            // delete static_cast<String *>(request->_tempObject);

            const String responseJson = R"({"status":"error","message":")" + errorMsg + R"("})";
            request->send(400, "application/json", responseJson);
        }

    },nullptr,[](AsyncWebServerRequest *request,uint8_t *data, size_t length, size_t idx,size_t total) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, data, length);
        if (error) {
            SerialManager::enqueue("Error parsing JSON for Wifi config on HTTP: ");
            SerialManager::enqueueLine(error.c_str());
            request->_tempObject = new String(error.c_str());
            return;
        }
        WifiManager::wifiConfigJsonCallback(doc);
        request->_tempObject = nullptr;
    });

    // Start server
    server.begin();
    SerialManager::enqueueLine("🌐 HTTP server started on port 80");
    if (mounted) {
        SerialManager::enqueueLine("📍 Open http://" + WifiManager::getServerIP().toString() + " in your browser");
    }
    WifiManager::addService("http","tcp",80);
}

bool DashboardServer::isMounted() {
    return mounted;
}

void DashboardServer::handleFileRequest(AsyncWebServerRequest *request) {
    // 1. Determine the base file path (without compression suffix)
    String path = request->url();
    SerialManager::enqueueLine("Handling:\t" + path);
    if (path == "/" || path == "") {
        path = "/web/index.html";
    } else {
        path = "/web" + path;
    }

    const char* encoding = getEncoding(request);
    const bool hasEncoding = (encoding != nullptr);


    String filePath = path;
    const char* contentEncoding = nullptr;

    if (hasEncoding) {
        if (strcmp(encoding, "br") == 0) {
            const String brPath = path + ".br";
            if (sd.exists(brPath.c_str())) {
                filePath = brPath;
                contentEncoding = "br";
            }
        }

        if (contentEncoding == nullptr && strcmp(encoding, "gzip") == 0) {
            const String gzPath = path + ".gz";
            if (sd.exists(gzPath.c_str())) {
                filePath = gzPath;
                contentEncoding = "gzip";
            }
        }
    }


    auto *file = new SdFile();
    if (!file->open(filePath.c_str(), O_READ)) {
        delete file;
        request->send(404, "text/plain", "File not found");
        SerialManager::enqueueLine("File not found");
        return;
    }

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        getContentType(path),   // MIME type based on original extension
        [file](uint8_t *buffer, const size_t maxLen, size_t) -> size_t {
            const size_t bytesRead = file->read(buffer, maxLen);
            if (bytesRead == 0) {
                file->close();
                delete file;
            }
            return bytesRead;
        }
    );

    // 6. Add headers
    response->addHeader("Content-Length", String(file->fileSize()));
    if (contentEncoding != nullptr) {
        response->addHeader("Content-Encoding", contentEncoding);
        // Optionally add cache control for compressed assets
        response->addHeader("Cache-Control", "public, max-age=31536000");
    }
    // 7. Send the response
    request->send(response);

    SerialManager::enqueueLine("response sent");

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
