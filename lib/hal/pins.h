#pragma once
//
// Board pin map for the rhythm IO stage (RP2350 / Pico 2).
//
// Shift-register IO over an SPI-style bus with a shared clock:
//   - Inputs:  2x 74HC165 (parallel-in -> serial-out)  = 16 input bits
//   - Outputs: 4x 74HC595 (serial-in  -> parallel-out)  = 32 output bits
//
// Pin / SPI-mux notes (RP2350):
//   GP6  SCLK  -> SPI0 SCK   (valid HW SPI0)        shared shift clock
//   GP16 MISO  -> SPI0 RX    (valid HW SPI0)        74165 Qh chain out
//   GP8  MOSI  -> SPI1 RX ONLY (NOT a HW-SPI TX!)   74595 serial in
//   GP15 LOAD  -> GPIO                              74165 SH/LD (active low)
//   GP28 SS    -> GPIO                              74595 RCLK (latch pulse)
//
// Driven by PIO (see shiftio.*), not hardware SPI: GP8 (MOSI) is not a
// hardware-SPI TX pin, and PIO can use any GPIO. One state machine clocks
// SCLK while shifting MOSI out and MISO in simultaneously.

// ---- Shared shift-register bus ----
#define PIN_SCLK      6    // shared shift clock (buffered, non-inverting)
#define PIN_MISO      16   // 74165 serial out
#define PIN_MOSI      8    // 74595 serial in
// Shift clock rate. Bus rated ~16-20 MHz; testing 16 MHz.
#define SHIFT_CLK_HZ  16000000

// ---- 74165 input chain ----
#define PIN_LOAD      15   // SH/LD, active low: pulse low to latch parallel inputs
#define NUM_165       2    // number of 74165s
#define NUM_INPUT_BYTES NUM_165   // 8 bits each -> 16 input bits

// ---- 74595 output chain ----
#define PIN_SS        28   // RCLK latch
#define NUM_595       4    // 32 output bits

// Bus transfer length: the input (74165) and output (74595) chains share the
// clock, so each tick clocks max(in, out) bytes. Input is valid in the first
// IO_IN_BYTES; the 74595s keep the last IO_OUT_BYTES clocked.
#define IO_IN_BYTES   NUM_INPUT_BYTES
#define IO_OUT_BYTES  NUM_595
#define IO_XFER_BYTES (IO_IN_BYTES > IO_OUT_BYTES ? IO_IN_BYTES : IO_OUT_BYTES)

// ---- 16 buttons B0..B15 ----
// B1..B7 and B9..B15 come from two 8-way priority encoders; their 3-bit codes
// sit in the last shift byte buf[1]. The encoder idle code is 7 (= no key),
// which collides with its index-0 key, so B0/B8 are bodge-wired to GPIO
// (active low, internal pull-up).
#define PIN_B0          3    // GP3 (patch)
#define PIN_B8          4    // GP4 (patch)
#define BTN_G1_SHIFT    2    // buf[1] bits 2..4 -> B1..B7
#define BTN_G2_SHIFT    5    // buf[1] bits 5..7 -> B9..B15
#define BTN_FIELD_MASK  0x7
#define BTN_ENC_IDLE    7    // no key pressed in that group
// S17..S20: 4 buttons wired straight to the remaining shift-register bits
// (buf[0] bits 6,7 and buf[1] bits 0,1), active high. See kDirect[] in input.cpp.
#define NUM_BUTTONS     20

// ---- 2 rotary encoders ----
// 3-bit value each, in the first shift byte buf[0].
#define ROT1_SHIFT      0    // buf[0] bits 0..2
#define ROT2_SHIFT      3    // buf[0] bits 3..5
#define ROT_FIELD_MASK  0x7
#define NUM_ROTARY      2

// ---- 2 on/off toggle switches (direct GPIO, active low, internal pull-up) ----
#define PIN_SW1         10   // ext sync (bool)
#define PIN_SW2         11   // variation (bool)
#define NUM_SWITCH      2

// ---- 2 potentiometers on ADC (A0 = GP26/ADC0, A1 = GP27/ADC1) ----
#define PIN_POT1        26   // ADC channel 0  (A0, shuffle)
#define PIN_POT2        27   // ADC channel 1  (A1, tempo)
#define NUM_POT         2

// ---- MIDI on UART0, 31250 baud ----
// Hardware UART0 is fixed: GP0 = TX, GP1 = RX. Bodge MIDI-OUT to GP0 and
// MIDI-IN to GP1 to match.
#define PIN_MIDI_TX     0
#define PIN_MIDI_RX     1

// ---- Direct-GPIO sync/trigger IO (NOT on the shift bus) ----
// Inputs:
#define PIN_SYNC_IN     12   // sync in
#define PIN_START       13   // start in
// Gate/trigger outputs O1..O7:
#define PIN_O1          22
#define PIN_O2          21
#define PIN_O3          20
#define PIN_O4          19
#define PIN_O5          18
#define PIN_O6          17
#define PIN_O7          14   // (GP15 was first asked, but that's PIN_LOAD)
// AC2/AC3 -- outputs (function TBD):
#define PIN_AC2         9
#define PIN_AC3         2
