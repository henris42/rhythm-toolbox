#pragma once
//
// midi: MIDI in/out on UART0 (31250 baud). Hardware UART0 pins: GP0 = TX, GP1 = RX.
// Simple running-status parser for input; helpers for output.
//
#include <cstdint>

struct MidiMsg {
    uint8_t status;   // full status byte (incl. channel), or a real-time byte
    uint8_t data1;
    uint8_t data2;    // 0 for 1-data / real-time messages
};

void midi_init();

// Poll the UART; returns true and fills `msg` when a complete message parsed.
// Call repeatedly until it returns false.
bool midi_read(MidiMsg *msg);

void midi_send(uint8_t status, uint8_t d1, uint8_t d2);  // 3-byte channel msg
void midi_send_rt(uint8_t rt);                           // single real-time byte
