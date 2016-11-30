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
Mud is a filter, overdrive and octave down.

 */

#include "DistrhoPlugin.hpp"
#include "mud.hpp"

void MudPlugin::initProgramName(uint32_t index, String& programName) {
    static const char* names[] = {"beef", "sweep", "sweep 2", "vibe", "megavibe", "honk"};
    if (index < 6) {
        programName = names[index];
    }
}

void MudPlugin::loadProgram(uint32_t index) {

    const float params[][3] = {
        {50, 50, 0},
        {90, 80, 20},
        {70, 90, -13},
        {65, 80, 85},
        {100, 15, -40},
        {100, 75, 0},
    };

    if (index < 6) {
        setParameterValue(PARAM_MIX, params[index][0]);
        mix_.complete();

        setParameterValue(PARAM_FILTER, params[index][1]);
        setParameterValue(PARAM_LFO, params[index][2]);
    }

}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void MudPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomable;

    switch (index) {

        case PARAM_MIX:
            parameter.name = "Mix";
            parameter.symbol = "mix";
            parameter.unit = "%";
            parameter.ranges.def = 40;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_FILTER:
            parameter.name = "Filter";
            parameter.symbol = "filter";
            parameter.unit = "";
            parameter.ranges.def = 45;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_LFO:
            parameter.name = "LFO";
            parameter.symbol = "lfo";
            parameter.unit = "";
            parameter.ranges.def = 0;
            parameter.ranges.min = -100;
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
float MudPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case PARAM_MIX:
            return 100.0 * mix_;

        case PARAM_FILTER:
            return filter_;

        case PARAM_LFO:
            return lfo_;

        default:
            return 0;
    }
}

/**
  Change a parameter value.
  The host may call this function from any context, including realtime processing.
  When a parameter is marked as automable, you must ensure no non-realtime operations are performed.
 */
void MudPlugin::setParameterValue(uint32_t index, float value) {

    switch (index) {

        case PARAM_MIX:
            mix_ = 0.01 * value;
            break;

        case PARAM_FILTER:
            filter_ = value;
            break;

        case PARAM_LFO:
            lfo_ = value;
            break;
    }
}

void MudPlugin::fixFilterParams() {
    // LFO - deadzone from [-10,10]
    float lfo_depth = 0;
    if (lfo_ < -10) {
        lfo_depth = 20; // bigger on -ve side
    } else if (lfo_ > 10) {
        lfo_depth = 10;
    }
    float lfo_rate = fmax(fabs(lfo_) - 10.0, 0) * 0.0002;
    if (lfo_ < 0) { // faster on -ve side
        lfo_rate *= 3.0;
    }
    lfo_counter_ += 1;

    float new_filter = filter_ + lfo_depth * sin(lfo_rate * lfo_counter_);
    new_filter = fmin(fmax(new_filter, 0), 100); // clamp
    new_filter = new_filter * 0.1 + prv_filter_ * 0.9; // LERP to new filter value
    prv_filter_ = new_filter;

    // calc params from meta-param
    filter_res_ = 5.0 + ((int) new_filter / 2.0);
    filter_cutoff_ = 5.0 + fabs(fabs(160.0 - 3.2 * new_filter) - 80.0);
    filter_gain_comp_ = 3.0 - fabs(fabs(160.0 - 3.2 * new_filter) - 80.0) / 40.0;

    // set up R/C constants
    lpf_.c = powf(0.5, 4.6 - (filter_cutoff_ / 27.2));
    float lr = powf(0.5, -0.6 + filter_res_ / 40.0);
    lpf_.one_minus_rc = 1.0 - (lr * lpf_.c);

    hpf_.c = powf(0.5, 4.6 + (filter_cutoff_ / 34.8));
    float hr = powf(0.5, 3.0 - (filter_res_ / 63.5));
    hpf_.one_minus_rc = 1.0 - (hr * hpf_.c);
}

/**
  Run/process function for plugins without MIDI input.
 */
void MudPlugin::run(const float** inputs, float** outputs, uint32_t frames) {
    const float* const left_input = inputs[0];
    /* */ float* const left_output = outputs[0];

    // once per block
    fixFilterParams();

    for (uint32_t i = 0; i < frames; ++i) {
        left_output[i] = process(left_, left_input[i]);
        tick();
    }
}

signal_t MudPlugin::process(Channel& ch, const signal_t in) {
    signal_t curr = preSaturate(in);
    curr = filterLPF(ch, curr);
    curr = filterHPF(ch, curr);
    curr = postSaturate(curr);
    curr = ch.dc_filter.process(curr);

    if (mix_ < 0.5) {
        // dry full vol, fade in wet
        return in + 2.0 * mix_ * curr;
    } else {
        // wet full vol, fade out dry
        return curr + 2.0 * (1.0 - mix_) * in;
    }
}

signal_t MudPlugin::preSaturate(const signal_t in) const {
    signal_t curr = (1.0 + PRE_SHAPER) * in / (1.0 + PRE_SHAPER * fabs(in));
    return fmax(fmin(curr, CLAMP), -CLAMP);
}

signal_t MudPlugin::postSaturate(const signal_t in) const {
    return (1.0 + POST_SHAPER)*in / (1.0 + POST_SHAPER * fabs(in));
}

// Applies a bandpass filter to the current sample.

float MudPlugin::filterLPF(Channel& ch, const float in) const {
    ch.v0 = (lpf_.one_minus_rc) * ch.v0 + lpf_.c * (in - ch.v1);
    ch.v1 = (lpf_.one_minus_rc) * ch.v1 + lpf_.c * ch.v0;
    return ch.v1;
}

float MudPlugin::filterHPF(Channel& ch, const float in) const {
    ch.hv0 = (hpf_.one_minus_rc) * ch.hv0 + hpf_.c * (in - ch.hv1);
    ch.hv1 = (hpf_.one_minus_rc) * ch.hv1 + hpf_.c * ch.hv0;
    return in - ch.hv1;
}

// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new MudPlugin();
}
