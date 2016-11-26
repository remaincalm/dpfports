/*
    Plugin Code:
    Copyright 2016 Daniel Arena <dan@remaincalm.org>
    LGPL3
 */

#ifndef RC_UTIL_H
#define RC_UTIL_H

#include "math.h"

/** Define a macro for converting a gain in dB to a coefficient. */
constexpr float DB_CO(float g) {
    return (g > -90.0f) ? powf(10.0f, g * 0.05f) : 0.0f;
}

/* SmoothParam models parameter smoothing (LERP) over a fixed # samples
 * following parameter value updates.
 */
template <class T> class SmoothParam {
public:

    SmoothParam(T init) : value(init), start(init), end(init) {
    }

    SmoothParam<T>& operator=(T f) {
        start = value;
        end = f;
        t = 0;
        return *this;
    }

    SmoothParam<T>& operator+=(T f) {
        this->operator=(end + f);
        return *this;
    }

    SmoothParam<T>& operator-=(T f) {
        this->operator=(end - f);
        return *this;
    }

    operator T() const {
        return value;
    }

    void tick() {
        if (t < len) {
            t += 1;
            const float frac = ((float) t / (float) len);
            value = (float) end * frac + (float) start * (1.0f - frac);
        } else {
            value = end;
        }
    }

private:
    T value = 0;
    T start = 0;
    T end = 0;
    int t = 0;
    const int len = 2400; // ~50ms at 48kHz
};

#endif

