//
// Created by urzu-7 on 8/31/26.
//

#ifndef ENLATADORA_IOT_SERIALQUEUE_H
#define ENLATADORA_IOT_SERIALQUEUE_H
#include <queue>
#include <string>
class SerialQueue {
    static inline std::queue<std::string> serialQueue;
    static inline std::vector<std::function<void(const char *)>> serialCallbacks;
    static inline TaskHandle_t task;
    static inline uint32_t period = 10;

    [[noreturn]] static void loop();

public:
    SerialQueue() = delete;

    static void enqueue(const std::string &message);

    static void enqueueLine(const std::string &message);

    static void enqueue(const String &message);
    static void enqueueLine(const String &message);

    static void enqueue(const char * str);
    static void enqueueLine(const char * str);

    static void registerCallback(const std::function<void(const char*)>& callback);
    static void run();
    static void init(uint32_t _microsecondsDelay = 10);


};


#endif //ENLATADORA_IOT_SERIALQUEUE_H
