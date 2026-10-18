#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

namespace smartjump {
// The caller must supply validated, mode-specific simulation results.
// This module does NOT infer geometry or collision state from game memory.
struct Candidate {
    double delaySeconds;
    bool simulationValid;
    bool survives;
    double progress;
};
struct Decision {
    bool shouldDelay = false;
    double delaySeconds = 0.0;
};
// Prefer the earliest verified safe candidate; never guess when simulation is unavailable.
// Corrections cannot retroactively rescue a click after a collision has happened.
inline Decision chooseTiming(std::span<const Candidate> candidates,
                             double maxDelaySeconds = 0.120) noexcept {
    if (!std::isfinite(maxDelaySeconds) || maxDelaySeconds < 0) return {};
    double earliest = std::numeric_limits<double>::infinity();
    bool safeNow = false;
    for (const auto& candidate : candidates) {
        if (!candidate.simulationValid || !candidate.survives ||
            !std::isfinite(candidate.delaySeconds) ||
            !std::isfinite(candidate.progress) ||
            candidate.delaySeconds < 0 || candidate.delaySeconds > maxDelaySeconds)
            continue;
        if (candidate.delaySeconds == 0) safeNow = true;
        earliest = std::min(earliest, candidate.delaySeconds);
    }
    if (safeNow || !std::isfinite(earliest) || earliest <= 0) return {};
    return {true, earliest};
}
} // namespace smartjump
