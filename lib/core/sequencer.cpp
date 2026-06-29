#include "sequencer.h"
#include <cstring>

namespace dmt {

void Sequencer::clear() { memset(g_, 0, sizeof(g_)); }

StepState Sequencer::get(uint8_t pat, uint8_t track, uint8_t step) const {
    if (pat >= SEQ_PATTERNS || track >= SEQ_MAX_TRACKS || step >= SEQ_STEPS)
        return STEP_OFF;
    return (StepState)g_[pat][track][step];
}

void Sequencer::set(uint8_t pat, uint8_t track, uint8_t step, StepState s) {
    if (pat >= SEQ_PATTERNS || track >= SEQ_MAX_TRACKS || step >= SEQ_STEPS) return;
    g_[pat][track][step] = s;
}

void Sequencer::cycle(uint8_t pat, uint8_t track, uint8_t step) {
    if (pat >= SEQ_PATTERNS || track >= SEQ_MAX_TRACKS || step >= SEQ_STEPS) return;
    g_[pat][track][step] = static_cast<uint8_t>((g_[pat][track][step] + 1) % 3);  // off->on->accent
}

}  // namespace dmt
