#pragma once
//
// io: the hardware-timed IO engine. A 2 kHz repeating timer owns the shared
// shift-register bus; each tick it renders the LED PWM frame and exchanges it
// with a fresh input read in a single full-duplex transaction, then decodes
// inputs into events queued for the main loop.
//
// Use leds.h to drive the LEDs; call io_process() from the main loop to handle
// input events.
//
#include "input.h"

// Init the bus, inputs, LEDs, and start the 2 kHz tick.
void io_init();

// Drain queued input events, calling `handler` for each. Call from main loop.
void io_process(InputHandler handler);

// Current 24-PPQN external sync tick count (incremented in the sync GPIO IRQ).
uint32_t io_sync_ticks();

// Service the USB device stack. No-op unless TOOLBOX_USB_MIDI is on (then the
// app must call this often from its loop, since the SDK doesn't pump tud_task()
// when the app provides its own USB descriptors).
void io_usb_task();
