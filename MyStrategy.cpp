#include "MyStrategy.hpp"
#include "gameUtils.hpp"
#include <iostream>

MyStrategy::MyStrategy() {}

UnitAction MyStrategy::getAction(const Unit &unit, const Game &game,
                                 Debug &debug) {

    if (curTick == -1)
    {
        myLevel = convertLevel(game);
    }

    if (curTick != game.currentTick)
    {
        TimeMeasure measure(t);

        {
            Simulator sim = convertSimulator(&myLevel, game, unit.playerId);
            sim.rnd.seed(curTick);
            strat.compute(sim);

            curTick = game.currentTick;
        }

        if (curTick % 100 == 99)
        {
            std::cout << curTick << "| T " << t << std::endl;
        }
    }

    for (const MyUnit &r : strat.sim.units)
    {
        if (r.side == Side::ME && r.id == unit.id)
        {
            UnitAction result;
            result.velocity = r.action.velocity;
            result.jump = r.action.jump;
            result.jumpDown = r.action.jumpDown;
            result.aim.x = r.action.aim.x;
            result.aim.y = r.action.aim.y;
            result.shoot = r.action.shoot;
            result.swapWeapon = r.action.swapWeapon;
            result.plantMine = r.action.plantMine;
            result.reload = r.action.reload;

            //LOG("ACTION " << r.id << " " << r.action.target_velocity << " " << r.action.jump_speed << " " << r.action.use_nitro);
            return result;
        }
    }

    LOG("ERRR");

    return UnitAction();
}