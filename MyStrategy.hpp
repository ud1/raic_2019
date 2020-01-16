#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Debug.hpp"
#include "model/CustomData.hpp"
#include "model/Game.hpp"
#include "model/Unit.hpp"
#include "model/UnitAction.hpp"
#include "Simulator.hpp"
#include "MyStrat.hpp"

class MyStrategy {
public:
    MyStrategy();
    UnitAction getAction(const Unit &unit, const Game &game, Debug &debug);

    MyLevel myLevel;
    MyStrat strat;
    int curTick = -1;
    double t = 0;
};

#endif