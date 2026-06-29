#include "shiftio.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "shiftio.pio.h"

static PIO s_pio;
static uint s_sm;

// Full-duplex byte transfer. tx/rx may be null (null tx sends 0x00; null rx
// discards). Bytes are pushed left-justified (<<24) so the MSB-first program
// shifts the real 8 bits; received bytes land in the low 8 bits.
static void transfer(const uint8_t *tx, uint8_t *rx, size_t n) {
    size_t tx_rem = n, rx_rem = n;
    while (tx_rem || rx_rem) {
        if (tx_rem && !pio_sm_is_tx_fifo_full(s_pio, s_sm)) {
            uint32_t b = tx ? *tx++ : 0u;
            pio_sm_put(s_pio, s_sm, b << 24);
            tx_rem--;
        }
        if (rx_rem && !pio_sm_is_rx_fifo_empty(s_pio, s_sm)) {
            const uint8_t b = static_cast<uint8_t>(pio_sm_get(s_pio, s_sm) & 0xffu);
            if (rx) *rx++ = b;
            rx_rem--;
        }
    }
}

void shiftio_init(PIO pio, uint sm) {
    s_pio = pio;
    s_sm = sm;

    uint offset = pio_add_program(pio, &shiftio_program);
    pio_sm_config c = shiftio_program_get_default_config(offset);

    sm_config_set_out_pins(&c, PIN_MOSI, 1);
    sm_config_set_in_pins(&c, PIN_MISO);
    sm_config_set_sideset_pins(&c, PIN_SCLK);
    // MSB first (shift left), autopull/autopush every 8 bits.
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_in_shift(&c, false, true, 8);

    // 4 SM cycles per bit -> target SHIFT_CLK_HZ.
    const float div = static_cast<float>(clock_get_hz(clk_sys)) / (4.0f * static_cast<float>(SHIFT_CLK_HZ));
    sm_config_set_clkdiv(&c, div);

    // Hand SCLK/MOSI/MISO to the PIO and set directions (MISO is the only input).
    pio_gpio_init(pio, PIN_SCLK);
    pio_gpio_init(pio, PIN_MOSI);
    pio_gpio_init(pio, PIN_MISO);
    pio_sm_set_pindirs_with_mask(
        pio, sm,
        (1u << PIN_SCLK) | (1u << PIN_MOSI),                       // outputs
        (1u << PIN_SCLK) | (1u << PIN_MOSI) | (1u << PIN_MISO));   // affected

    // Control lines via SIO: LOAD idle high (shift mode), SS idle low.
    gpio_init(PIN_LOAD);
    gpio_set_dir(PIN_LOAD, GPIO_OUT);
    gpio_put(PIN_LOAD, 1);
    gpio_init(PIN_SS);
    gpio_set_dir(PIN_SS, GPIO_OUT);
    gpio_put(PIN_SS, 0);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void shiftio_read_inputs(uint8_t *dst, size_t n) {
    // SH/LD is active-low and asynchronous: a brief low pulse captures the pins.
    gpio_put(PIN_LOAD, 0);
    sleep_us(1);
    gpio_put(PIN_LOAD, 1);
    transfer(nullptr, dst, n);
}

void shiftio_write_outputs(const uint8_t *src, size_t n) {
    transfer(src, nullptr, n);
    // RCLK latches the shift register into the outputs on the rising edge.
    gpio_put(PIN_SS, 1);
    sleep_us(1);
    gpio_put(PIN_SS, 0);
}

void shiftio_exchange(const uint8_t *out, uint8_t *in, size_t n) {
    // Latch the parallel inputs, full-duplex shift, then latch the outputs.
    gpio_put(PIN_LOAD, 0);
    busy_wait_us(1);
    gpio_put(PIN_LOAD, 1);

    transfer(out, in, n);

    gpio_put(PIN_SS, 1);
    busy_wait_us(1);
    gpio_put(PIN_SS, 0);
}
