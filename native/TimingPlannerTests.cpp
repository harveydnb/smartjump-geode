#include "TimingPlanner.hpp"
#include <array>
#include <cassert>
#include <limits>
using smartjump::Candidate;
using smartjump::chooseTiming;
int main() {
    const std::array<Candidate, 3> early{{
        {0.0, true, false, 0.0},
        {0.06, true, true, 10.0},
        {0.03, true, true, 8.0},
    }};
    auto corrected = chooseTiming(early);
    assert(corrected.shouldDelay && corrected.delaySeconds == 0.03);
    const std::array<Candidate, 2> alreadySafe{{
        {0.0, true, true, 1.0}, {0.03, true, true, 2.0}
    }};
    assert(!chooseTiming(alreadySafe).shouldDelay);
    const std::array<Candidate, 2> unknown{{
        {0.0, false, false, 0.0}, {0.04, false, true, 2.0}
    }};
    assert(!chooseTiming(unknown).shouldDelay);
    const std::array<Candidate, 2> invalid{{
        {0.2, true, true, 2.0},
        {std::numeric_limits<double>::quiet_NaN(), true, true, 2.0}
    }};
    assert(!chooseTiming(invalid).shouldDelay);
    return 0;
}
