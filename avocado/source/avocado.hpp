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

#ifndef AVOCADO_HPP
#define AVOCADO_HPP

#include "DistrhoPlugin.hpp"
#include "util.hpp"
#include "math.h"

typedef int samples_t; // integral sample length or position
typedef float samples_frac_t; // fractional sample length or position
typedef float signal_t; // signal value

const float PI = 3.141592653589793;

const int MAX_BUFFERS = 16;
const int MAX_BUFLEN = 48000 * 1.0;
const int FADE_SAMPLES = 32;


const int NUM_PROGRAMS = 1;

class AvocadoPlugin : public Plugin {
public:

    enum Parameters {
        PARAM_DRY_DB,
        PARAM_WET_DB,
        PARAM_COUNT
    };

    struct Channel {
    public:
        Channel() {
            memset(buffer, 0, MAX_BUFFERS * MAX_BUFLEN * sizeof(signal_t));
        }
        signal_t buffer[MAX_BUFFERS][MAX_BUFLEN] = {0};

        void tick() {
            //
        }
    };

    /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching the value in initParameter().
     */
    AvocadoPlugin() : Plugin(PARAM_COUNT, NUM_PROGRAMS, 0) {
        srate = getSampleRate();
        loadProgram(0);
    };

protected:

    void initProgramName(uint32_t index, String& programName) override;

    void loadProgram(uint32_t index) override;


    // -------------------------------------------------------------------
    // Information

    /**
      Get the plugin label.
      This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
     */
    const char* getLabel() const noexcept override {
        return "Avocado";
    }

    /**
      Get an extensive comment/description about the plugin.
      Optional, returns nothing by default.
     */
    const char* getDescription() const noexcept override {
        return "Avocado glitcher.";
    }

    /**
      Get the plugin author/maker.
     */
    const char* getMaker() const noexcept override {
        return "remaincalm.org";
    }

    /**
      Get the plugin license (a single line of text or a URL).
      For commercial plugins this should return some short copyright information.
     */
    const char* getLicense() const noexcept override {
        return "LGPL3";
    }

    /**
      Get the plugin version, in hexadecimal.
      @see d_version()
     */
    uint32_t getVersion() const noexcept override {
        return d_version(1, 0, 0);
    }

    /**
      Get the plugin unique Id.
      This value is used by LADSPA, DSSI and VST plugin formats.
      @see d_cconst()
     */
    int64_t getUniqueId() const noexcept override {
        return d_cconst('r', 'c', 'A', 'v');
    }

    // -------------------------------------------------------------------
    // Init

    /**
      Initialize the parameter @a index.
      This function will be called once, shortly after the plugin is created.
     */
    void initParameter(uint32_t index, Parameter& parameter) override;



    // -------------------------------------------------------------------
    // Internal data

    /**
      Get the current value of a parameter.
      The host may call this function from any context, including realtime processing.
     */
    float getParameterValue(uint32_t index) const override;

    /**
      Change a parameter value.
      The host may call this function from any context, including realtime processing.
      When a parameter is marked as automatable, you must ensure no non-realtime operations are performed.
     */
    void setParameterValue(uint32_t index, float value) override;

    /**
      Run/process function for plugins without MIDI input.
     */
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    signal_t process(Channel& ch, const signal_t in);
    void record(Channel& ch, const signal_t in);
    signal_t playback(Channel& ch, const signal_t in);
    float gate(Channel& ch, const signal_t in);

    Channel left_;

    // params    
    // gain
    const float gain_db_ = 6.0;
    SmoothParam<float> dry_out_db_ = 0.4;
    SmoothParam<float> wet_out_db_ = 0.4;

    int buffer_count_ = 0;
    int max_bufsiz_ = 0;
    int bufsiz_ = 0;
    int record_csr_ = 0;
    int record_buffer_ = 0;
    int playback_buffer_ = 0;
    int playback_csr_ = 0;
    bool is_recording_ = false;
    

    float repeat_probability_;
    float fadeout_probability_;
    float max_thresh_ = 0;
    float threshold_ = 0;
    //
    float thresh = 0;
    float max_thresh = 0;
    float target_gain = 0;
    float gain = 0;
    float last_gain = 0;
    float attack_ = 0;
    float mix_ = 0;

    // 
    samples_t srate;

    void tick() {
        dry_out_db_.tick();
        wet_out_db_.tick();
        left_.tick();
    }
};

#endif // AVOCADO_HPP
