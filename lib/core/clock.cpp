#include "clock.h"
#include "io.h"            // io_sync_ticks()
#include "pico/stdlib.h"

namespace dmt {

void Clock::init() {
    running_ = false;
    external_ = false;
    bpm_ = 120.0f;
    swing_ = 50;
    step_ = 0xFF;
}

uint32_t Clock::step_us_() const {
    return (uint32_t)(60000000.0f / (bpm_ * 4.0f));   // quarter / 4 = 16th
}

void Clock::set_tempo(float bpm) {
    if (bpm < 20.0f) bpm = 20.0f;
    if (bpm > 300.0f) bpm = 300.0f;
    bpm_ = bpm;
}

void Clock::set_swing(uint8_t pct) {
    if (pct < 50) pct = 50;
    if (pct > 75) pct = 75;
    swing_ = pct;
}

void Clock::set_external(bool e) {
    external_ = e;
    sync_base_ = io_sync_ticks();
}

void Clock::start() {
    running_ = true;
    step_ = 0xFF;
    next_us_ = time_us_32();
    sync_base_ = io_sync_ticks();
}

void Clock::stop()   { running_ = false; }
void Clock::toggle() { running_ ? stop() : start(); }

bool Clock::poll(uint8_t *step) {
    if (!running_) return false;

    if (external_) {
        // 24 PPQN -> 6 ticks per 16th note.
        if ((uint32_t)(io_sync_ticks() - sync_base_) >= 6) {
            sync_base_ += 6;
            step_ = (uint8_t)((step_ + 1) & 15);
            *step = step_;
            return true;
        }
        return false;
    }

    uint32_t now = time_us_32();
    if ((int32_t)(now - next_us_) >= 0) {
        step_ = (uint8_t)((step_ + 1) & 15);
        *step = step_;
        // Swing: even steps dwell longer, odd steps shorter (delays the
        // off-beat 16ths). s = 0 at 50%, 0.5 at 75%.
        float s = (swing_ - 50) / 50.0f;
        uint32_t T = step_us_();
        uint32_t dwell = (step_ & 1) ? (uint32_t)(T * (1.0f - s))
                                     : (uint32_t)(T * (1.0f + s));
        next_us_ = now + dwell;
        return true;
    }
    return false;
}

}  // namespace dmt
