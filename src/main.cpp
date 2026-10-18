#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Minimal gameplay lifecycle hook. This intentionally does not change
// physics, collision boxes, verification checks, or player inputs.
class $modify(SmartJumpPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        log::info("Smart Jump: PlayLayer initialized; gameplay hook active (no physics changes)");
        return true;
    }
};

class $modify(SmartJumpMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        log::info("Smart Jump: prototype loaded. Jump correction and hitbox scaling remain disabled.");
        return true;
    }
};
