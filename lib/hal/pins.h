#pragma once
//
// Board pin map for the Rhythm Toolbox IO stage (RP2350 / Pico 2).
//
// Shift-register IO over an SPI-style bus with a shared clock:
//   - Inputs:  2× 74HC165 (parallel-in -> serial-out)  = 16 input bits
//   - Outputs: 4× 74HC595 (serial-in  -> parallel-out)  = 32 output bits
//
// Driven by PIO (see shiftio.*), not hardware SPI — one state machine clocks
// SCLK while shifting MOSI out (74595) and MISO in (74165) simultaneously.
//
#include <cstdint>

// ---- Shared shift-register bus ----
inline constexpr unsigned PIN_SCLK = 6;   // shared shift clock (buffered, non-inverting)
inline constexpr unsigned PIN_MISO = 16;  // 74165 serial out
inline constexpr unsigned PIN_MOSI = 8;   // 74595 serial in
inline constexpr uint32_t SHIFT_CLK_HZ = 16'000'000;  // bus rated ~16–20 MHz

// ---- 74165 input chain ----
inline constexpr unsigned PIN_LOAD = 15;  // SH/LD̄, active low: pulse low to latch inputs
inline constexpr uint8_t  NUM_165  = 2;
inline constexpr uint8_t  NUM_INPUT_BYTES = NUM_165;  // 8 bits each -> 16 input bits

// ---- 74595 output chain ----
inline constexpr unsigned PIN_SS  = 28;   // RCLK latch
inline constexpr uint8_t  NUM_595 = 4;    // 32 output bits

// Bus transfer length: the 74165 and 74595 chains share the clock, so each tick
// clocks max(in, out) bytes. Input is valid in the first IO_IN_BYTES; the 74595s
// keep the last IO_OUT_BYTES clocked.
inline constexpr uint8_t IO_IN_BYTES   = NUM_INPUT_BYTES;
inline constexpr uint8_t IO_OUT_BYTES  = NUM_595;
inline constexpr uint8_t IO_XFER_BYTES = (IO_IN_BYTES > IO_OUT_BYTES) ? IO_IN_BYTES : IO_OUT_BYTES;

// ---- 20 buttons S1..S20 ----
// S1..S16 come from two 8-way priority encoders whose 3-bit codes sit in the
// last shift byte. The idle code (7) aliases each encoder's index-0 key, so
// those two keys (S1, S9) are on dedicated GPIO instead.
inline constexpr unsigned PIN_B0 = 3;     // S1 (patch)
inline constexpr unsigned PIN_B8 = 4;     // S9 (patch)
inline constexpr uint8_t  BTN_G1_SHIFT  = 2;    // buf[1] bits 2..4 -> S2..S8
inline constexpr uint8_t  BTN_G2_SHIFT  = 5;    // buf[1] bits 5..7 -> S10..S16
inline constexpr uint8_t  BTN_FIELD_MASK = 0x7;
inline constexpr uint8_t  BTN_ENC_IDLE   = 7;   // no key in that group
inline constexpr uint8_t  NUM_BUTTONS    = 20;  // S17..S20 are direct shift bits

// ---- 2 rotary switches (3-bit absolute position each, in the first shift byte) ----
inline constexpr uint8_t ROT1_SHIFT     = 0;    // buf[0] bits 0..2
inline constexpr uint8_t ROT2_SHIFT     = 3;    // buf[0] bits 3..5
inline constexpr uint8_t ROT_FIELD_MASK = 0x7;
inline constexpr uint8_t NUM_ROTARY     = 2;

// ---- 2 on/off toggle switches (direct GPIO, active low, internal pull-up) ----
inline constexpr unsigned PIN_SW1 = 10;   // ext sync
inline constexpr unsigned PIN_SW2 = 11;   // variation
inline constexpr uint8_t  NUM_SWITCH = 2;

// ---- 2 potentiometers on ADC (A0 = GP26/ADC0, A1 = GP27/ADC1) ----
inline constexpr unsigned PIN_POT1 = 26;  // ADC0 — shuffle
inline constexpr unsigned PIN_POT2 = 27;  // ADC1 — tempo
inline constexpr uint8_t  NUM_POT  = 2;

// ---- MIDI on UART0, 31250 baud (GP0 = TX, GP1 = RX) ----
inline constexpr unsigned PIN_MIDI_TX = 0;
inline constexpr unsigned PIN_MIDI_RX = 1;

// ---- Direct-GPIO sync/trigger IO (not on the shift bus) ----
inline constexpr unsigned PIN_SYNC_IN = 12;   // 24-PPQN clock in
inline constexpr unsigned PIN_START   = 13;   // start in
inline constexpr unsigned PIN_O1 = 22;
inline constexpr unsigned PIN_O2 = 21;
inline constexpr unsigned PIN_O3 = 20;
inline constexpr unsigned PIN_O4 = 19;
inline constexpr unsigned PIN_O5 = 18;
inline constexpr unsigned PIN_O6 = 17;
inline constexpr unsigned PIN_O7 = 14;
inline constexpr unsigned PIN_AC2 = 9;
inline constexpr unsigned PIN_AC3 = 2;
