#include "WebSeverManager.h"
#include "drivers/SerialMonitor.h"
#include "drivers/SdCardManager.h"
#include "config/build_flags.h"
#include "WifiManager.h"

#include <utility>

// Static definitions
AsyncWebServer WebSeverManager::server(80);

// ------------------------------------------------------------------
// Setup

void WebSeverManager::setup() {
    SerialMonitor::enqueueLine("\n=== 🚀 ESP32-S3 Web Server ===");

    if (SdCardManager::isMounted()) {
        // Configure routes
        server.on("/favicon.ico", HTTP_GET, handleFavicon);
        server.onNotFound(handleFileRequest);
    }

    server.on("/config/wifi/set", HTTP_POST, [](AsyncWebServerRequest *request) {
                  if (request->_tempObject == nullptr) {
                      request->send(200, "application/json", R"({"status":"success"})");
                  } else {
                      const auto *errorMsg = static_cast<String *>(request->_tempObject);
                      const String responseJson = R"({"status":"error","message":")" + *errorMsg + R"("})";
                      delete errorMsg;
                      request->_tempObject = nullptr;
                      request->send(400, "application/json", responseJson);
                  }
              }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t length, size_t, size_t) {
                  JsonDocument doc;
                  const DeserializationError error = deserializeJson(doc, data, length);
                  if (error) {
                      SerialMonitor::enqueue("Error parsing JSON for Wifi config on HTTP: ");
                      SerialMonitor::enqueueLine(error.c_str());
                      request->_tempObject = new String(error.c_str());
                      return;
                  }
                  WifiManager::wifiConfigJsonCallback(doc);
                  request->_tempObject = nullptr;
              });

    // Start server
    server.begin();
    SerialMonitor::enqueueLine("🌐 HTTP server started on port 80");
    if (SdCardManager::isMounted()) {
        SerialMonitor::enqueueLine("📍 Open http://" + WifiManager::getServerIP().toString() + " in your browser");
    }
    WifiManager::addService("http", "tcp", 80);
}

bool WebSeverManager::isMounted() {
    return SdCardManager::isMounted();
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
            if (const String brPath = path + ".br"; SdCardManager::exists(brPath)) {
                filePath = brPath;
                contentEncoding = "br";
            }
        }

        if (contentEncoding == nullptr && strcmp(encoding, "gzip") == 0) {
            if (const String gzPath = path + ".gz"; SdCardManager::exists(gzPath)) {
                filePath = gzPath;
                contentEncoding = "gzip";
            }
        }
    }


    auto *file = SdCardManager::openFile(filePath, O_READ);
    if (!file) {
        request->send(404, "text/plain", "File not found");
        SerialMonitor::enqueueLine("File not found");
        return;
    }

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        getContentType(path), // MIME type based on original extension
        [file](uint8_t *buffer, const size_t maxLen, size_t) -> size_t {
            if (const SdCardManager::LockGuard lock(pdMS_TO_TICKS(1000)); !lock) {
                return 0;
            }
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
