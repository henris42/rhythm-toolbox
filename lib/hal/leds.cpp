#include "leds.h"
#include "pins.h"
#include <cmath>
#include <cstring>

namespace {

// Double buffer: the ISR renders s_buf[s_front]; the main loop stages edits into
// s_buf[s_front ^ 1] and leds_commit() flips s_front (a single atomic write).
// Per LED: [0] = colour-1 PWM duty, [1] = colour-2 PWM duty (each 0..STEPS, and
// their sum is capped at STEPS so the two on-times fit in one frame).
uint8_t      s_buf[2][NUM_LEDS][2];
volatile int s_front = 0;

// Perceptual brightness (0..255) -> PWM duty (0..LED_PWM_STEPS) via gamma.
uint8_t s_gamma[256];
constexpr float kGamma = 2.2f;

// Map (led, terminal) -> position in the shift stream (0 = first bit clocked =
// out[0] MSB). The chain runs in reverse (led 0 lights the far LED). Terminal 0
// is the "01" bit (colour 1), terminal 1 the "10" bit (colour 2).
uint8_t led_bit(unsigned led, uint8_t terminal) {
    return (uint8_t)((NUM_LEDS - 1 - led) * 2 + terminal);
}

// Stage a LED's two duties into the back buffer, capping the total to one frame
// (a 2-wire LED can't drive both terminals at once).
void stage(unsigned led, uint8_t d1, uint8_t d2) {
    if (led >= NUM_LEDS) return;
    int total = (int)d1 + (int)d2;
    if (total > LED_PWM_STEPS) {                 // scale down, keep the hue ratio
        d1 = (uint8_t)((int)d1 * LED_PWM_STEPS / total);
        d2 = (uint8_t)((int)d2 * LED_PWM_STEPS / total);
    }
    uint8_t (*bk)[2] = s_buf[s_front ^ 1];
    bk[led][0] = d1;
    bk[led][1] = d2;
}

}  // namespace

void leds_init() {
    for (int i = 0; i < 256; i++) {
        float n = powf((float)i / 255.0f, kGamma);
        s_gamma[i] = (uint8_t)(n * LED_PWM_STEPS + 0.5f);
    }
}

void leds_set(unsigned led, LedColor color, uint8_t brightness) {
    uint8_t d = s_gamma[brightness];
    if (color == LED_COLOR1)      stage(led, d, 0);
    else if (color == LED_COLOR2) stage(led, 0, d);
    else                          stage(led, 0, 0);
}

void leds_set_mix(unsigned led, uint8_t c1, uint8_t c2) {
    stage(led, s_gamma[c1], s_gamma[c2]);
}

void leds_set_blend(unsigned led, uint8_t blend, uint8_t brightness) {
    uint8_t c1 = (uint8_t)((uint16_t)brightness * (255 - blend) / 255);
    uint8_t c2 = (uint8_t)((uint16_t)brightness * blend / 255);
    leds_set_mix(led, c1, c2);
}

void leds_clear() {
    memset(s_buf[s_front ^ 1], 0, NUM_LEDS * 2);
}

void leds_commit() {
    int pub = s_front ^ 1;
    s_front = pub;                                    // publish (atomic)
    memcpy(s_buf[pub ^ 1], s_buf[pub], NUM_LEDS * 2); // keep back in sync
}

void leds_render(uint8_t phase, uint8_t *out) {
    const uint8_t (*fr)[2] = s_buf[s_front];

    // Time-multiplex the two terminals: colour 1 for the first c1d phases, then
    // colour 2 for the next c2d phases, else off. At most one terminal high per
    // phase (never 11). 595 outputs are active high; packed MSB first.
    for (int i = 0; i < NUM_595; i++) out[i] = 0;
    for (unsigned led = 0; led < NUM_LEDS; led++) {
        uint8_t c1d = fr[led][0];
        uint8_t c2d = fr[led][1];
        uint8_t terminal;
        if (phase < c1d)                terminal = 0;
        else if (phase < c1d + c2d)     terminal = 1;
        else                            continue;     // off this phase
        uint8_t b = led_bit(led, terminal);
        out[b >> 3] |= (uint8_t)(1u << (7 - (b & 7)));
    }
}
