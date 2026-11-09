#pragma once
#include <chrono>

#include "agr/core/Types.hpp"

namespace agr {

/// High resolution monotonic stopwatch.
class Stopwatch {
public:
    using Clock = std::chrono::steady_clock;
    Stopwatch() : m_start(Clock::now()) {}
    void restart() { m_start = Clock::now(); }
    f64 elapsedSeconds() const { return std::chrono::duration<f64>(Clock::now() - m_start).count(); }
    f64 elapsedMilliseconds() const { return elapsedSeconds() * 1000.0; }

private:
    Clock::time_point m_start;
};

/// Per-frame timing information handed to every system update.
struct FrameTime {
    f64 deltaSeconds = 0.0;        ///< Variable delta (clamped).
    f64 unscaledDeltaSeconds = 0.0;
    f64 fixedDeltaSeconds = 1.0 / 60.0;
    f64 totalSeconds = 0.0;
    f64 timeScale = 1.0;
    u64 frameIndex = 0;
    f32 alpha = 0.0f;              ///< Interpolation factor between fixed steps.
};

} // namespace agr
