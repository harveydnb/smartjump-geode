#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
using namespace geode::prelude;

// Starter build: checks that the mod loads. No gameplay is changed yet.
class $modify(SmartJumpMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        log::info("Smart Jump prototype loaded. Smart jump and hitbox scaling are not implemented yet.");
        return true;
    }
};
