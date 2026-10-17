//
// Created by urzu-7 on 10/17/26.
//

#ifndef ENLATADORA_IOT_SDCARDMANAGER_H
#define ENLATADORA_IOT_SDCARDMANAGER_H

#include <Arduino.h>
#include <SdFat.h>

class SdCardManager {
    static inline SdFat sd;
    static inline bool mounted = false;
    static inline String rootPath = "/";

public:
    SdCardManager() = delete;

    static bool isMounted() { return mounted; }
    static const String &getRootPath() { return rootPath; }
    static void setRootPath(const String &path) { rootPath = path; }

    static String resolvePath(const String &path);

    static void init(const String &rootPath = "/");

    static bool exists(const String &path);
    static bool exists(const char *path);

    static SdFile *openFile(const String &path, oflag_t oflag = O_READ);
    static SdFile *openFile(const char *path, oflag_t oflag = O_READ);

    static bool open(SdFile &file, const String &path, oflag_t oflag = O_READ);
    static bool open(SdFile &file, const char *path, oflag_t oflag = O_READ);

    static SdFat &getSd() { return sd; }
};

#endif //ENLATADORA_IOT_SDCARDMANAGER_H
