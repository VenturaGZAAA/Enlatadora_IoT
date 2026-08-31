//
// Created by urzu-7 on 8/25/26.
//
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SdFat.h>

#include "DashboardServer.h"

// --- SD Card Pins for ESP32-S3-DevKitM-1 ---
#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12

WebServer DashboardServer::server = WebServer(80);
SdFat DashboardServer::sd;
SdFile DashboardServer::file;
bool DashboardServer::success = false;

void DashboardServer::setup() {
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

    success = true;
    // --- Configure Web Server ---
    server.on("/favicon.ico", handleFavicon);
    server.onNotFound(handleFileRequest);

    server.begin();
    Serial.println("🌐 HTTP server started on port 80");
    Serial.println("📍 Open http://" + WiFi.localIP().toString() + " in your browser");
    Serial.println("\n========================================\n");
}

void DashboardServer::loop() {
    if (!success) {
        return;
    }
    server.handleClient();
}

// New method to serve compressed files
void DashboardServer::streamCompressedFile(SdFile &fileTarget, const String &contentType, uint32_t originalSize) {
    const uint32_t compressedSize = fileTarget.fileSize();

    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: " + contentType + "\r\n";
    header += "Content-Length: " + String(compressedSize) + "\r\n";
    header += "Content-Encoding: gzip\r\n";
    header += "Cache-Control: public, max-age=31536000\r\n";  // Cache for 1 year
    header += "Connection: close\r\n";
    header += "\r\n";
    server.sendContent(header);

    constexpr uint16_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];

    while (fileTarget.available()) {
        if (const int bytesRead = fileTarget.read(buffer, CHUNK_SIZE); bytesRead > 0) {
            server.sendContent(reinterpret_cast<const char *>(buffer), bytesRead);
        }
    }
}


// Helper function to get content type
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

// Manually stream file from SdFat to web client
void DashboardServer::streamFile(SdFile &streamed_file, const String &contentType) {
    const uint32_t fileSize = streamed_file.fileSize();


    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: " + contentType + "\r\n";
    header += "Content-Length: " + String(fileSize) + "\r\n";
    header += "Connection: close\r\n";
    header += "\r\n"; // Empty line to end headers


    server.sendContent(header);

    constexpr uint16_t CHUNK_SIZE = 1024; // 1KB chunks
    uint8_t buffer[CHUNK_SIZE];

    size_t totalSent = 0;
    while (streamed_file.available()) {
        if (const int bytesRead = streamed_file.read(buffer, CHUNK_SIZE); bytesRead > 0) {
            server.sendContent(reinterpret_cast<const char *>(buffer), bytesRead);
            totalSent += bytesRead;
        }
    }
}

const char* DashboardServer::getEncoding() {
    if (server.hasHeader("Accept-Encoding")) {
        const String encoding = server.header("Accept-Encoding");
        if (encoding.indexOf("br") != -1) {
            return "br";
        }
        if (encoding.indexOf("gzip") != -1) {
            return "gzip";
        }
    }
    return nullptr;
}
// Update handleFileRequest to serve compressed files
void DashboardServer::handleFileRequest() {
    String path = server.uri();

    if (path == "/" || path == "") {
        path = "/web/index.html";
    } else {
        path = "/web" + path;
    }



    // Try to serve compressed version if browser supports it
    const char* encoding = getEncoding();

    // Check for brotli compressed file
    String compressedPath = String(path) + ".br";
    if (encoding != nullptr && strcmp(encoding, "br") == 0 && sd.exists(compressedPath.c_str())) {
        // Serve brotli compressed
        // ... similar to gzip but with Content-Encoding: br
        if (!file.open(compressedPath.c_str(), O_READ)) {
            server.send(500, "text/plain", "500: Failed to open compressed file");
            return;
        }

        const String contentType = getContentType(path);
        streamCompressedFile(file, contentType, 0);
        file.close();
        return;
    }
    compressedPath = String(path) + ".gz";

    // Check if compressed file exists and browser supports gzip
    if (encoding != nullptr && sd.exists(compressedPath.c_str())) {
        if (!file.open(compressedPath.c_str(), O_READ)) {
            server.send(500, "text/plain", "500: Failed to open compressed file");
            return;
        }

        const String contentType = getContentType(path);
        streamCompressedFile(file, contentType, 0);
        file.close();
        return;
    }

    // Fallback to uncompressed file
    if (!sd.exists(path.c_str())) {
        Serial.println("❌ File not found: " + path);
        server.send(404, "text/plain", "404: File Not Found");
        return;
    }

    if (!file.open(path.c_str(), O_READ)) {
        server.send(500, "text/plain", "500: Failed to open file");
        return;
    }

    const String contentType = getContentType(path);
    streamFile(file, contentType);
    file.close();
}

// Handle favicon.ico requests (optional, to avoid 404s)
void DashboardServer::handleFavicon() {
    server.send(204, "text/plain", ""); // No content
}
