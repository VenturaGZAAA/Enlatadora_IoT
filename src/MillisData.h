//
// Created by urzu-7 on 10/10/26.
//

#ifndef ENLATADORA_IOT_MILLISDATA_H
#define ENLATADORA_IOT_MILLISDATA_H


class MillisData {
    unsigned long lastTime = 0;
    unsigned long delay = 0;

public:
    explicit MillisData(const unsigned long d) {
        delay = d;
    }

    void reset();

    [[nodiscard]] bool isTime() const;
};

#endif //ENLATADORA_IOT_MILLISDATA_H
