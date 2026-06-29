#include "input.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <cstdlib>
#include <cstring>

namespace {

// Map raw priority-encoder/patch position (0..15) -> PCB switch label S1..S16.
// Identity except routing swaps. Edit as more swaps turn up.
//   - S5/S6 (raw 4/5) swapped.
//   - S13..S16 reshuffled: raw 12->S16, 13->S15, 14->S13, 15->S14.
const uint8_t kSwitch[16] = {
    1, 2, 3, 4, 6, 5, 7, 8, 9, 10, 11, 12, 16, 15, 13, 14,
};

// 4 buttons wired straight to shift-register bits (no encoder), active low
// (external pull-ups: idle high, pressed pulls the bit low).
struct DirectBtn { uint8_t byte, bit; };
const DirectBtn kDirect[] = {
    {0, 6},   // S17  buf[0] bit 6
    {0, 7},   // S18  buf[0] bit 7
    {1, 1},   // S19  buf[1] bit 1 (swapped vs S20)
    {1, 0},   // S20  buf[1] bit 0
};
constexpr uint8_t kNumDirect = sizeof(kDirect) / sizeof(kDirect[0]);
constexpr bool kDirectButtonsEnabled = true;

// Debounce windows in real milliseconds (scan-rate independent).
constexpr uint32_t kBtnDebounceMs = 5;
constexpr uint32_t kRotDebounceMs = 100;
constexpr uint32_t kSwDebounceMs  = 10;

// Pot handling: subsample to ~1 kHz, EMA low-pass, then a deadband on the raw
// 12-bit ADC value. Reported as 12-bit (0..4095); the app maps to BPM/shuffle.
constexpr int      kPotEveryN   = 16;   // every 16 ticks (16 kHz -> 1 kHz)
constexpr int      kPotEmaShift = 5;    // EMA alpha = 1/32 (more smoothing)
constexpr int      kPotDeadband = 16;   // 12-bit counts; > noise band, < counts/BPM

const uint8_t kSwPins[NUM_SWITCH]  = {PIN_SW1, PIN_SW2};

// start-in: direct GPIO, active low, debounced. (sync-in is a 24-PPQN clock
// handled by a GPIO IRQ in io.cpp, not here.)
constexpr uint32_t kStartDebounceMs = 5;

bool     s_pressed[NUM_BUTTONS];
uint32_t s_btn_time[NUM_BUTTONS];
uint8_t  s_rotary[NUM_ROTARY];
uint8_t  s_rot_cand[NUM_ROTARY];
uint32_t s_rot_time[NUM_ROTARY];
bool     s_sw[NUM_SWITCH];
uint32_t s_sw_time[NUM_SWITCH];
bool     s_start;
uint32_t s_start_time;
uint32_t s_pot_acc[NUM_POT];     // EMA accumulator (filtered << kPotEmaShift)
uint16_t s_pot[NUM_POT];         // last emitted 12-bit value
uint32_t s_pot_div = 0;          // subsample counter
bool     s_primed = false;

int decode_group(uint8_t field, int base) {
    if (field == BTN_ENC_IDLE) return -1;
    return base + (7 - field);     // 74148-style inverted code, idle = 7
}

void read_buttons(const uint8_t *buf, bool *now) {
    memset(now, 0, NUM_BUTTONS * sizeof(bool));

    bool raw[16] = {false};
    raw[0] = (gpio_get(PIN_B0) == 0);   // patch, active low -> S1
    raw[8] = (gpio_get(PIN_B8) == 0);   // patch, active low -> S9
    int b1 = decode_group((buf[1] >> BTN_G1_SHIFT) & BTN_FIELD_MASK, 0);
    int b2 = decode_group((buf[1] >> BTN_G2_SHIFT) & BTN_FIELD_MASK, 8);
    if (b1 >= 0) raw[b1] = true;
    if (b2 >= 0) raw[b2] = true;
    for (int i = 0; i < 16; i++)
        if (raw[i]) now[kSwitch[i] - 1] = true;

    if (kDirectButtonsEnabled)                 // direct buttons, active low
        for (uint8_t i = 0; i < kNumDirect; i++)
            now[16 + i] = ((buf[kDirect[i].byte] >> kDirect[i].bit) & 1) == 0;
}

uint16_t pot_raw(uint8_t i) {
    adc_select_input(i);          // channel 0 = GP26, 1 = GP27
    return adc_read();            // 0..4095
}

}  // namespace

void input_init() {
    gpio_init(PIN_B0);
    gpio_set_dir(PIN_B0, GPIO_IN);
    gpio_pull_up(PIN_B0);
    gpio_init(PIN_B8);
    gpio_set_dir(PIN_B8, GPIO_IN);
    gpio_pull_up(PIN_B8);

    for (uint8_t i = 0; i < NUM_SWITCH; i++) {
        gpio_init(kSwPins[i]);
        gpio_set_dir(kSwPins[i], GPIO_IN);
        gpio_pull_up(kSwPins[i]);
    }
    gpio_init(PIN_START);
    gpio_set_dir(PIN_START, GPIO_IN);
    gpio_pull_up(PIN_START);

    adc_init();
    adc_gpio_init(PIN_POT1);
    adc_gpio_init(PIN_POT2);

    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_btn_time, 0, sizeof(s_btn_time));
    memset(s_rotary, 0, sizeof(s_rotary));
    memset(s_rot_cand, 0, sizeof(s_rot_cand));
    memset(s_rot_time, 0, sizeof(s_rot_time));
    memset(s_sw, 0, sizeof(s_sw));
    memset(s_sw_time, 0, sizeof(s_sw_time));
    s_start = false;
    s_start_time = 0;
    memset(s_pot_acc, 0, sizeof(s_pot_acc));
    memset(s_pot, 0, sizeof(s_pot));
    s_pot_div = 0;
    s_primed = false;
}

bool input_held(uint8_t id) {
    if (id < 1 || id > NUM_BUTTONS) return false;
    return s_pressed[id - 1];   // debounced current state (set in input_decode)
}

void input_decode(const uint8_t *buf, InputHandler handler) {
    bool now[NUM_BUTTONS];
    read_buttons(buf, now);

    uint8_t rot[NUM_ROTARY];
    rot[0] = (buf[0] >> ROT1_SHIFT) & ROT_FIELD_MASK;
    rot[1] = (buf[0] >> ROT2_SHIFT) & ROT_FIELD_MASK;

    bool sw[NUM_SWITCH];
    for (uint8_t i = 0; i < NUM_SWITCH; i++)
        sw[i] = (gpio_get(kSwPins[i]) == 0);   // active low

    bool start = (gpio_get(PIN_START) == 0);   // active low -> asserted

    uint32_t t = to_ms_since_boot(get_absolute_time());

    if (!s_primed) {
        memcpy(s_pressed, now, sizeof(s_pressed));
        memcpy(s_rotary, rot, sizeof(s_rotary));
        memcpy(s_rot_cand, rot, sizeof(s_rot_cand));
        memcpy(s_sw, sw, sizeof(s_sw));
        s_start = start;
        for (uint8_t i = 0; i < NUM_BUTTONS; i++) s_btn_time[i] = t;
        for (uint8_t i = 0; i < NUM_ROTARY; i++) s_rot_time[i] = t;
        for (uint8_t i = 0; i < NUM_SWITCH; i++) s_sw_time[i] = t;
        s_start_time = t;
        for (uint8_t i = 0; i < NUM_POT; i++) {
            uint16_t raw = pot_raw(i);
            s_pot_acc[i] = static_cast<uint32_t>(raw) << kPotEmaShift;   // seed EMA
            s_pot[i] = raw;
        }
        s_primed = true;
        return;
    }

    // Buttons: emit on the press edge, then lock out bounce (no release event).
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (now[i] != s_pressed[i] && (t - s_btn_time[i]) >= kBtnDebounceMs) {
            s_pressed[i] = now[i];
            s_btn_time[i] = t;
            if (now[i]) {
                InputEvent e{InputEventType::ButtonPressed, static_cast<uint8_t>(i + 1), 0};
                handler(e);
            }
        }
    }

    // Rotaries: accept a new position only after it holds for kRotDebounceMs.
    for (uint8_t i = 0; i < NUM_ROTARY; i++) {
        if (rot[i] == s_rotary[i]) {
            s_rot_cand[i] = rot[i];
        } else if (rot[i] == s_rot_cand[i]) {
            if ((t - s_rot_time[i]) >= kRotDebounceMs) {
                s_rotary[i] = rot[i];
                InputEvent e{InputEventType::RotaryChanged, static_cast<uint8_t>(i + 1), rot[i]};
                handler(e);
            }
        } else {
            s_rot_cand[i] = rot[i];
            s_rot_time[i] = t;
        }
    }

    // Switches: on/off toggles, debounced, both edges.
    for (uint8_t i = 0; i < NUM_SWITCH; i++) {
        if (sw[i] != s_sw[i] && (t - s_sw_time[i]) >= kSwDebounceMs) {
            s_sw[i] = sw[i];
            s_sw_time[i] = t;
            InputEvent e{InputEventType::SwitchChanged, static_cast<uint8_t>(i + 1),
                         static_cast<uint16_t>(sw[i] ? 1 : 0)};
            handler(e);
        }
    }

    // Start: active-low signal, debounced, both edges.
    if (start != s_start && (t - s_start_time) >= kStartDebounceMs) {
        s_start = start;
        s_start_time = t;
        InputEvent e{InputEventType::StartChanged, 1, static_cast<uint16_t>(start ? 1 : 0)};
        handler(e);
    }

    // Pots: subsample, EMA low-pass, then 8-bit deadband.
    if (++s_pot_div >= kPotEveryN) {
        s_pot_div = 0;
        for (uint8_t i = 0; i < NUM_POT; i++) {
            uint16_t raw = pot_raw(i);
            // EMA: acc holds (filtered << shift). Written as -decay +raw to
            // avoid unsigned underflow when raw < the filtered value.
            s_pot_acc[i] = s_pot_acc[i] - (s_pot_acc[i] >> kPotEmaShift) + raw;
            const uint16_t v = static_cast<uint16_t>(s_pot_acc[i] >> kPotEmaShift);   // 12-bit
            if (std::abs(static_cast<int>(v) - static_cast<int>(s_pot[i])) >= kPotDeadband) {
                s_pot[i] = v;
                InputEvent e{InputEventType::PotChanged, static_cast<uint8_t>(i + 1), v};
                handler(e);
            }
        }
    }
}
