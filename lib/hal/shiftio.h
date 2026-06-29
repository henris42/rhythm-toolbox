#pragma once
//
// shiftio: PIO-driven shift-register IO for the rhythm board.
//
// One PIO state machine clocks the shared SCLK while shifting MOSI out (74595
// outputs) and MISO in (74165 inputs) simultaneously. The 74165 LOAD and 74595
// latch (SS/RCLK) are driven as plain GPIO around each transfer.
//
// PIO is used because the board's MOSI pin (GP8) is not a hardware-SPI TX pin.
//
#include "hardware/pio.h"
#include <cstdint>
#include <cstddef>

// Claim and start the state machine; configure SCLK/MOSI/MISO and the
// LOAD/SS control GPIOs.
void shiftio_init(PIO pio, uint sm);

// Latch the parallel inputs (pulse 74165 LOAD) and clock `n` bytes in.
// dst[0] is the first byte clocked -> the 74165 nearest MISO. Outputs are
// undisturbed (the 74595 latch is not pulsed).
void shiftio_read_inputs(uint8_t *dst, size_t n);

// Clock `n` bytes out to the 74595 chain, then pulse the latch (SS/RCLK) to
// present them. Inputs read during the shift are discarded.
void shiftio_write_outputs(const uint8_t *src, size_t n);

// Combined full-duplex tick: pulse LOAD (latch 74165 inputs), exchange n bytes
// (out -> 74595, in <- 74165 in the first 16 bits), then pulse SS (latch the
// 74595 outputs). ISR-safe (busy-waits, no sleeps). The IO engine uses this to
// read inputs and write the LED frame in a single bus transaction.
void shiftio_exchange(const uint8_t *out, uint8_t *in, size_t n);
