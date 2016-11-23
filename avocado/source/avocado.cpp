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
            setParameterValue(PARAM_DRY_DB, -96);
            setParameterValue(PARAM_WET_DB, -3);

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
            parameter.ranges.min = -96;
            parameter.ranges.max = 6;
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
        case PARAM_DRY_DB:
            return dry_out_db_;

        case PARAM_WET_DB:
            return wet_out_db_;

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
        case PARAM_DRY_DB:
            dry_out_db_ = value;
            break;

        case PARAM_WET_DB:
            wet_out_db_ = value;
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
      if rnd() > ?, start recording else stop recording.
      if recording, add to buffer. if buffer full, stop recording.
     */

    if (is_recording_) {
        ch.buffer[max_bufsiz_ * record_buffer_ + record_csr_] = in;
        record_csr_ += 1;

        if (record_csr_ > bufsiz_) {
            // TODO: measure volume level of that block
            is_recording_ = false;
        }
    } else {
        // start recording
        record_buffer_ = (int) (rand(buffer_count_));
        record_csr_ = 0;
        is_recording_ = true;
    }
}

signal_t AvocadoPlugin::process(Channel& ch, const signal_t in) {

    record(ch, in);

    /*
    playback:
      if buffer ended, choose either same buffer or a new buffer
      play selected buffer
     */
    if (playback_csr_ > bufsiz_) {

        playback_csr_ = 0;

        // TODO: try and choose a block that matches last input volume level?

        if (rand(100) > repeat_probability_) {
            playback_buffer_ = (int) (rand(buffer_count_));
        }

        // fadeout block?
        if (rand(100) > 105 - fadeout_probability_) {
            for (int i = 0; i < bufsiz_; ++i) {
                ch.buffer[max_bufsiz_ * playback_buffer_ + i] *= 0.85;
            }
        }
    }

    // fade edges
    float mult = 1;

    if (playback_csr_ < fade_samples_) {
        mult = (float) playback_csr_ / (float) fade_samples_;
    } else if (playback_csr_ > bufsiz_ - fade_samples_) {
        mult = (float) (bufsiz_ - playback_csr_ - 1) / (float) fade_samples;
    }

    // calculate raw output
    signal_t curr = mult * ch.buffer[max_bufsiz_ * playback_buffer_ + playback_csr_];

    // move cursor
    playback_csr_ += 1;


    // calculate threshold - moving slightly to correct for input level
    if (fabs(in) > thresh) {
        thresh = thresh * 0.9 + 0.1 * fabs(in);
    } else {
        thresh = thresh * 0.98 + 0.02 * fabs(in);
    }

    if (thresh > max(threshold_ / 100, max_thresh_ * threshold_ / 65)) {
        target_gain = 1;
    } else {
        target_gain = 1 - (mix_ / 100);
    }
    max_thresh = (thresh > max_thresh) ? max(max_thresh, max_thresh * 0.1 + thresh * 0.9) : max_thresh * 0.99998;

    // output mix
    if (target_gain > gain) {
        gain = gain * 0.95 + 0.05 * target_gain;
    } else {
        gain = gain * (1 - attack_ / 100000) + attack_ / 100000 * target_gain;
    }
    last_gain = gain;

    // output
    in = in * gain + curr * (1 - gain);
}
// Per-channel processing.

Plugin * DISTRHO::createPlugin() {
    return new AvocadoPlugin();
}
