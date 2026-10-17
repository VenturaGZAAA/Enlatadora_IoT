//
// Created by urzu-7 on 10/17/26.
//

#include "SdCardManager.h"


#define SD_CS   11
#define SD_MOSI 12
#define SD_SCK  13
#define SD_MISO 14


void SdCardManager::init(const String &_rootPath) {
    rootPath = _rootPath;

    //setup stuff
}