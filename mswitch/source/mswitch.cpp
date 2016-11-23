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
Mswitch is a MIDI-controlled switcher so I can use my DMC-4 Gen 2
as additional toggle inputs for my mod duo.

 */

#include "DistrhoPlugin.hpp"
#include "mswitch.hpp"
#include "math.h"

void MswitchPlugin::initProgramName(uint32_t index, String& programName) {
    switch (index) {
        case 0:
            programName = "Mswitch Default";
            break;
    }
}

void MswitchPlugin::loadProgram(uint32_t index) {
    switch (index) {
        case 0:
            setParameterValue(PARAM_P1, 178);
            setParameterValue(PARAM_P2, 87);
            break;
    }
}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void MswitchPlugin::initParameter(uint32_t index, Parameter& parameter) {

    switch (index) {
        case PARAM_P1:
            parameter.name = "Byte 1";
            parameter.symbol = "p1";
            break;

        case PARAM_P2:
            parameter.name = "Byte 2";
            parameter.symbol = "p2";
            break;
    }

    parameter.hints = kParameterIsAutomable | kParameterIsInteger;
    parameter.unit = "";
    parameter.ranges.def = 0;
    parameter.ranges.min = 0;
    parameter.ranges.max = 255;

}

// -------------------------------------------------------------------
// Internal data

/**
  Get the current value of a parameter.
  The host may call this function from any context, including realtime processing.
 */
float MswitchPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case PARAM_P1:
            return p1_;

        case PARAM_P2:
            return p2_;

        default:
            return 0;
    }
}

/**
  Change a parameter value.
  The host may call this function from any context, including realtime processing.
  When a parameter is marked as automable, you must ensure no non-realtime operations are performed.
 */
void MswitchPlugin::setParameterValue(uint32_t index, float value) {

    switch (index) {
        case PARAM_P1:
            p1_ = value;
            break;

        case PARAM_P2:
            p2_ = value;
            break;
    }
}

/**
    Run/process function for plugins with MIDI input.
 */
void MswitchPlugin::run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* events, uint32_t eventCount) {
    const float* const in = inputs[0];
    float* const wet = outputs[0];
    float* const dry = outputs[1];

    for (uint32_t i = 0; i < eventCount; ++i) {
        MidiEvent event = events[i];
        if (event.size == 3 && event.data[0] == p1_ && event.data[1] == p2_ /* ignore data[2] */) {
            state_ = !state_;
        }
    }
    if (state_) {
        for (uint32_t i = 0; i < frames; ++i) {
            wet[i] = in[i];
            dry[i] = 0;
        }
    } else {
        for (uint32_t i = 0; i < frames; ++i) {
            wet[i] = 0;
            dry[i] = in[i];
        }
    }
}

// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new MswitchPlugin();
}
