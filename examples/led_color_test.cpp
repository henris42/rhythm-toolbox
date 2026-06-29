// Definitive colour test. All 16 LEDs together, 3 s per phase, full brightness:
//   MODE 0: pure colour 1  (leds_set LED_COLOR1)  -> expect red
//   MODE 1: pure colour 2  (leds_set LED_COLOR2)  -> expect the other colour
//   MODE 2: 50/50 mix      (leds_set_mix 255,255) -> expect a blended hue
// The current mode is printed over USB serial so we can correlate.
#include <cstdio>
#include "io.h"
#include "leds.h"
#include "pico/stdlib.h"

int main() {
    io_init();
    int last = -1;
    while (true) {
        uint32_t t = to_ms_since_boot(get_absolute_time());
        int mode = (int)((t / 3000) % 3);
        if (mode != last) {
            last = mode;
            printf("MODE %d: %s\n", mode,
                   mode == 0 ? "pure COLOR1" :
                   mode == 1 ? "pure COLOR2" : "50/50 MIX");
            stdio_flush();
        }
        for (unsigned i = 0; i < NUM_LEDS; i++) {
            if (mode == 0)      leds_set(i, LED_COLOR1, 255);
            else if (mode == 1) leds_set(i, LED_COLOR2, 255);
            else                leds_set_mix(i, 255, 255);
        }
        leds_commit();
        io_usb_task();
        sleep_ms(2);
    }
}
