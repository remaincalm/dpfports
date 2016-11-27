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

#ifndef FLOATY_HPP
#define FLOATY_HPP

#include "DistrhoPlugin.hpp"
#include "math.h"

#include "util.hpp"

typedef int samples_t; // integral sample length or position
typedef float samples_frac_t; // fractional sample length or position
typedef float signal_t; // signal value

const float PI = 3.141592653589793;
const samples_t MAX_BUF = 48000 * 1.2; // 1.2 seconds at 48kHz
const samples_frac_t SMOOTH_OVERLAP = 128.0f; // Smooth out if rec/play csr overlap.
const signal_t CLAMP = 0.6;

const int NUM_PROGRAMS = 1;

class FloatyPlugin : public Plugin {
public:

    enum Parameters {
        PARAM_DELAY_MS,
        PARAM_MIX,
        PARAM_FEEDBACK,
        PARAM_WARP,
        PARAM_FILTER,
        PARAM_PLAYBACK_RATE,
        PARAM_COUNT
    };

    struct Filter {
        SmoothParam<float> c = 0.3;
        SmoothParam<float> one_minus_rc = 0.98;

        void tick() {
            c.tick();
            one_minus_rc.tick();
        }
    };

    struct Channel {

        Channel() {
            setDelay(1000);
        }

        void setDelay(samples_t delay) {
            if (delay == 0) {
                return;
            }

            if (play_csr != 0) {
                memset(buf, 0, MAX_BUF * sizeof (signal_t));
            }

            this->delay = delay;
            play_csr = 0;
            rec_csr = getModPoint() - delay;
        }

        samples_t getModPoint() const {
            return fmin(MAX_BUF, delay * 2);
        }

        // tape state
        samples_t delay = 1;
        samples_t rec_csr = 0;
        samples_frac_t play_csr = 0;

        // tape buffer
        signal_t buf[MAX_BUF] = {};

        // filter state
        float v0 = 0;
        float v1 = 0;
        float hv0 = 0;
        float hv1 = 0;

        void tick() {
            //
        }
    };

    /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching the value in initParameter().
     */
    FloatyPlugin() : Plugin(PARAM_COUNT, NUM_PROGRAMS, 0) {
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
        return "Floaty";
    }

    /**
      Get an extensive comment/description about the plugin.
      Optional, returns nothing by default.
     */
    const char* getDescription() const noexcept override {
        return "Floaty delay.";
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
        return d_cconst('r', 'c', 'F', 'l');
    }

    // -------------------------------------------------------------------
    // Init

    /**
      Initialize the parameter @a index.
      This function will be called once, shortly after the plugin is created.
     */
    void initParameter(uint32_t index, Parameter& parameter) override;

    //
    void fixFilterParams();

    // 
    void fixDelayParams();

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
    void advancePlayHead(Channel& pc);
    void advanceRecHead(Channel& ch);
    signal_t fadeNearOverlap(const Channel& ch, const signal_t in) const;
    signal_t readFromPlayHead(const Channel& ch) const;
    signal_t saturate(const signal_t in) const;
    signal_t bandpassFilter(Channel& ch, const signal_t in);
    signal_t process(Channel& ch, const signal_t in);

    Channel left_;
    Channel right_;
    Filter lpf_;
    Filter hpf_;

    // params
    samples_t delay_ = (int) (120.0 * 48000.0 / 1000.0);
    SmoothParam<float> mix_ = 0.4;
    SmoothParam<float> feedback_ = 0.2;

    float warp_ = 49;

    float warp_rate_hz_ = 0.1;
    float warp_rate_rad_ = 2.0 * PI * 0.1 / 48000.0;
    SmoothParam<float> warp_amount_ = 0.01;

    float filter_ = 25;
    SmoothParam<float> filter_gain_ = 1.0;
    float filter_cutoff_ = 60;
    float filter_res_ = 40;
    SmoothParam<samples_frac_t> playback_rate_ = 1.0;
    float channel_offset_ = 98.0;
    float warp_counter_ = 0; // this will probably lose resolution and go weird

    samples_t srate = 48000;

    void tick() {
        mix_.tick();
        feedback_.tick();
        warp_amount_.tick();
        filter_gain_.tick();
        playback_rate_.tick();
        left_.tick();
        right_.tick();
        lpf_.tick();
        hpf_.tick();
    }
};

#endif // FLOATY_HPP
