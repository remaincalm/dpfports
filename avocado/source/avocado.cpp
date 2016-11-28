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
Avocado is a bit crusher, distortion, resampler and mangler.

 */

#include "DistrhoPlugin.hpp"
#include "avocado.hpp"
#include "math.h"
#include "stdlib.h"

void AvocadoPlugin::initProgramName(uint32_t index, String& programName) {
    switch (index) {
        case 0:
            programName = "Avocado Default";
            break;
    }
}

void AvocadoPlugin::loadProgram(uint32_t index) {
    switch (index) {
        case 0:
            setParameterValue(PARAM_BUF_LENGTH, 45);
            setParameterValue(PARAM_BUF_COUNT, 4);
            setParameterValue(PARAM_CHARACTER, 20);
            break;
    }
}

/**
  Initialize the parameter @a index.
  This function will be called once, shortly after the plugin is created.
 */
void AvocadoPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomable;

    switch (index) {

        case PARAM_BUF_LENGTH:
            parameter.name = "Time";
            parameter.symbol = "bufsiz";
            parameter.unit = "ms";
            parameter.ranges.def = 50;
            parameter.ranges.min = 10;
            parameter.ranges.max = 250;
            break;

        case PARAM_BUF_COUNT:
            parameter.hints = kParameterIsAutomable | kParameterIsInteger;
            parameter.name = "Buffers";
            parameter.symbol = "bufcount";
            parameter.unit = "";
            parameter.ranges.def = 4;
            parameter.ranges.min = 2;
            parameter.ranges.max = MAX_BUFFERS;
            break;

        case PARAM_CHARACTER:
            parameter.name = "Repeat";
            parameter.symbol = "repeats";
            parameter.unit = "%";
            parameter.ranges.def = 10;
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
float AvocadoPlugin::getParameterValue(uint32_t index) const {
    switch (index) {

        case PARAM_BUF_LENGTH:
            return int(1000.0 * buffer_size_ / srate);

        case PARAM_BUF_COUNT:
            return buffer_count_;

        case PARAM_CHARACTER:
            return repeat_prob_;

        default:
            return 0;
    }
}

/**
  Change a parameter value.
  The host may call this function from any context, including realtime processing.
  When a parameter is marked as automable, you must ensure no non-realtime operations are performed.
 */
void AvocadoPlugin::setParameterValue(uint32_t index, float value) {

    switch (index) {

        case PARAM_BUF_LENGTH:
            buffer_size_ = value * srate / 1000.0;
            break;

        case PARAM_BUF_COUNT:
            buffer_count_ = value;
            break;

        case PARAM_CHARACTER:
            repeat_prob_ = value;
            break;
    }
}

/**
  Run/process function for plugins without MIDI input.
 */
void AvocadoPlugin::run(const float** inputs, float** outputs, uint32_t frames) {
    const float* const left_input = inputs[0];
    /* */ float* const left_output = outputs[0];

    for (uint32_t i = 0; i < frames; ++i) {
        left_output[i] = process(left_, left_input[i]);
        tick();
    }
}

void AvocadoPlugin::record(Channel& ch, const signal_t in) {

    /*
    record:
      if not recording and rnd() > ?, start recording else stop recording.
      if recording, add to buffer. if buffer full, stop recording.
     */

    if (is_recording_) {
        ch.buffer[record_buffer_][record_csr_] = in;
        record_csr_ += 1;
        if (record_csr_ > buffer_size_) {
            // hit end of buffer
            is_recording_ = false;
        }
    } else {
        // start recording
        record_buffer_ = rand() % buffer_count_;
        if (record_buffer_ == playback_buffer_) {
            record_buffer_ = (record_buffer_ + 1) % buffer_count_;
        }
        record_csr_ = 0;
        is_recording_ = true;
    }
}

signal_t AvocadoPlugin::playback(Channel& ch, const signal_t in) {

    /*
    playback:
      if buffer ended, choose either same buffer or a new buffer
      play selected buffer
     */
    if (playback_csr_ > buffer_size_) {
        playback_csr_ = 0;

        // character:
        // [0,50] -> repeat last buffer
        // [50, 100] -> fadeout

        if (rand() % 100 > repeat_prob_) {
            playback_buffer_ = rand() % buffer_count_;

        }

        //        // fadeout block?
        //        if (rand() % 100 > 2 * character_ - 100) {
        //            for (int i = 0; i < buffer_size_; ++i) {
        //                // TODO(dca): replace this with a per-buffer gain value
        //                ch.buffer[playback_buffer_][i] *= 0.98;
        //            }
        //        }
    }

    // fade edges
    float mult = 1;

    if (playback_csr_ < FADE_SAMPLES) {
        mult = (float) playback_csr_ / (float) FADE_SAMPLES;
    } else if (playback_csr_ > buffer_size_ - FADE_SAMPLES) {
        mult = (float) (buffer_size_ - playback_csr_ - 1) / (float) FADE_SAMPLES;
    }

    // calculate raw output
    signal_t curr = mult * ch.buffer[playback_buffer_][playback_csr_];

    // move cursor
    playback_csr_ += 1;
    return curr;
}

float AvocadoPlugin::gate(Channel& ch, const signal_t in) {
    // rectify and leaky integrate
    leaky_integrator = fmin(leaky_integrator * leakage + fabs(in) * (1.0 - leakage), 1.0);
    return leaky_integrator < threshold_ ? 1.0 : 0.0;
}

signal_t AvocadoPlugin::process(Channel& ch, const signal_t in) {
    record(ch, in);
    signal_t curr = playback(ch, in);
    float target_gain = gate(ch, in);
    gain_ = gain_ * (1.0 - attack_) + target_gain * attack_;
    return in + curr * gain_;
}

// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new AvocadoPlugin();
}
