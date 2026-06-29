#pragma once
//
// leds: 16 two-wire (antiparallel) bicolor LEDs on the 74595 chain, 2 bits per
// LED = the two terminals (01 -> colour 1, 10 -> colour 2, 00/11 -> off).
//
// A 2-wire LED can only show one colour at a time, so a "mix" is temporal: the
// engine gives some of the PWM frame to colour 1 and some to colour 2, and the
// fast 16 kHz refresh (125 Hz frame) fuses them into a blend. Brightness is
// perceptual (0..255), gamma-corrected to PWM duty.
//
// Double-buffered: leds_set*/leds_clear() stage into a back buffer; leds_commit()
// publishes atomically so the render ISR never sees a torn frame.
//
#include <cstdint>

inline constexpr unsigned NUM_LEDS      = 16;
inline constexpr uint8_t  LED_PWM_STEPS = 128;  // PWM duty resolution / phases

enum LedColor : uint8_t {
    LED_OFF    = 0,
    LED_COLOR1 = 1,   // pair = 01 (confirmed red)
    LED_COLOR2 = 2,   // pair = 10 (the other colour)
};

// Build the gamma lookup table. Call once before use (io_init does this).
void leds_init();

// Discrete colour (off / colour1 / colour2) at perceptual brightness 0..255.
void leds_set(unsigned led, LedColor color, uint8_t brightness);

// Mixed colour: independent perceptual intensity per colour (0..255), time-
// multiplexed into a blend. The two on-times share one frame, so at full mix
// each colour gets ~half the frame (the brightest blend is ~half-bright/colour).
void leds_set_mix(unsigned led, uint8_t c1, uint8_t c2);

// Convenience hue blend: 0 = colour1 .. 255 = colour2, at the given brightness
// (e.g. red -> amber -> green for a red/green LED).
void leds_set_blend(unsigned led, uint8_t blend, uint8_t brightness);

void leds_clear();

// Publish the staged frame to the render ISR (atomic).
void leds_commit();

// Render the current PWM phase of the published frame into NUM_595 output bytes.
void leds_render(uint8_t phase, uint8_t *out);
