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
Sub is a filter, overdrive and downsampler.

 */

#include "DistrhoPlugin.hpp"
#include "sub.hpp"
#include "math.h"

void SubPlugin::initProgramName(uint32_t index, String& programName) {
    switch (index) {
        case 0:
            programName = "Sub Default";
            break;
    }
}

void SubPlugin::loadProgram(uint32_t index) {
    switch (index) {
        case 0:
            setParameterValue(PARAM_DRY_DB, -96);
            setParameterValue(PARAM_WET_DB, -3);
            setParameterValue(PARAM_FILTER, 45);
            break;
    }
}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void SubPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomable;

    switch (index) {
        case PARAM_DRY_DB:
            parameter.name = "Dry Out";
            parameter.symbol = "dry";
            parameter.unit = "dB";
            parameter.ranges.def = -96;
            parameter.ranges.min = -96;
            parameter.ranges.max = 6;
            break;

        case PARAM_WET_DB:
            parameter.name = "Wet Out";
            parameter.symbol = "wet";
            parameter.unit = "dB";
            parameter.ranges.def = -3;
            parameter.ranges.min = -24;
            parameter.ranges.max = 6;
            break;

        case PARAM_FILTER:
            parameter.name = "Filter";
            parameter.symbol = "filter";
            parameter.unit = "";
            parameter.ranges.def = 45;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;
    }

}

// -------------------------------------------------------------------
// Internal data

/**
  Get the current value of a parameter.
  The host may call this function from any context, including realtime processing.
 */
float SubPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case PARAM_DRY_DB:
            return dry_out_db_;

        case PARAM_WET_DB:
            return wet_out_db_;

        case PARAM_FILTER:
            return filter_;

        default:
            return 0;
    }
}

/**
  Change a parameter value.
  The host may call this function from any context, including realtime processing.
  When a parameter is marked as automable, you must ensure no non-realtime operations are performed.
 */
void SubPlugin::setParameterValue(uint32_t index, float value) {

    switch (index) {
        case PARAM_DRY_DB:
            dry_out_db_ = value;
            break;

        case PARAM_WET_DB:
            wet_out_db_ = value;
            break;

        case PARAM_FILTER:
            filter_ = value;
            fixFilterParams();
            break;
    }
}

void SubPlugin::fixFilterParams() {
    // calc params from meta-param
    filter_res_ = 10 + ((int) filter_ / 20);

    // cutoff shape is \/\/
    filter_cutoff_ = 10.0 + fabs(fabs(160.0 - 3.2 * filter_) - 80.0);

    // set up R/C constants
    lpf_.c = powf(0.5, 4.6 - (filter_cutoff_ / 27.2));
    float lr = powf(0.5, -0.6 + filter_res_ / 40.0);
    lpf_.one_minus_rc = 1.0 - (lr * lpf_.c);

    hpf_.c = powf(0.5, 4.6 + (filter_cutoff_ / 34.8));
    float hr = powf(0.5, 3.0 - (filter_res_ / 43.5));
    hpf_.one_minus_rc = 1.0 - (hr * hpf_.c);
}

/**
  Run/process function for plugins without MIDI input.
 */
void SubPlugin::run(const float** inputs, float** outputs, uint32_t frames) {
    const float* const left_input = inputs[0];
    /* */ float* const left_output = outputs[0];

    for (uint32_t i = 0; i < frames; ++i) {
        left_output[i] = process(left_, left_input[i]);
        tick();
    }
}

signal_t SubPlugin::process(Channel& ch, const signal_t in) {
    signal_t curr = pregain(ch, in);
    curr = preSaturate(curr);
    curr = filterLPF(ch, curr);
    curr = filterHPF(ch, curr);
    curr = DB_CO(wet_out_db_) * curr; // boost before post-saturate
    curr = postSaturate(curr);
    curr = ch.dc_filter.process(curr);
    return DB_CO(dry_out_db_) * in + curr;
}

signal_t SubPlugin::pregain(const Channel& ch, const signal_t in) const {
    return DB_CO(gain_db_) * in;
}

signal_t SubPlugin::preSaturate(const signal_t in) const {
    signal_t curr = (1.0 + PRE_SHAPER) * in / (1.0 + PRE_SHAPER * fabs(in));
    return fmax(fmin(curr, CLAMP), -CLAMP);
}

signal_t SubPlugin::postSaturate(const signal_t in) const {
    return (1.0 + POST_SHAPER)*in / (1.0 + POST_SHAPER * fabs(in));
}

// Applies a bandpass filter to the current sample.

float SubPlugin::filterLPF(Channel& ch, const float in) const {
    ch.v0 = (lpf_.one_minus_rc) * ch.v0 + lpf_.c * (in - ch.v1);
    ch.v1 = (lpf_.one_minus_rc) * ch.v1 + lpf_.c * ch.v0;
    return ch.v1;
}

float SubPlugin::filterHPF(Channel& ch, const float in) const {
    ch.hv0 = (hpf_.one_minus_rc) * ch.hv0 + hpf_.c * (in - ch.hv1);
    ch.hv1 = (hpf_.one_minus_rc) * ch.hv1 + hpf_.c * ch.hv0;
    return in - ch.hv1;
}

// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new SubPlugin();
}
