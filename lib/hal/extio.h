#pragma once
//
// extio: direct-GPIO outputs (not on the shift bus) — gate/trigger outputs
// O1..O7 and AC2/AC3. Active high (high = on); idle low.
//
// Pulses: extio_pulse() drives a pin high and auto-clears it after a
// configurable length. extio_update() (called by the IO engine each tick)
// clears expired pulses, so timing resolution is the tick period (~62 us).
//
#include <cstdint>

enum ExtOut : uint8_t {
    EXT_O1, EXT_O2, EXT_O3, EXT_O4, EXT_O5, EXT_O6, EXT_O7,
    EXT_AC2, EXT_AC3,
    NUM_EXT_OUT,
};

void extio_init();
void extio_set(ExtOut o, bool on);            // direct level (cancels any pulse)
void extio_pulse(ExtOut o, uint32_t len_us);  // high now, auto-clear after len_us
void extio_update();                          // clear expired pulses (call each tick)
