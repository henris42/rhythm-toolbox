# Rhythm Toolbox

An open-source **Drum Machine Toolbox** for the RP2350 (Raspberry Pi Pico 2) — a
kit and firmware library for building your own analog drum machine. The hard
parts (tight timing, shift-register IO, gamma-PWM LEDs, trigger outputs, MIDI,
the sequencer) live in a hidden library; you write your machine in **one small
example file**.

## What it does

- **X0X step sequencer** — 8 patterns × tracks × 16 steps, on/off + accent, swing.
- **Clock** — internal tempo or external 24-PPQN sync, start/stop.
- **Voices** — each track fires a trigger output (configurable pulse length) and a
  MIDI note; incoming MIDI notes trigger voices too.
- **Default front panel** — step buttons, mode/track rotaries, tempo/shuffle pots,
  accent/fill/clear, a config mode, and a 16× bicolor LED grid with colour mixing.
- **MIDI over both DIN (UART) and USB** (class-compliant; the device also exposes
  a USB serial port + picotool reset).

## Documentation

- **[Musician's Guide](docs/musicians-guide/)** — play a machine running the default
  firmware (no coding).
- **[Vibe Coder's Guide](docs/vibe-coders-guide/)** — customize your machine by
  editing the example with an AI assistant.
- **[Constructor's Guide](docs/constructors-guide/)** — architecture, build/flash,
  the full API, and writing your own machine.
- **[IO Map](docs/io-map.md)** — the hardware pin/IO reference (source of truth).
- **[AGENTS.md](AGENTS.md)** — orientation for AI agents / contributors working on
  the codebase.

## Layout

```
lib/
  hal/    board layer — shift-register bus (PIO), inputs, LEDs, triggers, MIDI, USB
  core/   board-agnostic engine — clock, sequencer
  machine.*  toolbox.h   the framework + public API
examples/
  basic_drums.cpp     copy & edit this — declare voices, run()
docs/
  io-map.md           full hardware pin/IO map (source of truth)
```

A whole drum machine is ~20 lines — see [`examples/basic_drums.cpp`](examples/basic_drums.cpp).

## Build & flash

Requires the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
(set `PICO_SDK_PATH`). Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# drag build/basic_drums.uf2 onto the RP2350 in BOOTSEL, or use picotool:
picotool load -x build/basic_drums.uf2
```

USB MIDI is on by default; build with `-DTOOLBOX_USB_MIDI=OFF` for a CDC-only
device.

## Status

Hardware bring-up and the toolbox v1 are working on real hardware. Sequencer
features (song mode, per-track options, MIDI clock out) are in progress.

## License

MIT — see [LICENSE](LICENSE).
