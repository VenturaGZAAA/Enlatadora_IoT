#include "DashboardServer.h"
#include "SerialManager.h"
#include <SPI.h>
#include <algorithm>   // for std::min

// --- SD Card Pins for ESP32-S3-DevKitM-1 ---
#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12

// Static definitions
AsyncWebServer DashboardServer::server(80);
SdFat DashboardServer::sd;
// We no longer use a global SdFile – each request gets its own.

namespace {
    // ------------------------------------------------------------------
    // Custom response that streams from an SdFat file.
    // Takes ownership of the SdFile pointer.
    class SdFileResponse : public AsyncWebServerResponse {
    public:
        SdFileResponse(SdFile *file, const String &contentType, const size_t contentLength = 0,
                       const String &contentEncoding = "")
            : _file(file), _contentLength(contentLength),
              _contentEncoding(contentEncoding), _sent(0), _chunkSize(512) {
            _code = 200;
            _contentType = contentType;

            // Add standard headers using addHeader()
            addHeader("Content-Type", contentType);
            if (contentLength > 0) {
                addHeader("Content-Length", String(contentLength));
            } else {
                addHeader("Transfer-Encoding", "chunked");
            }
            if (!contentEncoding.isEmpty()) {
                addHeader("Content-Encoding", contentEncoding);
                addHeader("Cache-Control", "public, max-age=31536000");
            }
            addHeader("Connection", "close");
        }

        ~SdFileResponse() override {
            if (_file) {
                _file->close();
                delete _file;
            }
        }

        [[nodiscard]] bool _sourceValid() const override {
            return _file != nullptr && _file->isOpen();
        }

        size_t _fillBuffer(uint8_t *data, const size_t maxLen) {
            if (!_file || !_file->isOpen() || !_file->available()) return 0;

            // Read in chunks, respecting content length if known
            size_t toRead = std::min(maxLen, _chunkSize);
            if (_contentLength > 0) {
                if (const size_t remaining = _contentLength - _sent; toRead > remaining) toRead = remaining;
            }

            if (const int bytesRead = _file->read(data, toRead); bytesRead > 0) {
                _sent += bytesRead;
                return bytesRead;
            }
            return 0;
        }

        size_t _ack(AsyncWebServerRequest *request, const size_t len, uint32_t time) override {
            // Optional: feed watchdog or yield here if needed
            return len;
        }

    private:
        SdFile *_file;
        size_t _contentLength;
        String _contentEncoding;
        size_t _sent;
        const size_t _chunkSize;
    };
}

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

    SerialManager::enqueueLine("Path: " + path);

    // Try compressed versions based on Accept-Encoding
    const char* encoding = getEncoding(request);
    const bool hasEncoding = (encoding != nullptr);


    // --- Brotli ---
    String compressedPath = path + ".br";
    if (hasEncoding && strcmp(encoding, "br") == 0 && sd.exists(compressedPath.c_str())) {
        SerialManager::enqueueLine("Serving brotli: " + compressedPath);
        auto *filePtr = new SdFile();
        if (!filePtr->open(compressedPath.c_str(), O_READ)) {
            delete filePtr;
            request->send(500, "text/plain", "Failed to open compressed file");
            return;
        }
        const String contentType = getContentType(path);
        auto *response = new SdFileResponse(filePtr, contentType, filePtr->fileSize(), "br");
        request->send(response);
        return;
    }


    // --- Gzip ---
    compressedPath = path + ".gz";
    if (hasEncoding && sd.exists(compressedPath.c_str())) {
        SerialManager::enqueueLine("Serving gzip: " + compressedPath);
        auto *filePtr = new SdFile();
        if (!filePtr->open(compressedPath.c_str(), O_READ)) {
            delete filePtr;
            request->send(500, "text/plain", "Failed to open compressed file");
            return;
        }
        const String contentType = getContentType(path);
        auto *response = new SdFileResponse(filePtr, contentType, filePtr->fileSize(), "gzip");
        request->send(response);
        return;
    }

    SerialManager::enqueueLine("Falling back to uncompressed");

    // --- Fallback: uncompressed ---
    if (!sd.exists(path.c_str())) {
        request->send(404, "text/plain", "404: File Not Found");
        return;
    }
    auto *filePtr = new SdFile();
    if (!filePtr->open(path.c_str(), O_READ)) {
        delete filePtr;
        request->send(500, "text/plain", "Failed to open file");
        return;
    }

    const String contentType = getContentType(path);
    auto *response = new SdFileResponse(filePtr, contentType, filePtr->fileSize());
    request->send(response);
}

void DashboardServer::handleFavicon(AsyncWebServerRequest *request) {
    request->send(204);   // No content
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

const char* DashboardServer::getEncoding(const AsyncWebServerRequest *request) {
    auto *header = request->getHeader("Accept-Encoding");
    if (!header) return nullptr;

    const String value = header->value();
    SerialManager::enqueueLine("Encoding: " + value);
    if (value.indexOf("br") != -1) return "br";
    if (value.indexOf("gzip") != -1) return "gzip";
    return nullptr;
}