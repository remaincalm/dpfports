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
Paranoia is a bit crusher, distortion, resampler and mangler.

 */

#include "DistrhoPlugin.hpp"
#include "paranoia.hpp"
#include "math.h"

void ParanoiaPlugin::initProgramName(uint32_t index, String& programName) {
    static const char* names[] = {"grit", "more grit", "gated fuzz", "lofi", "invert", "lupine"};
    if (index < 6) {
        programName = names[index];
    }
}

void ParanoiaPlugin::loadProgram(uint32_t index) {

    const float params[][4] = {
        {-9, 100, 0, 40},
        {-3, 65, 0.5, 12.5},
        {-2, 0, 13.25, 60.94},
        {-1, 45, 3.75, 30},
        {-1, 90, 11, 34.4},
        {-9, 53.13, 12.50, 54.69}
    };

    if (index < 6) {
        setParameterValue(PARAM_WET_DB, params[index][0]);
        wet_out_db_.complete();

        setParameterValue(PARAM_CRUSH, params[index][1]);

        setParameterValue(PARAM_THERMONUCLEAR_WAR, params[index][2]);
        nuclear_.complete();

        setParameterValue(PARAM_FILTER, params[index][3]);
    }
}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void ParanoiaPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomable;

    switch (index) {
        case PARAM_WET_DB:
            parameter.name = "Level";
            parameter.symbol = "wet";
            parameter.unit = "dB";
            parameter.ranges.def = -3;
            parameter.ranges.min = -24;
            parameter.ranges.max = 6;
            break;

        case PARAM_CRUSH:
            parameter.name = "Crush";
            parameter.symbol = "crush";
            parameter.unit = "";
            parameter.ranges.def = 95;
            parameter.ranges.min = 0;
            parameter.ranges.max = 100;
            break;

        case PARAM_THERMONUCLEAR_WAR:
            parameter.name = "Mangle";
            parameter.symbol = "nuclear";
            parameter.unit = "";
            parameter.ranges.def = 0;
            parameter.ranges.min = 0;
            parameter.ranges.max = 16;
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
float ParanoiaPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case PARAM_WET_DB:
            return wet_out_db_;

        case PARAM_CRUSH:
            return crush_;

        case PARAM_THERMONUCLEAR_WAR:
            return nuclear_;

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
void ParanoiaPlugin::setParameterValue(uint32_t index, float value) {

    switch (index) {
        case PARAM_WET_DB:
            wet_out_db_ = value;
            break;

        case PARAM_CRUSH:
            crush_ = value;
            fixCrushParams();
            break;

        case PARAM_THERMONUCLEAR_WAR:
            nuclear_ = value;

        case PARAM_FILTER:
            filter_ = value;
            fixFilterParams();
            break;
    }
}

void ParanoiaPlugin::fixCrushParams() {
    bitdepth_ = (crush_ < 50) ? 6 : 10;
    if (crush_ > 99.0) {
        resample_hz_ = srate;
    } else if (crush_ > 50) {
        resample_hz_ = 300.0 + (crush_ - 50.0) * 600.0;
    } else {
        resample_hz_ = 300.0 + (50.0 - crush_) * 600.0;
    }
    per_sample_ = (float) srate / (float) resample_hz_;
    bitscale_ = pow(2, bitdepth_ - 1) - 0.5;
}

void ParanoiaPlugin::fixFilterParams() {
    // cutoff shape is \/\/
    // need to compensate for filter gain
    filter_cutoff_ = 20.0 + fabs(fabs(160.0 - 3.2 * filter_) - 80.0);
    filter_gain_comp_ = 3.0 - fabs(fabs(160.0 - 3.2 * filter_) - 80.0) / 40.0;

    // calc params from meta-param
    if (filter_ <= 80) {
        filter_mode_ = MODE_BANDPASS;
        filter_res_ = 10 + (filter_ / 8.0);
    } else if (filter_ <= 99) {
        filter_res_ = 40.0;
        filter_gain_comp_ = 1;
        filter_mode_ = MODE_HPF;
    } else {
        filter_gain_comp_ = 1;
        filter_mode_ = MODE_OFF;
    }

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
void ParanoiaPlugin::run(const float** inputs, float** outputs, uint32_t frames) {
    const float* const left_input = inputs[0];
    /* */ float* const left_output = outputs[0];

    for (uint32_t i = 0; i < frames; ++i) {
        left_output[i] = process(left_, left_input[i]);
        tick();
    }
}

signal_t ParanoiaPlugin::process(Channel& ch, const signal_t in) {
    signal_t curr = pregain(ch, in);
    curr = resample(ch, curr);
    curr = preSaturate(curr);
    curr = bitcrush(curr);

    if (filter_mode_ == MODE_LPF || filter_mode_ == MODE_BANDPASS) {
        curr = filterLPF(ch, curr);
    }
    if (filter_mode_ == MODE_HPF || filter_mode_ == MODE_BANDPASS) {
        curr = filterHPF(ch, curr);
    }
    curr = filter_gain_comp_ * DB_CO(wet_out_db_) * curr; // boost before post-saturate
    curr = postSaturate(curr);
    curr = ch.dc_filter.process(curr);
    return curr;
}

signal_t ParanoiaPlugin::pregain(const Channel& ch, const signal_t in) const {
    return DB_CO(gain_db_) * in;
}

// resample is a dodgy resampler that sounds cool.

signal_t ParanoiaPlugin::resample(Channel& ch, const signal_t in) const {
    ch.sample_csr += 1;
    if (ch.sample_csr < ch.next_sample && resample_hz_ < RESAMPLE_MAX) {
        return ch.prev_in;
    } else {
        ch.next_sample += per_sample_;
        if (resample_hz_ == RESAMPLE_MAX) {
            ch.sample_csr = ch.next_sample;
        }
        ch.prev_in = in;
        return in;
    }
}

signal_t ParanoiaPlugin::bitcrush(const signal_t in) const {
    // boost from [-1, 1] to [0, 2^bitdepth) and truncate.
    float curr = (1.0 + in) * (float) bitscale_;

    // truncate
    curr = (int) curr;

    // Mangle (interpolating between L and R settings on mangle knob.
    float nuclear_l = (int) nuclear_;
    float mix = nuclear_ - nuclear_l;
    float nuclear_r = nuclear_l + ((mix > 0.001) ? 1 : 0);
    signal_t left = mangler_.mangleForBitDepth(nuclear_l, bitdepth_, curr);
    signal_t right = mangler_.mangleForBitDepth(nuclear_r, bitdepth_, curr);
    curr = left * (1.0 - mix) + right * mix;

    // Return to [-1, 1] range.
    curr = (curr / (float) bitscale_) - 1.0;
    float gain_l = mangler_.relgain(nuclear_l);
    float gain_r = mangler_.relgain(nuclear_r);
    float gain = gain_l * (1.0 - mix) + gain_r * mix;
    return curr * gain;
}

signal_t ParanoiaPlugin::preSaturate(const signal_t in) const {
    signal_t curr = (1.0 + PRE_SHAPER) * in / (1.0 + PRE_SHAPER * fabs(in));
    return fmax(fmin(curr, CLAMP), -CLAMP);
}

signal_t ParanoiaPlugin::postSaturate(const signal_t in) const {
    return (1.0 + POST_SHAPER)*in / (1.0 + POST_SHAPER * fabs(in));
}

// Applies a bandpass filter to the current sample.

float ParanoiaPlugin::filterLPF(Channel& ch, const float in) const {
    ch.v0 = (lpf_.one_minus_rc) * ch.v0 + lpf_.c * (in - ch.v1);
    ch.v1 = (lpf_.one_minus_rc) * ch.v1 + lpf_.c * ch.v0;
    return ch.v1;
}

float ParanoiaPlugin::filterHPF(Channel& ch, const float in) const {
    ch.hv0 = (hpf_.one_minus_rc) * ch.hv0 + hpf_.c * (in - ch.hv1);
    ch.hv1 = (hpf_.one_minus_rc) * ch.hv1 + hpf_.c * ch.hv0;
    return in - ch.hv1;
}

// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new ParanoiaPlugin();
}
