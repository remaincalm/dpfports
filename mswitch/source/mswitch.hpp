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

#ifndef MSWITCH_HPP
#define MSWITCH_HPP

#include "DistrhoPlugin.hpp"
#include "util.hpp"
#include "math.h"

const int NUM_PROGRAMS = 1;

class MswitchPlugin : public Plugin {
public:

    enum Parameters {
        PARAM_P1,
        PARAM_P2,
        PARAM_COUNT
    };

    /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching the value in initParameter().
     */
    MswitchPlugin() : Plugin(PARAM_COUNT, NUM_PROGRAMS, 0) {
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
        return "Mswitch";
    }

    /**
      Get an extensive comment/description about the plugin.
      Optional, returns nothing by default.
     */
    const char* getDescription() const noexcept override {
        return "Mswitch switcher.";
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
        return d_cconst('r', 'c', 'M', 's');
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
    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* events, uint32_t eventCount) override;

private:

    bool state_ = false;
    uint8_t p1_ = 0;
    uint8_t p2_ = 0;
};

#endif // MSWITCH_HPP
