#pragma once
//
// Machine: the Drum Machine Toolbox framework. Owns the clock, sequencer, the
// default front-panel UI, trigger outputs and MIDI. A hacker's app just declares
// its voices, optionally sets a few things and hooks, then calls run().
//
#include "extio.h"        // ExtOut
#include "input.h"        // InputEvent
#include "sequencer.h"    // StepState
#include "clock.h"
#include <cstdint>

namespace dmt {

// One drum voice: a name, the trigger output it fires, and a MIDI note (0 = none).
struct Track {
    const char *name;
    ExtOut      out;
    uint8_t     note;
};

// Optional hook, called once per step with the step index (0..15).
using StepHook = void (*)(uint8_t step);

class Machine {
public:
    void set_tracks(const Track *tracks, uint8_t count);
    void set_tempo(float bpm);
    void set_pulse_us(uint32_t us);          // trigger pulse length (default 5 ms)
    void on_step(StepHook hook);

    // Default-UI LED brightness (perceptual 0..255) for on-step / accent /
    // playhead cursor. Defaults: 150 / 220 / 150.
    void set_led_levels(uint8_t on, uint8_t accent, uint8_t playhead);

    // Program a step of pattern 0 (for presets / demo beats).
    void set(uint8_t track, uint8_t step, StepState s);

    void run();   // init everything and loop forever

private:
    void handle(const InputEvent &e);
    void poll_midi();
    void fire_step(uint8_t step);
    void voice_fire(uint8_t track, bool accent, bool send_midi);
    void render_leds();
    int  held_config_option() const;   // which S1-S8 is held (0..7) or -1
    void apply_config(uint8_t opt);

    static void event_trampoline(const InputEvent &e);

    const Track *tracks_ = nullptr;
    uint8_t      ntracks_ = 0;
    Sequencer    seq_;
    Clock        clock_;
    StepHook     step_hook_ = nullptr;
    uint8_t      cur_pat_   = 0;   // "mode" rotary (0..6); pos 7 = config
    uint8_t      cur_track_ = 0;   // "editable" rotary
    uint8_t      playhead_  = 0;
    uint32_t     pulse_us_  = 5000;
    bool         config_mode_ = false;
    uint8_t      cfg_val_[8] = {3, 0, 0, 0, 0, 0, 0, 0};  // per-option value 0..7
    uint8_t      led_on_ = 150, led_accent_ = 220, led_playhead_ = 150;
};

}  // namespace dmt
