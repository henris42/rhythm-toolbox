#pragma once
//
// clock: musical timing. Internal tempo (with swing) or external 24-PPQN sync.
// Emits 16th-note steps (0..15). The transport (start/stop) gates it.
//
#include <cstdint>

namespace dmt {

class Clock {
public:
    void init();
    void set_tempo(float bpm);     // internal tempo
    void set_swing(uint8_t pct);   // 50 (straight) .. 75 (max shuffle)
    void set_external(bool e);     // follow the 24-PPQN sync-in clock
    void start();
    void stop();
    void toggle();
    bool running() const { return running_; }

    // Call frequently from the run loop; returns true when a new step is due
    // and writes the step index (0..15).
    bool poll(uint8_t *step);

private:
    uint32_t step_us_() const;     // base 16th-note period

    bool     running_  = false;
    bool     external_ = false;
    float    bpm_      = 120.0f;
    uint8_t  swing_    = 50;
    uint8_t  step_     = 0xFF;      // wraps to 0 on first advance
    uint32_t next_us_  = 0;         // internal: time of next step
    uint32_t sync_base_ = 0;        // external: sync-tick count of last step
};

}  // namespace dmt
