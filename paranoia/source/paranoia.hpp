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


#ifndef PARANOIA_HPP
#define PARANOIA_HPP

#include "DistrhoPlugin.hpp"
#include "util.hpp"
#include "math.h"


const samples_t RESAMPLE_MAX = 48000;

// waveshapes
const float PRE_SHAPER = 0.857;
const float POST_SHAPER = 0.9;
const float CLAMP = 0.9;

const int NUM_PROGRAMS = 6;

const int NUM_MANGLERS = 17;
const int MANGLER_BITDEPTH = 8;


// Bitcrusher.

class Mangler {
public:

    enum Mangle {
        X = -1, // invert
        O = 0, // off
        I = 1 // on
    };

    Mangler() {
        initMangler();
    };

    // works on samples in [0, 2^bitdepth) range

    int mangleForBitDepth(const int pattern_idx, int bitdepth, const int in) const {
        int clear_mask = clear_masks_[pattern_idx];
        int xor_mask = xor_masks_[pattern_idx];

        if (bitdepth < 8) {
            clear_mask >>= 8 - bitdepth;
        }
        if (bitdepth > 8) {
            clear_mask <<= bitdepth - 8;
        }

        int curr = in & ~clear_mask;
        curr = curr ^ xor_mask;
        return curr;
    }

    float relgain(const int pattern_idx) const {
        return relgain_[pattern_idx];
    }

private:

    void initMangler() {
        Mangle manglers[NUM_MANGLERS][MANGLER_BITDEPTH] = {
            { I, I, I, I, I, I, I, I},
            { I, I, I, I, I, I, I, O},
            { I, I, I, I, I, I, I, X}, //
            { I, I, I, I, I, I, O, I},
            { I, I, I, I, I, I, X, I},
            { I, I, I, I, I, O, O, I},
            { I, I, I, I, I, I, X, X},
            { I, I, I, X, I, I, I, I},
            { I, I, O, I, I, I, I, I},
            { O, X, I, O, X, O, X, I},
            { X, X, I, I, X, I, X, I},
            { O, O, O, O, I, O, O, O},
            { O, O, O, O, O, X, O, I},
            { O, O, I, I, O, O, X, I},
            { O, O, O, I, I, X, O, X},
            { O, O, O, O, I, I, X, X},
            { O, O, O, O, I, I, I, I},
        };


        for (int idx = 0; idx < NUM_MANGLERS; ++idx) {
            const auto pattern = manglers[idx];
            int clear_mask = 0;
            int xor_mask = 0;
            for (int i = 0; i < 8; ++i) {
                if (pattern[8 - i] == O) {
                    clear_mask = clear_mask | (int) pow(2, i);
                }
                if (pattern[8 - i] == X) {
                    xor_mask = xor_mask | (int) pow(2, i);
                }
            }
            clear_masks_[idx] = clear_mask;
            xor_masks_[idx] = xor_mask;
        }
    }

    int xor_masks_[NUM_MANGLERS];
    int clear_masks_[NUM_MANGLERS];

    // gain compensation for patterns in manglers
    float relgain_[NUM_MANGLERS] = {
        1.0, // 0
        1.0,
        1.0, // 2
        0.8,
        1.0, // 4
        0.8,
        1.0, // 6
        0.1,
        0.1, // 8
        0.3,
        0.1, // 10
        1.3,
        2.0, // 12
        0.2,
        0.8, // 14
        0.5,
        0.5 // 16
    };
};

class ParanoiaPlugin : public Plugin {
public:

    enum FilterMode {
        MODE_OFF = 0,
        MODE_LPF = 1,
        MODE_BANDPASS = 2,
        MODE_HPF = 3,
    };

    enum Parameters {
        PARAM_WET_DB,
        PARAM_CRUSH,
        PARAM_THERMONUCLEAR_WAR,
        PARAM_FILTER,
        PARAM_COUNT
    };

    struct Channel {
        // filter state
        float v0 = 0;
        float v1 = 0;
        float hv0 = 0;
        float hv1 = 0;

        // resampler state
        samples_frac_t next_sample = 0;
        signal_t prev_in = 0;
        long sample_csr = 0;

        // DC filter
        DcFilter dc_filter;

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
    ParanoiaPlugin() : Plugin(PARAM_COUNT, NUM_PROGRAMS, 0) {
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
        return "Paranoia";
    }

    /**
      Get an extensive comment/description about the plugin.
      Optional, returns nothing by default.
     */
    const char* getDescription() const noexcept override {
        return "Paranoia distortion/mangler.";
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
        return d_cconst('r', 'c', 'P', 'a');
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
    void fixCrushParams();
    void fixFilterParams();

    signal_t pregain(const Channel& ch, const signal_t in) const;
    signal_t resample(Channel& ch, const signal_t in) const;
    signal_t bitcrush(const signal_t in) const;
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
    const float gain_db_ = 6.0;
    SmoothParam<float> wet_out_db_ = 0.4;

    // filter
    float filter_ = 0;
    float filter_cutoff_ = 0;
    float filter_res_ = 0;
    SmoothParam<float> filter_gain_comp_ = 1.0;
    FilterMode filter_mode_ = MODE_BANDPASS;

    // resampler
    float crush_ = 95;
    samples_t resample_hz_ = 33000;
    SmoothParam<float> per_sample_ = 2;

    // bitcrusher
    int bitdepth_ = 10;
    SmoothParam<float> bitscale_ = 1;
    SmoothParam<float> nuclear_ = 0;
    Mangler mangler_;

    //
    samples_t srate;

    void tick() {
        wet_out_db_.tick();
        per_sample_.tick();
        filter_gain_comp_.tick();
        bitscale_.tick();
        nuclear_.tick();
        lpf_.tick();
        hpf_.tick();
    }
};

#endif // PARANOIA_HPP
