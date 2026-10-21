//
// Created by urzu-7 on 10/17/26.
//

#include "SdCardManager.h"
#include "drivers/universal/SerialMonitor.h"
#include <SPI.h>

#define SD_CS   11
#define SD_MOSI 12
#define SD_SCK  13
#define SD_MISO 14

void SdCardManager::ensureMutex() {
    if (!sdMutex) {
        sdMutex = xSemaphoreCreateRecursiveMutex();
    }
}

bool SdCardManager::lock(const TickType_t timeout) {
    ensureMutex();
    if (!sdMutex) {
        return false;
    }
    return xSemaphoreTakeRecursive(sdMutex, timeout) == pdTRUE;
}

void SdCardManager::unlock() {
    if (sdMutex) {
        xSemaphoreGiveRecursive(sdMutex);
    }
}

SemaphoreHandle_t SdCardManager::getMutex() {
    ensureMutex();
    return sdMutex;
}

bool SdCardManager::isMounted() {
    LockGuard lock(pdMS_TO_TICKS(100));
    return mounted;
}

const String &SdCardManager::getRootPath() {
    return rootPath;
}


void SdCardManager::init(const String &_rootPath) {
    LockGuard lock(pdMS_TO_TICKS(5000));
    rootPath = _rootPath;

    SerialMonitor::writeLine("📀 Initializing SD card...");
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (const SdSpiConfig cfg(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI); !sd.begin(cfg)) {
        SerialMonitor::writeLine(" ❌ Card Mount Failed!");
        SPI.end();
        mounted = false;
    } else {
        mounted = true;
        SerialMonitor::writeLine("✅ SD card mounted");
    }
}

String SdCardManager::resolvePath(const String &path) {
    LockGuard lock(pdMS_TO_TICKS(100));
    if (rootPath.isEmpty() || rootPath == "/") {
        if (path.startsWith("/")) {
            return path;
        }
        return "/" + path;
    }

    String cleanRoot = rootPath;
    while (cleanRoot.endsWith("/")) {
        cleanRoot = cleanRoot.substring(0, cleanRoot.length() - 1);
    }
    if (!cleanRoot.startsWith("/")) {
        cleanRoot = "/" + cleanRoot;
    }

    if (path.startsWith(cleanRoot)) {
        return path;
    }

    if (path.startsWith("/")) {
        return cleanRoot + path;
    }
    return cleanRoot + "/" + path;
}

bool SdCardManager::exists(const String &path) {
    if (const LockGuard lock(pdMS_TO_TICKS(1000)); !lock || !mounted) {
        return false;
    }
    const String fullPath = resolvePath(path);
    return sd.exists(fullPath.c_str());
}

bool SdCardManager::exists(const char *path) {
    if (!path) {
        return false;
    }
    return exists(String(path));
}

SdFile *SdCardManager::openFile(const String &path, oflag_t oflag) {
    if (const LockGuard lock(pdMS_TO_TICKS(1000)); !lock || !mounted) {
        return nullptr;
    }
    const String fullPath = resolvePath(path);
    auto *file = new SdFile();
    if (!file->open(&sd, fullPath.c_str(), oflag)) {
        delete file;
        return nullptr;
    }
    return file;
}

SdFile *SdCardManager::openFile(const char *path, const oflag_t oflag) {
    if (!path) {
        return nullptr;
    }
    return openFile(String(path), oflag);
}

bool SdCardManager::open(SdFile &file, const String &path, const oflag_t oflag) {
    if (const LockGuard lock(pdMS_TO_TICKS(1000)); !lock || !mounted) {
        return false;
    }
    const String fullPath = resolvePath(path);
    return file.open(&sd, fullPath.c_str(), oflag);
}

bool SdCardManager::open(SdFile &file, const char *path, const oflag_t oflag) {
    if (!path) {
        return false;
    }
    return open(file, String(path), oflag);
}

bool SdCardManager::closeFile(SdFile *&file) {
    if (!file) {
        return false;
    }
    if (const LockGuard lock(pdMS_TO_TICKS(1000)); !lock) {
        return false;
    }
    const bool res = file->close();
    delete file;
    file = nullptr;
    return res;
}

bool SdCardManager::close(SdFile &file) {
    if (const LockGuard lock(pdMS_TO_TICKS(1000)); !lock) {
        return false;
    }
    return file.close();
}