#include "extio.h"
#include "pins.h"
#include "pico/stdlib.h"

namespace {
const uint8_t kPins[NUM_EXT_OUT] = {
    PIN_O1, PIN_O2, PIN_O3, PIN_O4, PIN_O5, PIN_O6, PIN_O7, PIN_AC2, PIN_AC3,
};

// Per-output pulse state. 32-bit us deadline (atomic on RP2350); the bool says
// whether a pulse is in flight. extio_pulse writes the deadline before setting
// the flag, so the tick ISR only reads a valid deadline.
uint32_t      s_pulse_end[NUM_EXT_OUT];
volatile bool s_pulsing[NUM_EXT_OUT];
}  // namespace

void extio_init() {
    for (int i = 0; i < NUM_EXT_OUT; i++) {
        gpio_init(kPins[i]);
        gpio_set_dir(kPins[i], GPIO_OUT);
        gpio_put(kPins[i], 0);   // idle low (active high)
        s_pulsing[i] = false;
    }
}

void extio_set(ExtOut o, bool on) {
    if (o >= NUM_EXT_OUT) return;
    s_pulsing[o] = false;        // a direct level cancels any pulse
    gpio_put(kPins[o], on);
}

void extio_pulse(ExtOut o, uint32_t len_us) {
    if (o >= NUM_EXT_OUT) return;
    s_pulse_end[o] = time_us_32() + len_us;
    gpio_put(kPins[o], 1);
    s_pulsing[o] = true;
}

void extio_update() {
    const uint32_t now = time_us_32();
    for (int i = 0; i < NUM_EXT_OUT; i++) {
        if (s_pulsing[i] && static_cast<int32_t>(now - s_pulse_end[i]) >= 0) {
            gpio_put(kPins[i], 0);
            s_pulsing[i] = false;
        }
    }
}
