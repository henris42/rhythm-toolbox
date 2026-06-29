#pragma once
//
// Drum Machine Toolbox — public API.
//
// Build your own analog drum machine: declare your voices, optionally tweak a
// few settings and hooks, and call run(). The toolbox handles the shift-register
// IO, gamma-PWM LEDs, trigger outputs, MIDI, clock (internal tempo + external
// 24-PPQN sync, swing), the X0X sequencer, and the default front panel.
//
// Default front panel:
//   S1..S16  step buttons (toggle on/off; hold S19 to toggle accent)
//   S17      start / stop          S18  fill (hold -> play/edit fill pattern)
//   S19      accent (hold + step)  S20  clear current track
//   Rotary 1 mode: pos 0-6 = pattern, pos 7 = CONFIG
//   Rotary 2 editable track select
//   Pot A1   tempo                 Pot A0    swing/shuffle
//   Switch 1 external sync         LEDs: colour = accent, bright = playhead
//
// Config (rotary 1 -> pos 7): hold a left button (S1-S8 = option) and push a
// right button (S9-S16 = value 0-7). v1 option: S1 = trigger pulse length.
//
#include "machine.h"
#include "sequencer.h"   // StepState
#include "extio.h"       // ExtOut: EXT_O1.., EXT_AC2/3
