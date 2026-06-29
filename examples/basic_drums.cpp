// Drum Machine Toolbox — example app. Copy this file and make it your own.
//
// Declare your voices (name, trigger output, MIDI note), optionally preload a
// pattern, and run(). Everything else — UI, clock, sequencer, MIDI — is handled.

#include "toolbox.h"
using namespace dmt;

// Your analog voices: each fires a trigger output and an optional MIDI note.
static Track tracks[] = {
    {"BD", EXT_O1, 36},   // bass drum
    {"SD", EXT_O2, 38},   // snare
    {"CH", EXT_O3, 42},   // closed hat
    {"OH", EXT_O4, 46},   // open hat
    {"CP", EXT_O5, 39},   // clap
    {"CB", EXT_O6, 56},   // cowbell
};

int main() {
    Machine m;
    m.set_tracks(tracks, sizeof(tracks) / sizeof(tracks[0]));
    m.set_tempo(120);
    m.set_led_levels(150, 220, 150);   // on / accent / playhead brightness (0..255)

    // A starter beat on pattern 0 (edit live on the panel from here).
    for (uint8_t s = 0; s < 16; s += 4) m.set(0, s, STEP_ON);  // BD 4-on-the-floor
    m.set(1, 4, STEP_ON);
    m.set(1, 12, STEP_ACCENT);                                  // SD backbeat (accent)
    for (uint8_t s = 0; s < 16; s += 2) m.set(2, s, STEP_ON);  // CH eighths

    m.run();   // never returns
}
