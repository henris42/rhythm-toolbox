#include "io.h"
#include "input.h"
#include "leds.h"
#include "extio.h"
#include "shiftio.h"
#include "pins.h"
#include "pico/stdlib.h"
#if TOOLBOX_USB_MIDI
#include "tusb.h"
#endif

namespace {

// Tick = LED_PWM_STEPS * refresh. 128 * 125 Hz = 16 kHz. Inputs are scanned at
// the same rate (harmless; debounce is time-based).
constexpr int kTickHz = 16000;
repeating_timer_t s_timer;
volatile uint8_t s_phase = 0;   // PWM phase, 0..LED_PWM_STEPS-1

// SPSC event ring: produced in the timer ISR, consumed in io_process().
constexpr int QCAP = 64;
InputEvent        s_q[QCAP];
volatile uint16_t s_head = 0;   // write index (ISR)
volatile uint16_t s_tail = 0;   // read index (main)

void enqueue(const InputEvent &e) {
    uint16_t next = (uint16_t)((s_head + 1) % QCAP);
    if (next == s_tail) return;   // full: drop (shouldn't happen at 2 kHz)
    s_q[s_head] = e;
    s_head = next;
}

// 24-PPQN external sync clock: counted on the active (falling) edge of GP12.
// The sequencer reads this directly rather than going through the event queue.
volatile uint32_t s_sync_ticks = 0;

void gpio_irq(uint gpio, uint32_t /*events*/) {
    if (gpio == PIN_SYNC_IN) s_sync_ticks++;
}

bool on_tick(repeating_timer_t *) {
    // One full-duplex transaction of IO_XFER_BYTES. LED frame goes in the last
    // IO_OUT_BYTES (the 595s keep the last bits clocked); inputs come back in
    // the first IO_IN_BYTES.
    uint8_t out[IO_XFER_BYTES] = {0};
    leds_render(s_phase, out + (IO_XFER_BYTES - IO_OUT_BYTES));

    uint8_t in[IO_XFER_BYTES];
    shiftio_exchange(out, in, IO_XFER_BYTES);

    input_decode(in, enqueue);
    extio_update();   // clear any expired output pulses

    uint8_t p = (uint8_t)(s_phase + 1);
    s_phase = (p >= LED_PWM_STEPS) ? 0 : p;
    return true;   // keep repeating
}

}  // namespace

void io_init() {
    // Bring up USB CDC: gives the hacker printf debugging and, crucially, keeps
    // picotool's reset-to-BOOTSEL interface alive for the next flash.
    stdio_init_all();

    shiftio_init(pio0, 0);
    input_init();
    extio_init();
    leds_init();
    leds_clear();
    leds_commit();

    // sync-in (24 PPQN), active low: count falling edges via GPIO IRQ.
    gpio_init(PIN_SYNC_IN);
    gpio_set_dir(PIN_SYNC_IN, GPIO_IN);
    gpio_pull_up(PIN_SYNC_IN);
    gpio_set_irq_enabled_with_callback(PIN_SYNC_IN, GPIO_IRQ_EDGE_FALL, true,
                                       &gpio_irq);
    // Negative period => fixed 2 kHz cadence measured from each start, so a
    // slightly long callback doesn't drift the rate.
    add_repeating_timer_us(-1000000 / kTickHz, on_tick, nullptr, &s_timer);
}

void io_process(InputHandler handler) {
    while (s_tail != s_head) {
        InputEvent e = s_q[s_tail];
        s_tail = (uint16_t)((s_tail + 1) % QCAP);
        handler(e);
    }
}

uint32_t io_sync_ticks() {
    return s_sync_ticks;
}

void io_usb_task() {
#if TOOLBOX_USB_MIDI
    tud_task();
#endif
}
