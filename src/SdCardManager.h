//
// Created by urzu-7 on 10/17/26.
//

#ifndef ENLATADORA_IOT_SDCARDMANAGER_H
#define ENLATADORA_IOT_SDCARDMANAGER_H
#include <SdFat.h>


class SdCardManager {
    static inline SdFat sd;
    static inline bool mounted = false;
    static inline String rootPath = "/";

public:
    static bool isMounted() { return mounted; }

    static void init(const String &rootPath = "/");
};


#endif //ENLATADORA_IOT_SDCARDMANAGER_H
