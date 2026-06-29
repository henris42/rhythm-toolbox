#include "midi.h"
#include "pins.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#if TOOLBOX_USB_MIDI
#include "tusb.h"
#endif

#define MIDI_UART uart0
#define MIDI_BAUD 31250

namespace {

// Running-status byte parser; one instance per stream (UART / USB).
struct Parser {
    uint8_t status = 0, data[2] = {}, idx = 0, need = 0;
};
Parser s_uart;
#if TOOLBOX_USB_MIDI
Parser s_usb;
#endif

// Feed one byte; returns true and fills `m` when a full message is parsed.
bool feed(Parser &p, uint8_t b, MidiMsg *m) {
    if (b >= 0xF8) {                       // system real-time: single byte
        m->status = b; m->data1 = m->data2 = 0;
        return true;
    }
    if (b & 0x80) {                        // status byte
        p.status = b; p.idx = 0;
        uint8_t hi = b & 0xF0;
        if (hi == 0xC0 || hi == 0xD0) p.need = 1;
        else if (b == 0xF1 || b == 0xF3)  p.need = 1;
        else if (b == 0xF2)               p.need = 2;
        else if (b >= 0xF4) { m->status = b; m->data1 = m->data2 = 0; p.status = 0; return true; }
        else p.need = 2;
    } else {                               // data byte (running status applies)
        if (!p.status) return false;
        p.data[p.idx++] = b;
        if (p.idx >= p.need) {
            p.idx = 0;
            m->status = p.status;
            m->data1 = p.data[0];
            m->data2 = (p.need > 1) ? p.data[1] : 0;
            return true;
        }
    }
    return false;
}

}  // namespace

void midi_init() {
    uart_init(MIDI_UART, MIDI_BAUD);
    gpio_set_function(PIN_MIDI_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_MIDI_RX, GPIO_FUNC_UART);
    uart_set_format(MIDI_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(MIDI_UART, true);
}

bool midi_read(MidiMsg *m) {
    while (uart_is_readable(MIDI_UART))
        if (feed(s_uart, (uint8_t)uart_getc(MIDI_UART), m)) return true;
#if TOOLBOX_USB_MIDI
    uint8_t b;
    while (tud_midi_available() && tud_midi_stream_read(&b, 1))
        if (feed(s_usb, b, m)) return true;
#endif
    return false;
}

void midi_send(uint8_t status, uint8_t d1, uint8_t d2) {
    uart_putc_raw(MIDI_UART, status);
    uart_putc_raw(MIDI_UART, d1);
    uart_putc_raw(MIDI_UART, d2);
#if TOOLBOX_USB_MIDI
    uint8_t pkt[3] = {status, d1, d2};
    tud_midi_stream_write(0, pkt, 3);
#endif
}

void midi_send_rt(uint8_t rt) {
    uart_putc_raw(MIDI_UART, rt);
#if TOOLBOX_USB_MIDI
    tud_midi_stream_write(0, &rt, 1);
#endif
}
