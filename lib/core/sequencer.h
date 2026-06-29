#pragma once
//
// sequencer: classic X0X grid. Patterns x tracks x 16 steps, each step OFF / ON
// / ACCENT (accent shown via LED colour and routed to the accent bus).
//
#include <cstdint>

namespace dmt {

enum StepState : uint8_t { STEP_OFF = 0, STEP_ON = 1, STEP_ACCENT = 2 };

constexpr uint8_t SEQ_STEPS      = 16;
constexpr uint8_t SEQ_PATTERNS   = 8;   // selected by the "mode" rotary
constexpr uint8_t SEQ_MAX_TRACKS = 8;   // selectable by the "editable" rotary

class Sequencer {
public:
    void clear();
    StepState get(uint8_t pat, uint8_t track, uint8_t step) const;
    void set(uint8_t pat, uint8_t track, uint8_t step, StepState s);
    void cycle(uint8_t pat, uint8_t track, uint8_t step);  // off->on->accent->off

private:
    uint8_t g_[SEQ_PATTERNS][SEQ_MAX_TRACKS][SEQ_STEPS] = {};
};

}  // namespace dmt
