cmake_minimum_required(VERSION 3.16)
project(Enlatadora_IoT C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    src
    src/config
    src/drivers
    src/machine
    src/network
    src/audio
    include
)

file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.c"
    "src/*.h"
    "include/*.h"
)

add_custom_target(Production ALL
    COMMAND platformio -f -c clion run
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${SRC_FILES}
)
