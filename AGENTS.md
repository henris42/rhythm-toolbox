# AGENTS.md — orientation for AI agents

Read this first. It's the fast path to working effectively in this repo. Deeper
detail is in [`docs/`](docs/).

## What this is

**Rhythm Toolbox** — open-source firmware + library for building an analog drum
machine on the **RP2350 (Raspberry Pi Pico 2)**. The hard real-time work lives in
a library; an end user writes their machine in **one short example file**.

## Layout & the golden rule

```
lib/hal/    board layer  — shiftio (PIO bus), input, leds, extio, midi, io, usb, pins
lib/core/   engine       — clock, sequencer   (board-agnostic)
lib/machine.* lib/toolbox.h   framework + default UI + public API
examples/   one-file apps (basic_drums.cpp = the thing users edit)
docs/       constructors-guide/ · musicians-guide/ · vibe-coders-guide/ · io-map.md
```

**Golden rule:** app-level changes go in `examples/`. Only touch `lib/` for
framework/HAL/engine changes — and say so. `docs/io-map.md` is the hardware
source of truth.

## How the hardware works (the non-obvious bits)

- **One 16 kHz timer ISR owns the shared shift-register bus.** Each tick does a
  *single full-duplex transfer*: clocks the LED PWM frame out to 4× 74595 **and**
  reads buttons in from 2× 74165 simultaneously, then decodes inputs into an
  ISR→main-loop event ring. See `lib/hal/io.cpp`. Don't add a second bus owner.
- **The bus is PIO** (`shiftio.pio`), not hardware SPI, because the board's MOSI
  pin isn't an SPI-TX pin. Pin-agnostic; runs at 16 MHz.
- **External 24-PPQN sync** is a GPIO interrupt counter (`io_sync_ticks()`), not
  polled — keep clock timing off the slow path.
- **LEDs are 2-wire antiparallel bicolor**: one colour at a time (`01`=colour1,
  `10`=colour2, `00/11`=off, never emit `11`). "Colour mixing" is **temporal** —
  the frame is split between the two terminals; at 16 kHz it fuses into a blend.
  Brightness is perceptual 0–255, gamma-corrected. See `lib/hal/leds.cpp`.
  - Gotcha: green LEDs are much brighter than red, so a dim/even mix reads as
    green; a *bright balanced* mix (`leds_set_mix(220,220)`) gives clean amber.
- **Board-quirk remaps are table-driven and isolated** — fix wiring swaps here,
  not in logic: button order `kSwitch[]`/`kDirect[]` (`input.cpp`), LED chain
  order `led_bit()` (`leds.cpp`), patched pins in `pins.h`. On a good board these
  are identity maps.

## USB MIDI (composite) — important gotchas

- Default-on (`-DTOOLBOX_USB_MIDI=OFF` for CDC-only). Device = **USB MIDI + USB
  serial (CDC) + picotool reset**, all on one cable.
- TinyUSB is driven manually: `io_usb_task()` (→ `tud_task()`) is pumped from the
  run loop — **required**, the SDK stops pumping once you supply your own
  descriptors. If USB doesn't enumerate, this is the usual cause.
- The picotool **reset interface must be at USB interface index 2** (CDC=0/1,
  reset=2, MIDI=3/4) and `bcdUSB=0x0210`, so the SDK's MS-OS-2.0 descriptor makes
  it driverless on Windows (else reflashing over usbipd hangs). See
  `lib/hal/usb/usb_descriptors.c`.

## Build, flash, test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # once
cmake --build build
picotool load -x build/basic_drums.uf2                    # or drag .uf2 onto BOOTSEL
```

- Each `examples/*.cpp` is its own target. `PICO_SDK_PATH` must point at the Pico
  SDK (defaults to a local path in `CMakeLists.txt`).
- **This is hardware-in-the-loop.** You can't see LEDs / hear triggers from the
  shell. The real verification loop is: make a small change → flash → **ask the
  human what they observe** → iterate. Keep changes small and testable.
- Machine-local flashing helpers may exist under `local/` (gitignored, e.g. a
  WSL+usbipd auto-attach + flash script); use them if present.

## Conventions

- C++17, Pico SDK. Match the surrounding style; comments explain *why*.
- The `dmt` namespace is the public API (`toolbox.h`).
- Commits: author is the human; **do not add AI co-authors**. Commit only when
  asked; don't push unless asked.
