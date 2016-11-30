/*
    Plugin Code:
    Copyright 2016 Daniel Arena <dan@remaincalm.org>
    LGPL3
 */

#ifndef RC_UTIL_H
#define RC_UTIL_H

#include "math.h"

const float PI = 3.141592653589793;

typedef int samples_t; // integral sample length or position
typedef float samples_frac_t; // fractional sample length or position
typedef float signal_t; // signal value

// DB to gain coefficient

constexpr float DB_CO(float g) {
    return (g > -90.0f) ? powf(10.0f, g * 0.05f) : 0.0f;
}

// DC filter. Call process once per sample.

class DcFilter {
public:

    signal_t process(const signal_t in) {
        out = 0.99 * out + in - prv_in;
        prv_in = in;
        return out;
    }

private:
    signal_t out = 0;
    signal_t prv_in = 0;
};

/* SmoothParam models parameter smoothing (LERP) over a fixed # samples
 * following parameter value updates.
 */
template <class T, int U = 2400 > class SmoothParam {
public:

    SmoothParam(T init) : value(init), start(init), end(init) {
    }

    SmoothParam<T, U>& operator=(T f) {
        start = value;
        end = f;
        t = 0;
        return *this;
    }

    SmoothParam<T, U>& operator+=(T f) {
        this->operator=(end + f);
        return *this;
    }

    SmoothParam<T, U>& operator-=(T f) {
        this->operator=(end - f);
        return *this;
    }

    operator T() const {
        return value;
    }

    void complete() {
        t = len;
        value = end;
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
    const int len = U;
};

#endif

