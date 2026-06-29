# IO Map

Hardware pin and IO reference for the Rhythm Toolbox board (RP2350 / Pico 2). Pin
definitions are in [`lib/hal/pins.h`](../lib/hal/pins.h).

## Shift-register bus

| Role | GPIO | Notes |
|------|------|-------|
| SCLK | GP6  | shared shift clock (buffered, non-inverting) |
| MISO | GP16 | 74165 chain serial out (inputs) |
| MOSI | GP8  | 74595 chain serial in (LED outputs) |
| LOAD | GP15 | 74165 SH/LD̄ — latch parallel inputs (active low) |
| SS   | GP28 | 74595 RCLK — latch outputs |

PIO-driven (`shiftio`), SPI mode 0, MSB first, `SHIFT_CLK_HZ` = 16 MHz. Scanned at
16 kHz; input debounce is time-based, independent of the scan rate.

## Inputs

### Buttons S1–S20

- **S1–S16** come from two 8-way priority encoders. Each encoder's 3-bit code sits
  in the last shift byte: `buf[1]` bits 2–4 (group S1–S8) and bits 5–7 (group
  S9–S16); code `7` = no key. A priority encoder reports only the highest key in
  its group, so at most one per group at a time. The two index-0 keys (**S1**,
  **S9**) would alias the "no key" code, so they're read from dedicated GPIO
  (GP3, GP4, active low).
- **S17–S20** are wired straight to the remaining shift bits (`buf[0]` bits 6–7,
  `buf[1]` bits 0–1), active low.

`input.cpp` maps decoded positions to the S1–S20 labels (`kSwitch[]` / `kDirect[]`),
so board wiring order is absorbed in firmware.

### Rotaries, switches, pots

| Input | Source | Notes |
|-------|--------|-------|
| R1, R2 | `buf[0]` bits 0–2 / 3–5 | 8-position absolute rotary switches |
| SW1 | GP10 | toggle — external sync on/off |
| SW2 | GP11 | toggle — variation |
| Pot A0 | GP26 / ADC0 | shuffle — 12-bit, EMA-filtered |
| Pot A1 | GP27 / ADC1 | tempo — 12-bit, EMA-filtered |

### Shift-bit allocation

| Byte | bit 7 | bit 6 | bit 5 | bit 4 | bit 3 | bit 2 | bit 1 | bit 0 |
|------|-------|-------|-------|-------|-------|-------|-------|-------|
| buf[0] | S18 | S17 | ◄── R2 ──► | ◄── R1 ──► | | |
| buf[1] | ◄── S9–S16 encoder ──► | ◄── S1–S8 encoder ──► | S19 | S20 |

(Plus S1/S9 on GP3/GP4.)

### Sync / start in

| Input | Pin | Notes |
|-------|-----|-------|
| sync in | GP12 | 24-PPQN clock, active low, GPIO IRQ → `io_sync_ticks()` |
| start in | GP13 | active low → `StartChanged` |

## Outputs

### LEDs — 16 two-wire bicolor

Driven by the 74595 chain (32 bits, 2 per LED = the two terminals):

| Pair | Result |
|------|--------|
| 01 | colour 1 |
| 10 | colour 2 |
| 00 / 11 | off |

Gamma-corrected software PWM (`LED_PWM_STEPS` = 128 → 125 Hz refresh). A 2-wire LED
shows one colour at a time; colour "mixing" is temporal — the PWM frame is split
between the two terminals (`leds.cpp`).

### Triggers + MIDI

| Signal | Pin | Notes |
|--------|-----|-------|
| O1–O7 | GP22, 21, 20, 19, 18, 17, 14 | gate/trigger outputs, `extio_pulse()` |
| AC2, AC3 | GP9, GP2 | accent / aux outputs |
| MIDI out / in | GP0 / GP1 | UART0, 31250 baud (also class-compliant USB MIDI) |

Outputs are active high; `extio_pulse(out, len_us)` drives high and auto-clears.

## Event model (`lib/hal/input.h`)

The IO engine emits:

| Event | Fields | Notes |
|-------|--------|-------|
| `ButtonPressed` | id 1–20 (S1–S20) | press edge only |
| `RotaryChanged` | id 1–2, value 0–7 | absolute position |
| `SwitchChanged` | id 1–2, value 0/1 | both edges |
| `PotChanged` | id 1–2, value 0–4095 | EMA-filtered |
| `StartChanged` | value 0/1 | start-in |

The sync clock is read via `io_sync_ticks()`; MIDI in via `midi_read()`.

## IO engine (`lib/hal/io.cpp`)

A hardware timer at 16 kHz owns the bus. Each tick renders the LED PWM frame,
exchanges it **full-duplex** in one transaction (LED bytes out → 74595s, input
bytes in ← 74165s), and decodes inputs into an ISR-safe event ring drained by the
main loop via `io_process()`. The transfer is `max(NUM_165, NUM_595)` bytes, so
adding shift-register chips is just bumping those counts.
