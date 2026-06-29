#include "machine.h"
#include "io.h"
#include "leds.h"
#include "extio.h"
#include "midi.h"
#include "pico/stdlib.h"

namespace dmt {

namespace {
Machine *s_self = nullptr;          // for the C-style io event handler
constexpr uint8_t kMidiDrumCh = 0x09;   // channel 10 (0-based 9)
constexpr uint8_t kAccentVel  = 127;
constexpr uint8_t kNormalVel  = 100;

constexpr uint8_t kFillPat = SEQ_PATTERNS - 1;   // last slot = fill pattern (S18)
constexpr uint8_t kConfigPos = 7;                // mode rotary position 7 = config

// Config options (hold a left button S1-S8 to pick, push a right button
// S9-S16 to set value 0..7). v1 defines one; the rest are reserved.
enum { CFG_PULSE = 0, NUM_CONFIG_OPTS = 1 };
const uint32_t kPulseTable[8] = {1000, 2000, 3000, 5000, 10000, 20000, 50000, 100000};
}  // namespace

void Machine::set_tracks(const Track *tracks, uint8_t count) {
    tracks_ = tracks;
    ntracks_ = count > SEQ_MAX_TRACKS ? SEQ_MAX_TRACKS : count;
}
void Machine::set_tempo(float bpm)      { clock_.set_tempo(bpm); }
void Machine::set_pulse_us(uint32_t us) { pulse_us_ = us; }
void Machine::on_step(StepHook hook)    { step_hook_ = hook; }
void Machine::set_led_levels(uint8_t on, uint8_t accent, uint8_t playhead) {
    led_on_ = on; led_accent_ = accent; led_playhead_ = playhead;
}
void Machine::set(uint8_t track, uint8_t step, StepState s) { seq_.set(0, track, step, s); }

void Machine::event_trampoline(const InputEvent &e) {
    if (s_self) s_self->handle(e);
}

void Machine::voice_fire(uint8_t track, bool accent, bool send_midi) {
    if (track >= ntracks_) return;
    extio_pulse(tracks_[track].out, pulse_us_);
    if (accent) extio_pulse(EXT_AC2, pulse_us_);          // shared accent bus
    if (send_midi && tracks_[track].note)
        midi_send(0x90 | kMidiDrumCh, tracks_[track].note,
                  accent ? kAccentVel : kNormalVel);
}

void Machine::fire_step(uint8_t step) {
    // S18 held = fill: play the dedicated fill pattern (last slot) instead.
    const uint8_t pat = input_held(18) ? kFillPat : cur_pat_;
    for (uint8_t t = 0; t < ntracks_; t++) {
        StepState st = seq_.get(pat, t, step);
        if (st != STEP_OFF) voice_fire(t, st == STEP_ACCENT, /*send_midi=*/true);
    }
}

void Machine::handle(const InputEvent &e) {
    switch (e.type) {
    case InputEventType::ButtonPressed: {
        if (e.id == 17) { clock_.toggle(); break; }        // S17 = start/stop (always)

        if (config_mode_) {                                 // hold S1-8 + push S9-16
            if (e.id >= 9 && e.id <= 16) {
                int opt = held_config_option();
                if (opt >= 0 && opt < NUM_CONFIG_OPTS) {
                    cfg_val_[opt] = e.id - 9;
                    apply_config(opt);
                }
            }
            break;
        }

        // Pattern edit. S18 held edits/auditions the fill pattern.
        uint8_t pat = input_held(18) ? kFillPat : cur_pat_;
        if (e.id >= 1 && e.id <= 16) {
            uint8_t s = e.id - 1;
            StepState cur = seq_.get(pat, cur_track_, s);
            if (input_held(19))                             // S19 = accent modifier
                seq_.set(pat, cur_track_, s, cur == STEP_ACCENT ? STEP_ON : STEP_ACCENT);
            else                                            // plain toggle on/off
                seq_.set(pat, cur_track_, s, cur == STEP_OFF ? STEP_ON : STEP_OFF);
        } else if (e.id == 20) {                            // S20 = clear track
            for (uint8_t s = 0; s < SEQ_STEPS; s++)
                seq_.set(pat, cur_track_, s, STEP_OFF);
        }
        break;
    }
    case InputEventType::RotaryChanged:
        if (e.id == 1) {                                    // mode rotary
            if (e.value == kConfigPos) config_mode_ = true;
            else { config_mode_ = false; cur_pat_ = e.value; }   // 0..6
        } else if (e.id == 2) {                             // editable track
            cur_track_ = (e.value < ntracks_) ? e.value : (ntracks_ ? ntracks_ - 1 : 0);
        }
        break;
    case InputEventType::SwitchChanged:
        if (e.id == 1) clock_.set_external(e.value);       // ext-sync
        break;
    case InputEventType::PotChanged:
        if (e.id == 2) clock_.set_tempo(40.0f + e.value * 200.0f / 4095.0f);  // A1
        else if (e.id == 1) clock_.set_swing(static_cast<uint8_t>(50 + e.value * 25 / 4095)); // A0
        break;
    case InputEventType::StartChanged:
        if (e.value) clock_.toggle();                      // external start in
        break;
    }
}

void Machine::poll_midi() {
    MidiMsg m;
    while (midi_read(&m)) {
        if ((m.status & 0xF0) == 0x90 && m.data2 > 0) {    // note on (vel > 0)
            for (uint8_t t = 0; t < ntracks_; t++)
                if (tracks_[t].note && tracks_[t].note == m.data1) {
                    voice_fire(t, m.data2 >= kAccentVel, /*send_midi=*/false);
                    break;
                }
        }
    }
}

int Machine::held_config_option() const {
    for (uint8_t i = 1; i <= 8; i++)
        if (input_held(i)) return i - 1;
    return -1;
}

void Machine::apply_config(uint8_t opt) {
    switch (opt) {
    case CFG_PULSE: pulse_us_ = kPulseTable[cfg_val_[CFG_PULSE] & 7]; break;
    default: break;   // reserved options
    }
}

void Machine::render_leds() {
    leds_clear();

    if (config_mode_) {
        // Left LEDs = config options (held one bright); right LEDs = its value.
        int held = held_config_option();
        for (uint8_t o = 0; o < 8; o++)
            if (o < NUM_CONFIG_OPTS)
                leds_set(o, (o == held) ? LED_COLOR2 : LED_COLOR1, (o == held) ? 255 : 70);
        if (held >= 0 && held < NUM_CONFIG_OPTS)
            for (uint8_t v = 0; v < 8; v++)
                leds_set(8 + v, LED_COLOR1, (v == cfg_val_[held]) ? 255 : 25);
        leds_commit();
        return;
    }

    // Pattern view (fill pattern while S18 held):
    //   on = colour 1, accent = blended colour (mix), playhead = colour 2 cursor.
    uint8_t pat = input_held(18) ? kFillPat : cur_pat_;
    for (uint8_t s = 0; s < SEQ_STEPS && s < NUM_LEDS; s++) {
        StepState st = seq_.get(pat, cur_track_, s);
        bool ph = (s == playhead_ && clock_.running());
        if (ph) {
            leds_set(s, LED_COLOR2, led_playhead_);              // playhead (colour 2)
        } else if (st == STEP_ACCENT) {
            leds_set_mix(s, led_accent_, led_accent_);           // accent = amber (mix)
        } else if (st == STEP_ON) {
            leds_set(s, LED_COLOR1, led_on_);                    // on (colour 1)
        }
    }
    leds_commit();
}

void Machine::run() {
    s_self = this;
    io_init();
    midi_init();
    clock_.init();
    clock_.start();

    while (true) {
        io_usb_task();                  // service USB (when USB MIDI is enabled)
        io_process(event_trampoline);   // buttons/rotaries/pots/switches/start
        poll_midi();                    // incoming notes -> fire voices

        uint8_t step;
        if (clock_.poll(&step)) {
            playhead_ = step;
            fire_step(step);
            if (step_hook_) step_hook_(step);
        }

        render_leds();
        sleep_us(250);                  // ~4 kHz loop; heavy IO is in the ISR
    }
}

}  // namespace dmt
