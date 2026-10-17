#include "WebSeverManager.h"
#include "SerialManager.h"
#include "SdCardManager.h"
#include <SPI.h>
#include <build_flags.h>

#include <utility>
#include "WifiManager.h"

// Static definitions
AsyncWebServer WebSeverManager::server(80);

// ------------------------------------------------------------------
// Setup

void WebSeverManager::setup() {
    SerialManager::enqueueLine("\n=== 🚀 ESP32-S3 Web Server ===");

    // Initialize SD card
    SerialManager::enqueueLine("📀 Initializing SD card...");
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);


    if (const SdSpiConfig cfg(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI); !sd.begin(cfg)) {
        SerialManager::enqueueLine(" ❌ Card Mount Failed!");
        SPI.end();
    } else {
        mounted = true;
        SerialManager::enqueueLine("✅ SD card mounted");
        // Configure routes
        server.on("/favicon.ico", HTTP_GET, handleFavicon);
        server.onNotFound(handleFileRequest);
    }

    server.on("/config/wifi/set", HTTP_POST, [](AsyncWebServerRequest *request) {
                  if (request->_tempObject == nullptr) {
                      request->send(200, "application/json", R"({"status":"success"})");
                  } else {
                      auto *errorMsg = static_cast<String *>(request->_tempObject);
                      const String responseJson = R"({"status":"error","message":")" + *errorMsg + R"("})";
                      delete errorMsg;
                      request->_tempObject = nullptr;
                      request->send(400, "application/json", responseJson);
                  }
              }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t length, size_t, size_t) {
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
    WifiManager::addService("http", "tcp", 80);
}

bool WebSeverManager::isMounted() {
    return mounted;
}

void WebSeverManager::setOnRootRequested(std::function<void()> callback) {
    onRootRequested = std::move(callback);
}

void WebSeverManager::handleFileRequest(AsyncWebServerRequest *request) {
    // 1. Determine the base file path (without compression suffix)
    String path = request->url();
    if (path == "/" || path == "") {
        path = webPath + "/index.html";
        if (onRootRequested) {
            onRootRequested();
        }
    } else {
        path = webPath+ path;
    }

    const char *encoding = getEncoding(request);
    const bool hasEncoding = (encoding != nullptr);


    String filePath = path;
    const char *contentEncoding = nullptr;

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
        getContentType(path), // MIME type based on original extension
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
#ifndef DISABLE_DASHBOARD_CACHE
        response->addHeader("Cache-Control", "public, max-age=31536000");
#endif
    }
    // 7. Send the response
    request->send(response);

}

void WebSeverManager::handleFavicon(AsyncWebServerRequest *request) {
    request->send(204); // No content
}

// ------------------------------------------------------------------
// Helpers
String WebSeverManager::getContentType(const String &filename) {
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

const char *WebSeverManager::getEncoding(const AsyncWebServerRequest *request) {
    auto *header = request->getHeader("Accept-Encoding");
    if (!header) return nullptr;

    const String value = header->value();
    if (value.indexOf("br") != -1) return "br";
    if (value.indexOf("gzip") != -1) return "gzip";
    return nullptr;
}
