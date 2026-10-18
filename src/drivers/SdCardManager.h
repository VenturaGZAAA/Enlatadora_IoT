//
// Created by urzu-7 on 10/17/26.
//

#ifndef ENLATADORA_IOT_SDCARDMANAGER_H
#define ENLATADORA_IOT_SDCARDMANAGER_H

#include <Arduino.h>
#include <SdFat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SdCardManager {
    static inline SdFat sd;
    static inline bool mounted = false;
    static inline String rootPath = "/";
    static inline SemaphoreHandle_t sdMutex = nullptr;

    static void ensureMutex();



public:
    class LockGuard {
        bool locked = false;

    public:
        explicit LockGuard(const TickType_t timeout = portMAX_DELAY) {
            locked = lock(timeout);
        }

        ~LockGuard() {
            if (locked) {
                unlock();
            }
        }

        [[nodiscard]] bool isLocked() const { return locked; }
        explicit operator bool() const { return locked; }
    };

    SdCardManager() = delete;

    static bool lock(TickType_t timeout = portMAX_DELAY);
    static void unlock();
    static SemaphoreHandle_t getMutex();

    static bool isMounted();
    static const String &getRootPath();

    static String resolvePath(const String &path);

    static void init(const String &_rootPath = "/");

    static bool exists(const String &path);
    static bool exists(const char *path);

    static SdFile *openFile(const String &path, oflag_t oflag = O_READ);
    static SdFile *openFile(const char *path, oflag_t oflag = O_READ);

    static bool open(SdFile &file, const String &path, oflag_t oflag = O_READ);
    static bool open(SdFile &file, const char *path, oflag_t oflag = O_READ);

    static bool closeFile(SdFile *&file);
    static bool close(SdFile &file);
};

#endif //ENLATADORA_IOT_SDCARDMANAGER_H
