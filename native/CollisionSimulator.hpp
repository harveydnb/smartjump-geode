#pragma once
#include "TimingPlanner.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace smartjump {
// World-space axis-aligned solids/hazards, supplied by a validated level adapter.
struct Rect { double left, bottom, right, top; };
struct Vec2 { double x, y; };
enum class Mode { Cube, Ship, Ball, UFO, Wave, Robot, Spider };
struct State {
    Vec2 position, velocity;
    double halfWidth, halfHeight;
    Mode mode;
    bool grounded;
    bool gravityFlipped;
};
struct World {
    std::span<const Rect> hazards;
    std::span<const Rect> solids;
    double gravity = -45.0;
    double jumpSpeed = 15.0;
    double shipThrust = 70.0;
    double waveSpeed = 15.0;
};
struct Simulation {
    bool valid = false;
    bool survives = false;
    double progress = 0;
};
inline bool validRect(const Rect& r) {
    return std::isfinite(r.left) && std::isfinite(r.right) &&
           std::isfinite(r.bottom) && std::isfinite(r.top) &&
           r.left <= r.right && r.bottom <= r.top;
}
inline bool overlap(const Rect& a, const Rect& b) {
    return a.left < b.right && a.right > b.left &&
           a.bottom < b.top && a.top > b.bottom;
}
inline Rect bounds(const State& s) {
    return {s.position.x - s.halfWidth, s.position.y - s.halfHeight,
            s.position.x + s.halfWidth, s.position.y + s.halfHeight};
}
inline bool validState(const State& s, const World& w) {
    const double vals[] = {s.position.x,s.position.y,s.velocity.x,s.velocity.y,
        s.halfWidth,s.halfHeight,w.gravity,w.jumpSpeed,w.shipThrust,w.waveSpeed};
    for (double v : vals) if (!std::isfinite(v)) return false;
    if (s.halfWidth <= 0 || s.halfHeight <= 0 || w.jumpSpeed < 0 ||
        w.shipThrust < 0 || w.waveSpeed < 0) return false;
    for (const auto& r : w.hazards) if (!validRect(r)) return false;
    for (const auto& r : w.solids) if (!validRect(r)) return false;
    return true;
}
// Conservative swept-AABB: includes both endpoints and the space between.
// This prevents tunnelling past thin hazards during a simulation step.
inline bool sweptHit(const Rect& before, const Rect& after, const Rect& obstacle) {
    Rect swept{std::min(before.left,after.left),std::min(before.bottom,after.bottom),
               std::max(before.right,after.right),std::max(before.top,after.top)};
    return overlap(swept, obstacle);
}
// Approximate reference simulator, NOT Geometry Dash's exact physics.
// Unmodelled portals, pads, or mode-specific collision rules must cause the
// game adapter to reject the prediction, not treat it as safe.
inline Simulation simulate(State state, const World& world, double pressDelay,
                           double horizon = 0.25, double dt = 1.0/240.0) {
    if (!validState(state,world) || !std::isfinite(pressDelay) ||
        !std::isfinite(horizon) || !std::isfinite(dt) ||
        pressDelay < 0 || horizon <= 0 || horizon > 2 || dt <= 0 || dt > 0.02)
        return {};
    const double sign = state.gravityFlipped ? -1.0 : 1.0;
    bool pressed = false;
    double t = 0;
    while (t < horizon) {
        double step = std::min(dt,horizon-t);
        if (!pressed && t + step > pressDelay) step = pressDelay-t;
        if (!pressed && t >= pressDelay - 1e-10) {
            pressed = true;
            switch (state.mode) {
                case Mode::Cube: case Mode::Ball: case Mode::UFO:
                case Mode::Robot: case Mode::Spider:
                    if (state.grounded || state.mode == Mode::UFO)
                        state.velocity.y = sign*world.jumpSpeed;
                    break;
                case Mode::Ship: case Mode::Wave: break;
            }
        }
        if (step < 1e-12) { if (t >= horizon-1e-12) break; continue; }
        Rect old = bounds(state);
        if (state.mode == Mode::Wave) {
            state.velocity.y = (pressed ? 1.0 : -1.0)*sign*world.waveSpeed;
        } else {
            state.velocity.y += (world.gravity*sign +
                (state.mode == Mode::Ship && pressed ? sign*world.shipThrust : 0))*step;
        }
        state.position.x += state.velocity.x*step;
        state.position.y += state.velocity.y*step;
        Rect next = bounds(state);
        for (const auto& hazard : world.hazards)
            if (sweptHit(old,next,hazard)) return {true,false,state.position.x};
        // Solid collisions are conservatively treated as blocking, not as safe
        // landings: this model cannot yet resolve Geometry Dash collision rules.
        for (const auto& solid : world.solids)
            if (sweptHit(old,next,solid)) return {false,false,state.position.x};
        t += step;
    }
    return {true,true,state.position.x};
}
inline Decision planJump(const State& state, const World& world,
                         double maxDelay = 0.12, double horizon = 0.25) {
    std::vector<Candidate> candidates;
    for (int i=0;i<=12;++i) {
        double delay = maxDelay * i / 12.0;
        Simulation result = simulate(state,world,delay,horizon);
        candidates.push_back({delay,result.valid,result.survives,result.progress});
    }
    // A single unknown collision makes the model insufficient to certify
    // an alternative timing; no correction is safer than a guessed one.
    for (const auto& c : candidates) if (!c.simulationValid) return {};
    return chooseTiming(candidates,maxDelay);
}
} // namespace smartjump
