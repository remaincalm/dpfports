/*
    Plugin Code:
    Copyright 2016 Daniel Arena <dan@remaincalm.org>
    LGPL3

    Plugin Template:
    Copyright 2016 Filipe Coelho <falktx@falktx.com>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
Floaty is a weird delay FX unit.

It is a tape-style delay, except:

 * Record head moves around in one direction, constant speed.
 * Play head is free in both speed, direction, and is modulated.
 * Playback path runs through a resonant bandpass filter (actually a LPF + HPF)
 * There's some saturation in there as well.

This allows for:

 * Standard set of musical delays
 * Chorus/flanger
 * Pitch-shifting
 * Very deep and weird reverse reverbs

So, it's good fun.

 */

#include "DistrhoPlugin.hpp"
#include "floaty.hpp"
#include "math.h"
#include "util.hpp"

void FloatyPlugin::initProgramName(uint32_t index, String& programName) {
    switch (index) {
        case 0:
            programName = "Floaty Default";
            break;
    }
}

void FloatyPlugin::loadProgram(uint32_t index) {
    switch (index) {
        case 0:
            setParameterValue(PARAM_DELAY_MS, 300);
            setParameterValue(PARAM_MIX, 50);
            setParameterValue(PARAM_FEEDBACK, 20);
            setParameterValue(PARAM_WARP, 40);
            setParameterValue(PARAM_FILTER, 37.5);
            setParameterValue(PARAM_PLAYBACK_RATE, 1.0);
            break;
    }
}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void FloatyPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomable;
    switch (index) {
        case PARAM_DELAY_MS:
            parameter.name = "Delay";
            parameter.symbol = "delay";
            parameter.unit = "ms";
            parameter.ranges.def = 110;
            parameter.ranges.min = 10;
            parameter.ranges.max = 0.5 * 1000.0 * ((float) MAX_BUF / (float) srate);
            break;

        case PARAM_MIX:
            parameter.name = "Mix";
            parameter.symbol = "mix";
            parameter.unit = "%";
            parameter.ranges.def = 40;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_FEEDBACK:
            parameter.name = "Feedback";
            parameter.symbol = "feedback";
            parameter.unit = "%";
            parameter.ranges.def = 20;
            parameter.ranges.min = 0;
            parameter.ranges.max = 80;
            break;

        case PARAM_WARP:
            parameter.name = "Warp";
            parameter.symbol = "";
            parameter.unit = "";
            parameter.ranges.def = 48;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_FILTER:
            parameter.name = "Filter";
            parameter.symbol = "filter";
            parameter.unit = "";
            parameter.ranges.def = 60;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_PLAYBACK_RATE:
            parameter.name = "Playback Rate";
            parameter.symbol = "rate";
            parameter.unit = "x";
            parameter.ranges.def = 1;
            parameter.ranges.min = -2;
            parameter.ranges.max = 2;
            break;

    }
}

// -------------------------------------------------------------------
// Internal data

/**
  Get the current value of a parameter.
  The host may call this function from any context, including realtime processing.
 */
float FloatyPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case PARAM_DELAY_MS:
            return 1000.0 * (float) left_.delay / srate;
        case PARAM_MIX:
            return 100.0 * mix_;
        case PARAM_FEEDBACK:
            return 100.0 * feedback_;
        case PARAM_WARP:
            return warp_;
        case PARAM_FILTER:
            return filter_;
        case PARAM_PLAYBACK_RATE:
            return playback_rate_;
        default:
            return 0;
    }
}

/**
  Change a parameter value.
  The host may call this function from any context, including realtime processing.
  When a parameter is marked as automable, you must ensure no non-realtime operations are performed.
 */
void FloatyPlugin::setParameterValue(uint32_t index, float value) {
    bool filter_changed = false;
    bool delay_changed = false;
    switch (index) {
        case PARAM_DELAY_MS:
            delay_ = value * srate / 1000.0;
            delay_changed = true;
            break;

        case PARAM_MIX:
            mix_ = 0.01 * value;
            break;

        case PARAM_FEEDBACK:
            feedback_ = 0.01 * value;
            break;

        case PARAM_WARP:
            warp_ = value;
            if (warp_ <= 50) {
                warp_rate_hz_ = 0.08;
            } else {
                warp_rate_hz_ = 3.0;
            }
            warp_rate_rad_ = 2.0 * PI * warp_rate_hz_ / srate;
            warp_amount_ = 0.01 * fabs(2.0 - 0.04 * value);
            break;

        case PARAM_FILTER:
            filter_ = value;
            filter_changed = true;
            break;

        case PARAM_PLAYBACK_RATE:
            playback_rate_ = value;
            break;
    }

    // RC filter params (hi/lo) - selected by magic
    if (filter_changed) {
        fixFilterParams();
    }

    if (delay_changed) {
        fixDelayParams();
    }
}

void FloatyPlugin::fixDelayParams() {
    samples_frac_t lr_offset = (1.0 - 0.01 * channel_offset_) * delay_;
    left_.setDelay(delay_);
    right_.setDelay(delay_ + lr_offset);
    warp_counter_ = 0;
}

void FloatyPlugin::fixFilterParams() {
    filter_res_ = 0.1 + filter_ * 0.6;
    filter_cutoff_ = 50.0 + 45.0 * sin(filter_ / 10.0);

    lpf_.c = powf(0.5, 4.6 - (filter_cutoff_ / 27.2));
    float lr = powf(0.5, -0.6 + filter_res_ / 40.0);
    lpf_.one_minus_rc = 1.0 - (lr * lpf_.c);

    hpf_.c = powf(0.5, 4.1 + (filter_cutoff_ / 200.0));
    float hr = powf(0.5, 1 + filter_res_ / 200.0);
    hpf_.one_minus_rc = 1.0 - (hr * hpf_.c);
}

/**
  Run/process function for plugins without MIDI input.
 */
void FloatyPlugin::run(const float** inputs, float** outputs, uint32_t frames) {
    // TODO(dca): right channel.
    const float* const input = inputs[0];
    /* */ float* const left_output = outputs[0];
    /* */ float* const right_output = outputs[1];

    for (uint32_t i = 0; i < frames; ++i) {
        tick();
        left_output[i] = process(left_, input[i]);
        right_output[i] = process(right_, input[i]);
    }
}

signal_t FloatyPlugin::process(Channel& ch, const signal_t in) {
    // Read back from tape.
    advancePlayHead(ch);
    signal_t curr = readFromPlayHead(ch);
    curr = fadeNearOverlap(ch, curr);
    curr = saturate(curr);
    curr = bandpassFilter(ch, curr);

    // Write back to tape.
    ch.buf[ch.rec_csr] = in + curr * feedback_;

    advanceRecHead(ch);

    if (mix_ < 0.5) {
        // dry full vol, fade in wet
        return in + 2.0 * mix_ * curr;
    } else {
        // wet full vol, fade out dry
        return curr + 2.0 * (1.0 - mix) * in;
    }
}

// Advances the play head around the tape loop (w/ modulation).

void FloatyPlugin::advancePlayHead(Channel& ch) {
    ch.play_csr += playback_rate_;

    // clamp warp_amount_ to prevent overruns.
    // magic number chosen analytically, plus some buffer. i think there's a
    // better formula that has pi/2 in it somewhere but we need extra due to
    // wanting to stay away from the overlap region etc.
    // also need to use internal delay value, not channel delay, so that
    // L and R have the same limit.
    float max_warp_amount = ((channel_offset_ * delay_ / 100.0) - SMOOTH_OVERLAP * 2) * warp_rate_hz_ / 20000.0;
    samples_frac_t warp = fmin(max_warp_amount, warp_amount_) * sin(warp_counter_);
    ch.play_csr += warp;
    warp_counter_ += warp_rate_rad_;

    ch.play_csr = fmod(ch.play_csr + (samples_frac_t) ch.getModPoint(), ch.getModPoint());
}


// Sets sample to the current value under playhead.

signal_t FloatyPlugin::readFromPlayHead(const Channel& ch) const {
    // Get interpolation data from both sides of floating-point sample location.
    samples_t play_csr0 = ch.play_csr;
    samples_t play_csr1 = (play_csr0 + 1) % ch.getModPoint();
    samples_frac_t fraction = ch.play_csr - play_csr0;

    // LERP between both positions
    const signal_t s0 = ch.buf[play_csr0] * (1.0 - fraction);
    const signal_t s1 = ch.buf[play_csr1] * (fraction);
    return s0 + s1;
}

// Dampen value if play/rec cursor overlap to prevent clicks.

signal_t FloatyPlugin::fadeNearOverlap(const Channel& ch, signal_t in) const {
    samples_frac_t overlap_dist = fabs(
            (fmod(ch.play_csr, ch.getModPoint())) - (float) (ch.rec_csr % ch.getModPoint()));
    float overlap_mult = (overlap_dist >= SMOOTH_OVERLAP) ? 1.0 : (overlap_dist / SMOOTH_OVERLAP);
    return in * overlap_mult;
}

// Advances the record head around the tape loop (linear).

void FloatyPlugin::advanceRecHead(Channel& ch) {
    ch.rec_csr = (ch.rec_csr + 1) % ch.getModPoint();
}

signal_t FloatyPlugin::saturate(const signal_t in) const {
    float shaper_amt = 3.0f - 0.8f * in;
    return fmax(-CLAMP, fmin(CLAMP, CLAMP * ((1 + shaper_amt) * in) / (1 + (shaper_amt * fabs(in)))));
}

// Applies a bandpass filter to the current sample.

float FloatyPlugin::bandpassFilter(Channel& ch, const float in) {
    // LPF
    ch.v0 = (lpf_.one_minus_rc) * ch.v0 + lpf_.c * (in - ch.v1);
    ch.v1 = (lpf_.one_minus_rc) * ch.v1 + lpf_.c * ch.v0;
    // HPF
    ch.hv0 = (hpf_.one_minus_rc) * ch.hv0 + hpf_.c * (ch.v1 - ch.hv1);
    ch.hv1 = (hpf_.one_minus_rc) * ch.hv1 + hpf_.c * ch.hv0;
    return ch.v1 - ch.hv1;
}

// Per-channel processing.

Plugin* DISTRHO::createPlugin() {
    return new FloatyPlugin();
}
