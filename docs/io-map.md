# rhythm — IO map

Single source of truth for the control surface. The **Function** columns are the
only thing left to fill in; everything else is fixed by the hardware. The C++
in `src/io_map.h` mirrors this table — update both together.

See [pins.h](../include/pins.h) for the raw pin/bus definitions.

## Bus

| Role | GPIO | Notes |
|------|------|-------|
| SCLK | GP6  | shared shift clock, buffered (non-inverting) |
| MISO | GP16 | 74165 chain serial out |
| MOSI | GP8  | 74595 chain serial in |
| LOAD | GP15 | 74165 SH/LD̄, active low (latch parallel inputs) |
| SS   | GP28 | 74595 RCLK (latch outputs) |

Driven by PIO (`shiftio`), SPI mode 0, MSB first. Shift clock `SHIFT_CLK_HZ`
(16 MHz; bus rated ~16-20 MHz). Scanned at 16 kHz;
input debounce is time-based so it's independent of the scan rate.

## Inputs

`shiftio_read_inputs()` returns `buf[0..1]`. `buf[0]` is the first byte clocked
(74165 nearest MISO); within a byte the MSB (bit 7) is the first bit out.

### Buttons S1..S16 (two 8-way priority encoders + 2 patches)

16 buttons read through two 8-way priority encoders plus two GPIO patches. Each
encoder emits a 3-bit code in the **last shift byte** `buf[1]`. The idle code is
`7` (`111`) = no key, which collides with the encoder's index-0 key — so the two
index-0 keys (**S1**, **S9**) are bodge-wired to GPIO instead.

Decode is first-cut (74148-style inverted code, `raw = 7 - field`), to be
confirmed against the live print. `kSwitch[]` in `src/input.cpp` maps raw
positions to PCB labels; **S5/S6 are swapped** there due to routing.

**Full bit → switch table:**

| Source                  | Field code | raw | Switch |
|-------------------------|:----------:|:---:|:------:|
| GP3 (patch, active low) |     —      |  0  | **S1** |
| `buf[1]` bits 2..4 (g1) |     6      |  1  | S2 |
| `buf[1]` bits 2..4 (g1) |     5      |  2  | S3 |
| `buf[1]` bits 2..4 (g1) |     4      |  3  | S4 |
| `buf[1]` bits 2..4 (g1) |     3      |  4  | **S6** ⇄ |
| `buf[1]` bits 2..4 (g1) |     2      |  5  | **S5** ⇄ |
| `buf[1]` bits 2..4 (g1) |     1      |  6  | S7 |
| `buf[1]` bits 2..4 (g1) |     0      |  7  | S8 |
| GP4 (patch, active low) |     —      |  8  | **S9** |
| `buf[1]` bits 5..7 (g2) |     6      |  9  | S10 |
| `buf[1]` bits 5..7 (g2) |     5      | 10  | S11 |
| `buf[1]` bits 5..7 (g2) |     4      | 11  | S12 |
| `buf[1]` bits 5..7 (g2) |     3      | 12  | **S16** ⇄ |
| `buf[1]` bits 5..7 (g2) |     2      | 13  | **S15** ⇄ |
| `buf[1]` bits 5..7 (g2) |     1      | 14  | **S13** ⇄ |
| `buf[1]` bits 5..7 (g2) |     0      | 15  | **S14** ⇄ |

`g1 == 7` ⇒ no key in S1..S8 group; `g2 == 7` ⇒ no key in S9..S16 group. A
priority encoder reports only the highest key in its group, so at most one of
S2..S8 and one of S10..S16 at once (plus S1/S9 independently). ⇄ = routing swap.

### Rotary switches (2)

**8-position absolute** rotary switches (not encoders); 3-bit position each, in
the first shift byte `buf[0]`. `RotaryChanged` reports the absolute position
0..7 on change — no accumulation/delta.

| Rotary | Source             |
|--------|--------------------|
| R1     | `buf[0]` bits 0..2 |
| R2     | `buf[0]` bits 3..5 |

### Direct buttons S17..S20

4 buttons wired straight to the remaining shift-register bits (no encoder),
**active low** (external pull-ups: idle 1, pressed 0). Labels provisional. This
uses every bit position.

| Switch | Source       |
|--------|--------------|
| S17    | `buf[0]` bit 6 |
| S18    | `buf[0]` bit 7 |
| S19    | `buf[1]` bit 1 |
| S20    | `buf[1]` bit 0 |

### Bit usage summary

| Byte    | bit 7 | bit 6 | bit 5 | bit 4 | bit 3 | bit 2 | bit 1 | bit 0 |
|---------|-------|-------|-------|-------|-------|-------|-------|-------|
| buf[0]  | S18   | S17   | ◄──── R2 (3-bit) ────► | ◄──── R1 (3-bit) ────► | | |
| buf[1]  | ◄──── g2 → S10..S16 ────► | ◄──── g1 → S2..S8 ────► | S19 | S20 |

(`buf[0]` low six bits split R1=bits0..2, R2=bits3..5; `buf[1]` g1=bits2..4,
g2=bits5..7.) Plus S1/S9 on GP3/GP4.

### Toggle switches (2) and pots (2) — direct to the RP2350

Not on the shift bus:

| Input | Pin              | Notes |
|-------|------------------|-------|
| SW1   | GP10             | **ext sync** (bool) — active low, pull-up, debounced both edges |
| SW2   | GP11             | **variation** (bool) — "" |
| Pot1  | GP26 / ADC0 (A0) | shuffle — EMA (α=1/16) + 4-count deadband |
| Pot2  | GP27 / ADC1 (A1) | tempo — "" |

Pots sampled at ~1 kHz, EMA-filtered, reported as **12-bit (0..4095)** via
`PotChanged`; the app maps tempo→BPM (40..240) and shuffle→%. Switches via
`SwitchChanged` (value 0/1).

## Direct-GPIO sync / trigger IO (`src/extio.h`)

Not on the shift bus. Outputs active high, idle low (`extio_set`).

| Signal | Pin  | Dir | Notes |
|--------|------|-----|-------|
| sync in | GP12 | in  | **24-PPQN clock**, active low, GPIO IRQ → `io_sync_ticks()` |
| start   | GP13 | in  | active low, debounced 5 ms → `StartChanged` (asserted=1) |
| MIDI out | GP0 | out | UART0 TX, 31250 baud |
| MIDI in  | GP1 | in  | UART0 RX, 31250 baud |
| O1      | GP22 | out | |
| O2      | GP21 | out | |
| O3      | GP20 | out | |
| O4      | GP19 | out | |
| O5      | GP18 | out | |
| O6      | GP17 | out | |
| O7      | GP14 | out | (GP15 was first asked, but that's `PIN_LOAD`) |
| AC2     | GP9  | out | function TBD |
| AC3     | GP2  | out | function TBD |

Outputs support `extio_pulse(out, len_us)` — drive high and auto-clear after a
configurable length (the tick ISR clears expired pulses, ~62 µs resolution).

## C++ event model (`src/input.h`)

`input_decode()` (run by the IO engine) emits events:

| Event           | Fields            | Notes |
|-----------------|-------------------|-------|
| `ButtonPressed` | `id` 1..20 (S1..S20) | press edge only, no release |
| `RotaryChanged` | `id` 1..2 (R1..R2), `value` 0..7 | absolute position, on change |
| `SwitchChanged` | `id` 1..2, `value` 0/1 | on/off toggle, both edges |
| `PotChanged`    | `id` 1..2, `value` 0..4095 | EMA-filtered, 12-bit |
| `StartChanged`  | `value` 0/1 | start-in asserted (active low) |

(Sync clock is not an event — read `io_sync_ticks()` for the 24-PPQN count.
MIDI in is parsed via `midi_read()`; see `src/midi.h`.)

The first poll seeds the baseline silently (no boot-time events). Because the
buttons are priority-encoded, only the highest key per group is visible at once
(plus S1/S9 independently).

## Outputs — 16 bicolor LEDs (4× 74595 = 32 bits)

32 output bits drive **16 two-wire (antiparallel) bicolor LEDs**, 2 bits each =
the two terminals. The pair shows one colour at a time (no mixing):

| Pair | Result |
|------|--------|
| 01   | colour 1 (confirmed red) |
| 10   | colour 2 |
| 00 / 11 | off (no current) |

Software-PWM'd by the IO engine (`leds.cpp`):
- `led_bit(led, terminal)` maps to the 32-bit shift stream; the chain is
  **reversed** (led 0 = LED 1 at the far end). Active-high (confirmed).
- Render drives exactly one terminal high while on → 01 or 10, else 00; never 11.
- Stream position 0 = first bit clocked = `out[0]` MSB.

## IO engine (`src/io.cpp`)

A hardware `repeating_timer` at **2 kHz** owns the bus. Each tick:

1. `leds_render(phase)` builds the 4 output bytes for the current PWM phase.
2. `shiftio_exchange()` does one full-duplex transaction: out → 74595s, in ←
   74165s (valid in the first 16 bits). LOAD latches inputs before, SS latches
   outputs after.
3. `input_decode()` turns the read into events, enqueued in an ISR-safe ring.

The main loop calls `io_process(handler)` to drain events. Tick = 16 kHz, PWM
`LED_PWM_STEPS` (128) duty levels → 16000/128 = 125 Hz refresh. Shift clock
`SHIFT_CLK_HZ` (16 MHz). The tick transfers `IO_XFER_BYTES` = max(in,out) bytes,
so adding 74165/74595 chips is just bumping `NUM_165`/`NUM_595`. LED brightness
is perceptual 0..255, **gamma-corrected**
(γ = 2.2, `s_gamma[]` LUT) to PWM duty in `leds_set()`.
