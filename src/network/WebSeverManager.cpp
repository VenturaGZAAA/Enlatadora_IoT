#include "WebSeverManager.h"
#include "drivers/universal/Monitor.h"
#ifdef SD_CARD_CONNECTED
#include "drivers/specific/SdCardManager.h"
#endif
#if __has_include("config/build_flags.h")
#include "config/build_flags.h"
#endif
#include "drivers/universal/WifiManager.h"
#include <Update.h>
#include <utility>

// Static definitions
AsyncWebServer WebSeverManager::server(80);

// ------------------------------------------------------------------
// Setup

void WebSeverManager::setup() {
#ifdef SD_CARD_CONNECTED
    if (SdCardManager::isMounted()) {
        // Configure routes
        server.on("/favicon.ico", HTTP_GET, handleFavicon);
        server.onNotFound(handleFileRequest);
    }
#endif


    // Start server
    server.begin();
    Monitor::writeLine("🌐 HTTP server started on port 80");
#ifdef SD_CARD_CONNECTED
    if (SdCardManager::isMounted()) {
        Monitor::writeLine("📍 Open http://" + WifiManager::getServerIP().toString() + " in your browser");
    }
#endif
    WifiManager::addService("http", "tcp", 80);
}

#ifdef SD_CARD_CONNECTED

void WebSeverManager::setOnRootRequested(std::function<void()> callback) {
    onRootRequested = std::move(callback);
}
#endif

void WebSeverManager::setOnUpdateSuccessful(std::function<void()> callback) {
    onUpdateSuccessful = std::move(callback);
}

void WebSeverManager::registerJsonPostEndpoint(const String &path,
                                               const std::function<void(const JsonDocument &)> &callback,
                                               const char *onError) {
    server.on(path, HTTP_POST, [](AsyncWebServerRequest *request) {
                  if (request->_tempObject == nullptr) {
                      request->send(200, "application/json", R"({"status":"success"})");
                  } else {
                      const auto *errorMsg = static_cast<String *>(request->_tempObject);
                      const String responseJson = R"({"status":"error","message":")" + *errorMsg + R"("})";
                      delete errorMsg;
                      request->_tempObject = nullptr;
                      request->send(400, "application/json", responseJson);
                  }
              }, nullptr, [callback, onError](AsyncWebServerRequest *request, uint8_t *data, size_t length, size_t,
                                              size_t) {
                  JsonDocument doc;
                  const DeserializationError error = deserializeJson(doc, data, length);
                  if (error) {
                      Monitor::write(onError);
                      Monitor::writeLine(error.c_str());
                      request->_tempObject = new String(error.c_str());
                      return;
                  }
                  callback(doc);
                  request->_tempObject = nullptr;
              });
}

void WebSeverManager::enableWifiPostEndpoint(const String &path) {
    registerJsonPostEndpoint(path, WifiManager::wifiConfigJsonCallback, "Error parsing JSON for Wifi config on HTTP: ");
}

void WebSeverManager::enableOTASupport(const char *username, const char *password, const char *path) {
    const String user = username ? String(username) : String();
    const String pass = password ? String(password) : String();
    const bool authRequired = user.length() > 0 && pass.length() > 0;

    auto &handler = server.on(
        path, HTTP_POST,
        // 1. Final Request Handler (runs after upload finishes)
        [user, pass, authRequired](AsyncWebServerRequest *request) {
            if (authRequired && !request->authenticate(user.c_str(), pass.c_str())) {
                return request->requestAuthentication();
            }

            const bool success = !Update.hasError() && Update.isFinished();
            AsyncWebServerResponse *response = request->beginResponse(
                success ? 200 : 500,
                "text/plain",
                success ? "Update OK, rebooting...\n" : String("Update FAILED: ") + Update.errorString() + "\n"
            );
            response->addHeader("Connection", "close");
            request->send(response);

            if (success) {
                // Schedule restart in a separate FreeRTOS task so AsyncTCP can finish transmitting the response
                xTaskCreate([](void *) {
                    if (onUpdateSuccessful) {
                        onUpdateSuccessful();
                    }
                    Monitor::writeLine("Update OK, rebooting...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP.restart();
                }, "ota_restart", 2048, nullptr, 1, nullptr);
            } else {
                Update.abort();
            }
        },
        // 2. Upload Handler (runs for multipart/form-data file uploads, e.g. curl -F "firmware=@...")
        [user, pass, authRequired](AsyncWebServerRequest *request, const String &filename, const size_t index,
                                   uint8_t *data, const size_t len,
                                   const bool final) {
            if (authRequired && !request->authenticate(user.c_str(), pass.c_str())) {
                return;
            }

            static size_t lastReportedPercent = 0;
            if (index == 0) {
                lastReportedPercent = 0;
                // First chunk: start the update
                Monitor::writeLine("Update Start (Multipart): " + filename);
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError() && Update.isRunning()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                } else {
                    const size_t total = request->contentLength();
                    if (total > 0) {
                        const size_t current = index + len;
                        const size_t percent = (current * 100) / total;
                        if (percent >= lastReportedPercent + 10 && percent < 100) {
                            lastReportedPercent = (percent / 10) * 10;
                            Monitor::writeLine(
                                "OTA Progress: " + String(lastReportedPercent) + "% (" + String(current) + " bytes)");
                        }
                    }
                }
            }
            if (final) {
                // Last chunk: finalize the update
                if (Update.end(true)) {
                    Monitor::writeLine("Update Success: " + String(index + len) + " bytes written (100%)");
                } else {
                    Update.printError(Serial);
                }
            }
        },
        // 3. Body Handler (runs for raw binary stream POSTs, e.g. curl -H "Content-Type: application/octet-stream" --data-binary @...)
        [user, pass, authRequired](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index,
                                   const size_t total) {
            if (authRequired && !request->authenticate(user.c_str(), pass.c_str())) {
                return;
            }

            static size_t lastReportedPercent = 0;
            if (index == 0) {
                lastReportedPercent = 0;
                // First chunk: start the update
                Monitor::writeLine("Update Start (Raw Body): " + String(total) + " bytes");
                const size_t updateSize = (total > 0 && total != UPDATE_SIZE_UNKNOWN) ? total : UPDATE_SIZE_UNKNOWN;
                if (!Update.begin(updateSize, U_FLASH)) {
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError() && Update.isRunning()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                } else if (total > 0) {
                    const size_t current = index + len;
                    const size_t percent = (current * 100) / total;
                    if (percent >= lastReportedPercent + 10 && percent < 100) {
                        lastReportedPercent = (percent / 10) * 10;
                        Monitor::writeLine(
                            "OTA Progress: " + String(lastReportedPercent) + "% (" + String(current) + "/" +
                            String(total) + " bytes)");
                    }
                }
            }
            if (index + len == total || (total == 0 && len == 0)) {
                // Last chunk: finalize the update
                if (Update.end(true)) {
                    Monitor::writeLine("Update Success: " + String(index + len) + " bytes written (100%)");
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    if (authRequired) {
        handler.setAuthentication(username, password);
    }
}

void WebSeverManager::handleFileRequest(AsyncWebServerRequest *request) {
    // 1. Determine the base file path (without compression suffix)
    String path = request->url();

#ifndef SD_CARD_CONNECTED
    Monitor::writeLine("🌐 HTTP server request unhandled: " + path);
    request->send(404, "text/plain", "File not found");
#else
    if (path == "/" || path == "") {
        path = webPath + "/index.html";
        if (onRootRequested) {
            onRootRequested();
        }
    } else {
        path = webPath + path;
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
        Monitor::writeLine("File not found: " + filePath);
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
#endif
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
