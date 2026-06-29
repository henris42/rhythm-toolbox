#pragma once
//
// input: decode the rhythm control surface into events.
//
//   - 20 buttons S1..S20    -> ButtonPressed (press edge only, no release)
//   - 2 8-position rotaries -> RotaryChanged (absolute position 0..7 on change)
//
// The bus read itself is owned by the IO engine (io.cpp); this module just
// decodes a fresh input buffer. IDs are the PCB labels. See docs/io-map.md.
//
#include <cstdint>

enum class InputEventType : uint8_t {
    ButtonPressed,   // a button went down
    RotaryChanged,   // a rotary switch moved to a new position
    SwitchChanged,   // an on/off toggle switch changed
    PotChanged,      // a potentiometer moved (filtered)
    StartChanged,    // start-in line changed (value = asserted, active low)
    // (sync-in is a 24-PPQN clock handled by a GPIO IRQ; see io_sync_ticks())
};

struct InputEvent {
    InputEventType type;
    uint8_t  id;      // Button 1..20 (S), Rotary 1..2 (R), Switch 1..2, Pot 1..2
    uint16_t value;   // Rotary 0..7; Switch/Sync/Start 0/1; Pot 0..4095; 0 for buttons
};

using InputHandler = void (*)(const InputEvent &);

// Reset decode state and configure the B1/B9 (S1/S9) patch GPIOs.
void input_init();

// Current debounced held state of a button (id 1..20). For hold-modifiers
// (e.g. accent / fill) where the press-only event isn't enough.
bool input_held(uint8_t id);

// Decode a fresh input read (>= NUM_INPUT_BYTES) into events. Debounce is
// time-based, so call it at whatever rate the engine ticks. The first call
// establishes the baseline silently (no events). Safe to call from the timer
// ISR; `handler` runs there too, so keep it short (e.g. enqueue).
void input_decode(const uint8_t *buf, InputHandler handler);
