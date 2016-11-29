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

// all of these are structured badly.
// there should be a params struct we pass around.
// the channel should own its own processing.


#ifndef MUD_HPP
#define MUD_HPP

#include "DistrhoPlugin.hpp"
#include "util.hpp"
#include "math.h"

typedef int samples_t; // integral sample length or position
typedef float samples_frac_t; // fractional sample length or position
typedef float signal_t; // signal value

const float PI = 3.141592653589793;

// waveshapes
const float PRE_SHAPER = 0.4;
const float POST_SHAPER = 0.9;
const float CLAMP = 0.98;

const int NUM_PROGRAMS = 6;

// DC filter. Call process once per sample.

class DcFilter {
public:

    signal_t process(const signal_t in) {
        otm = 0.99 * otm + in - itm;
        itm = in;
        return otm;
    }

private:
    signal_t otm = 0;
    signal_t itm = 0;
};

class MudPlugin : public Plugin {
public:

    enum Parameters {
        PARAM_MIX,
        PARAM_FILTER,
        PARAM_LFO,
        PARAM_COUNT
    };

    struct Channel {
    public:

        // filter state
        float v0 = 0;
        float v1 = 0;
        float hv0 = 0;
        float hv1 = 0;

        // DC filter
        DcFilter dc_filter;

        void tick() {
            //
        }
    };

    struct Filter {
        SmoothParam<float> c = 0.3;
        SmoothParam<float> one_minus_rc = 0.98;

        void tick() {
            c.tick();
            one_minus_rc.tick();
        }
    };

    /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching the value in initParameter().
     */
    MudPlugin() : Plugin(PARAM_COUNT, NUM_PROGRAMS, 0) {
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
        return "Mud";
    }

    /**
      Get an extensive comment/description about the plugin.
      Optional, returns nothing by default.
     */
    const char* getDescription() const noexcept override {
        return
        "Mud modulation/filter\n"
        "\n"
        "Mix: direct/processed mix\n"
        "Filter: bandpass frequency/resonance\n"
        "LFO: speed/depth - left side is deep, right side is mellow";
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
        return d_cconst('r', 'c', 'M', 'u');
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
    void fixFilterParams();
    void fixLfoParams();

    signal_t preSaturate(const signal_t in) const;
    signal_t postSaturate(const signal_t in) const;
    signal_t filterDC(Channel& ch, const signal_t in) const;
    signal_t filterLPF(Channel& ch, const signal_t in) const;
    signal_t filterHPF(Channel& ch, const signal_t in) const;
    signal_t process(Channel& ch, const signal_t in);

    Channel left_;
    Filter lpf_;
    Filter hpf_;

    // params
    // gain
    SmoothParam<float> mix_ = 1.0;

    // LFO
    float lfo_ = 0;
    long lfo_counter_ = 0;
    float prv_filter_ = 0;

    // filter
    float filter_ = 0;
    float filter_cutoff_ = 0;
    float filter_res_ = 0;
    SmoothParam<float> filter_gain_comp_ = 1.0;

    //
    samples_t srate;

    void tick() {
        mix_.tick();
        lpf_.tick();
        hpf_.tick();
        left_.tick();
        filter_gain_comp_.tick();
    }
};

#endif // MUD_HPP
