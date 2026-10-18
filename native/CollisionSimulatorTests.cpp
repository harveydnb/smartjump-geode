#include "CollisionSimulator.hpp"
#include <array>
#include <cassert>
int main() {
    using namespace smartjump;
    State player{{0,2},{10,0},0.4,0.4,Mode::Cube,true,false};
    std::array<Rect,1> hazards{{{1.2,0.0,1.4,1.0}}};
    World world{hazards,{}};
    auto noJump=simulate(player,world,0.3,0.2);
    assert(noJump.valid && !noJump.survives);
    auto jump=simulate(player,world,0.0,0.2);
    assert(jump.valid && jump.survives);
    State invalid=player; invalid.halfWidth=-1;
    assert(!simulate(invalid,world,0).valid);
    std::array<Rect,1> solids{{{1.2,0,1.4,1.0}}};
    World unknown{{},solids};
    auto collision=simulate(player,unknown,0.3,0.2);
    assert(!collision.valid);
    assert(!planJump(player,unknown).shouldDelay);
    for (auto mode : {Mode::Cube,Mode::Ship,Mode::Ball,Mode::UFO,
                      Mode::Wave,Mode::Robot,Mode::Spider}) {
        player.mode=mode;
        assert(simulate(player,World{},0.0).valid);
    }
}
