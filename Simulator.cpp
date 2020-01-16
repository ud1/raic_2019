#include "Simulator.hpp"

MyWeaponParams WEAPON_PARAMS[3];

bool isOnLadder(const MyUnit &u, const Simulator &sim)
{
    Tile tile1 = sim.level->getTile(u.pos.x + EPS, u.pos.y);
    Tile tile2 = sim.level->getTile(u.pos.x - EPS, u.pos.y);
    if (tile1 == LADDER && tile2 == LADDER)
        return true;

    tile1 = sim.level->getTile(u.pos.x + EPS, u.pos.y + sim.level->unitH05 + EPS);
    tile2 = sim.level->getTile(u.pos.x - EPS, u.pos.y + sim.level->unitH05 + EPS);
    return tile1 == LADDER && tile2 == LADDER;
}

bool checkTile(Tile tile, bool onLadder, bool jumpDown)
{
    if (tile == WALL)
        return true;

    if (!jumpDown)
    {
        if (tile == PLATFORM)
            return true;
    }

    return false;
}

bool bulletLevelCollision(const MyLevel *level, const MyBullet &b)
{
    BBox bb = b.getBBox();

    int ix = b.pos.x;
    int iy = b.pos.y;

    for (int y = iy - 1; y <= iy + 1; ++y)
    {
        if (y >= 0 && y < level->h)
        {
            for (int x = ix -1; x <= ix + 1; ++x)
            {
                if (x >= 0 && x < level->w)
                {
                    Tile tile = level->getTile(x, y);
                    if (tile == WALL)
                    {
                        BBox tb = BBox::cubeAt(x, y);
                        if (bb.intersects(tb))
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

void damageUnit(Side side, MyUnit &u, Simulator &sim, int bulletDamage, int firedUnitId)
{
    int damage = std::min(bulletDamage, u.health);
    u.health -= damage;

    if (side != u.side)
    {
        if (side == Side::ME)
            sim.score[0] += damage;
        else
            sim.score[1] += damage;
    }
    else
    {
        if (u.id != firedUnitId)
            sim.friendlyFireByUnitId[firedUnitId] += bulletDamage;

        if (side == Side::ME)
            sim.selfFire[0] += damage;
        else
            sim.selfFire[1] += damage;
    }
}

int microticsDir(P p, P vel, double microtickT)
{
    double resX = 1000;
    double resY = 1000;

    double fx = std::floor(p.x);

    if (fx == p.x && vel.x != 0)
        return 1;

    double fy = std::floor(p.y);

    if (fy == p.y && vel.y != 0)
        return 1;

    if (vel.x > 0)
    {
        resX = (fx + 1.0 - p.x) / vel.x;
    }
    else if (vel.x < 0)
    {
        resX = (fx - p.x) / vel.x;
    }

    if (vel.y > 0)
    {
        resY = (fy + 1.0 - p.y) / vel.y;
    }
    else if (vel.y < 0)
    {
        resY = (fy - p.y) / vel.y;
    }

    double res = std::min(resX, resY);

    return res / microtickT;
}

int microticsDir(const BBox &b, P vel, double microtickT)
{
    int t1 = microticsDir(P(b.minP.x, b.minP.y), vel, microtickT);
    if (t1 <= 1)
        return 1;
    int t2 = microticsDir(P(b.minP.x, b.maxP.y), vel, microtickT);
    if (t2 <= 1)
        return 1;
    int t3 = microticsDir(P(b.maxP.x, b.maxP.y), vel, microtickT);
    if (t3 <= 1)
        return 1;
    int t4 = microticsDir(P(b.maxP.x, b.minP.y), vel, microtickT);
    if (t4 <= 1)
        return 1;

    return std::min(std::min(t1, t2), std::min(t3, t4));
}

void Simulator::tickNoOpt()
{
    for (MyUnit &u : units)
    {
        u.action.velocity = clamp(u.action.velocity, -level->properties.unitMaxHorizontalSpeed, level->properties.unitMaxHorizontalSpeed);
    }

    for (int mt = 0; mt < level->properties.updatesPerTick; ++mt)
    {
        microtick(mt, 1);
    }
}

void Simulator::tick()
{
    for (MyUnit &u : units)
    {
        u.action.velocity = clamp(u.action.velocity, -level->properties.unitMaxHorizontalSpeed, level->properties.unitMaxHorizontalSpeed);
    }

    if (level->properties.updatesPerTick > 1)
    {
        int microticksLeft = level->properties.updatesPerTick;

        while (microticksLeft > 0)
        {
            microtick(0, 1);
            microticksLeft--;

            if (microticksLeft == 0)
                break;

            int microticksToSimulate = microticksLeft;

            if (microticksToSimulate > 1)
            {
                for (const MyMine &m : mines)
                {
                    if (m.state == PREPARING || m.state == TRIGGERED)
                    {
                        if (m.timerMicroticks < microticksToSimulate)
                            microticksToSimulate = m.timerMicroticks;
                    }
                }
            }

            for (const MyBullet &b : bullets)
            {
                if (microticksToSimulate > 1)
                {
                    microticksToSimulate = std::min(microticksToSimulate, microticsDir(b.getBBox(), b.vel, level->microtickT));
                }
                else
                {
                    break;
                }
            }

            for (const MyUnit &u : units)
            {
                if (u.weapon)
                {
                    if (u.action.shoot && u.weapon.fireTimerMicrotics == 0)
                        microticksToSimulate = 1;
                    else if (u.weapon.fireTimerMicrotics > 0)
                        microticksToSimulate = std::min(microticksToSimulate, u.weapon.fireTimerMicrotics);
                }

                if (u.maxTimeMicrotics > 0)
                    microticksToSimulate = std::min(microticksToSimulate, u.maxTimeMicrotics);
            }

            for (const MyUnit &u : units)
            {
                if (microticksToSimulate > 1)
                {
                    P vel = u.pos - u._oldPos;
                    microticksToSimulate = std::min(microticksToSimulate, microticsDir(u.getBBox(), vel, 1));

                    if (microticksToSimulate > 1)
                    {
                        microticksToSimulate = std::min(microticksToSimulate, microticsDir(u.getCenter(), vel, 1)); // for ladder
                    }
                }
                else
                {
                    break;
                }
            }

            if (microticksToSimulate <= 0)
                microticksToSimulate = 1;

            // TODO mines & lootboxes

            microtick(level->properties.updatesPerTick - microticksLeft, microticksToSimulate);
            microticksLeft -= microticksToSimulate;
        }
    }
    else
    {
        for (int mt = 0; mt < level->properties.updatesPerTick; ++mt)
        {
            microtick(mt, 1);
        }
    }
}

void Simulator::microtick(int subtickInd, int microtickCount)
{
    //LOG("WC "<< microtickCount);
    int mc = microtickCount;

    const double w05 = level->unitW05;
    const double dt = level->microtickT * microtickCount;

    /*if (curTick >= 6723)
    {
        LOG("A");
    }*/

    for (MyUnit &unit : units)
    {
        unit._oldPos = unit.pos;

        if (unit.weapon)
        {
            if (subtickInd == 0)
            {
                if (unit.action.aim.len2() > 0.25)
                {
                    if (unit.weapon.lastAngle == P(0, 0))
                    {
                        unit.weapon.lastAngle = unit.action.aim.norm();
                    }
                    else if (unit.weapon.lastAngle != unit.action.aim)
                    {
                        double delta = normalizeAngle(unit.weapon.lastAngle.getAngle() - unit.action.aim.getAngle());
                        unit.weapon.lastAngle = unit.action.aim.norm();

                        unit.weapon.spread += std::abs(delta);

                        if (unit.weapon.spread > unit.weapon.params->maxSpread)
                            unit.weapon.spread = unit.weapon.params->maxSpread;
                    }
                }
            }

            if (unit.action.shoot && unit.weapon.fireTimerMicrotics == 0)
            {
                MyBullet b;
                b.pos = unit.getCenter();
                b.unitId = unit.id;
                b.side = unit.side;


                if (enFireAtCenter && unit.isEnemy())
                {
                    b.vel = unit.weapon.lastAngle * unit.weapon.params->bulletSpeed;
                }
                else if (myRLFireDir >= 0 && unit.isMine() && unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                {
                    P angle = myRLFireDir == 0 ? P(-unit.weapon.spread) : P(unit.weapon.spread);
                    b.vel = unit.weapon.lastAngle.rotate(angle) * unit.weapon.params->bulletSpeed;
                }
                else
                {
                    P angle = P(getRandom() * unit.weapon.spread);
                    b.vel = unit.weapon.lastAngle.rotate(angle) * unit.weapon.params->bulletSpeed;
                }
                b.size05 = unit.weapon.params->bulletSize * 0.5;
                b.params = unit.weapon.params;
                b.weaponType = unit.weapon.weaponType;

                unit.weapon.lastFireTick = curTick;
                if (unit.weapon.magazine > 1)
                {
                    unit.weapon.magazine--;
                    unit.weapon.fireTimerMicrotics = unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    unit.weapon.magazine = unit.weapon.params->magazineSize;
                    unit.weapon.fireTimerMicrotics = unit.weapon.params->reloadTimeMicrotiks;
                }

                unit.weapon.spread += unit.weapon.params->recoil;

                bullets.push_back(b);
            }

            if (unit.action.reload && unit.weapon.magazine < unit.weapon.params->magazineSize)
            {
                unit.weapon.magazine = unit.weapon.params->magazineSize;
                unit.weapon.fireTimerMicrotics = unit.weapon.params->reloadTimeMicrotiks;
            }

            if (unit.weapon.spread > unit.weapon.params->maxSpread)
                unit.weapon.spread = unit.weapon.params->maxSpread;

            unit.weapon.spread -= unit.weapon.params->aimSpeed * dt;

            if (unit.weapon.spread < unit.weapon.params->minSpread)
                unit.weapon.spread = unit.weapon.params->minSpread;
        }

        if (unit.action.plantMine && unit.mines > 0)
        {
            bool isJump = unit.canCancel ? unit.canJump && unit.action.jump : unit.canJump;
            bool isJumpDown = !isJump && (!unit.onLadder || unit.action.jumpDown);

            double dy = unit.pos.y - std::floor(unit.pos.y);

            if (dy < 0.01)
            {
                if (!isJump && isJumpDown)
                {
                    int y = unit.pos.y;
                    if (y > 0)
                    {
                        Tile t = level->getTile(unit.pos.x, y - 1);
                        if (t == WALL || t == PLATFORM)
                        {
                            unit.mines--;
                            unit.action.plantMine = false;

                            MyMine mine;
                            mine.side = unit.side;
                            mine.pos = unit.pos;
                            mine.size = P(level->properties.mineSize.x, level->properties.mineSize.y);
                            mine.triggerRadius = level->properties.mineTriggerRadius;
                            mine.state = PREPARING;
                            mine.timerMicroticks = level->toMicroticks(level->properties.minePrepareTime);
                            mines.push_back(mine);
                        }
                    }
                }
            }
        }
    }

    while (microtickCount > 0)
    {
        int microtickRound = microtickCount;
        if (microtickCount > 1)
        {
            for (MyBullet &b : bullets)
            {
                for (MyUnit &u : units)
                {

                    if (microtickCount > 1)
                    {
                        BBox unitBBox = u.getBBox();
                        unitBBox.expand(b.size05);

                        P relVel = b.vel - u._vel;
                        std::pair<bool, P> col = unitBBox.rayIntersection(b.pos, relVel);

                        if (col.first)
                        {
                            microtickRound = std::min(microtickRound, (int) std::ceil(std::sqrt(col.second.dist2(b.pos) / relVel.len2()) / level->microtickT));

                            if (microtickRound <= 0)
                                microtickRound = 1;

                            //if (microtickRound < microtickCount)
                            //    LOG("MM " << microtickRound << " " << microtickCount);
                        }
                    }
                }

                for (MyMine &m : mines)
                {
                    if (microtickCount > 1)
                    {
                        BBox mineBBox = m.getBBox();
                        mineBBox.expand(b.size05);


                        std::pair<bool, P> col = mineBBox.rayIntersection(b.pos, b.vel);

                        if (col.first)
                        {
                            microtickRound = std::min(microtickRound, (int) std::ceil(std::sqrt(col.second.dist2(b.pos) / b.vel.len2()) / level->microtickT));

                            if (microtickRound <= 0)
                                microtickRound = 1;

                            //if (microtickRound < microtickCount)
                            //    LOG("MM " << microtickRound << " " << microtickCount);
                        }
                    }
                }
            }
        }



        double rDt = level->microtickT * microtickRound;

        for (MyUnit &unit : units)
        {
            if (unit.action.velocity != 0)
            {
                double x = unit.pos.x + unit.action.velocity * rDt;

                int ix;
                if (unit.action.velocity > 0)
                {
                    ix = (int) (unit.pos.x + level->unitW05 - EPS) + 1;
                    if (ix >= level->w)
                        ix = level->w - 1;
                }
                else
                {
                    ix = (int) (unit.pos.x - level->unitW05 + EPS) - 1;
                    if (ix < 0)
                        ix = 0;
                }

                for (int y = unit.pos.y; y <= (unit.pos.y + unit.size.y); ++y)
                {
                    if (y >= 0 && y <= level->h)
                    {
                        if (unit.action.velocity > 0)
                        {
                            if (level->getTile(ix, y) == WALL)
                            {
                                x = std::min(x, ix - w05 - EPS);
                            }
                        }
                        else
                        {
                            if (ix < 0 || level->getTile(ix, y) == WALL)
                            {
                                x = std::max(x, ix + 1.0 + w05 + EPS);
                            }
                        }
                    }
                }

                BBox b1 = unit.getBBox();

                double dx = x - unit.pos.x;
                for (const MyUnit &u2 : units)
                {
                    if (&u2 != &unit)
                    {
                        BBox b2 = u2.getBBox();
                        dx = b2.calculateXOffset(b1, dx);
                    }
                }

                unit.pos.x += dx;
            }
        }

        for (MyUnit &unit : units)
        {
            bool isJump = unit.canCancel ? unit.canJump && unit.action.jump : unit.canJump;
            bool isJumpDown = !isJump && (!unit.onLadder || unit.action.jumpDown);

            bool hit = false;
            if (isJump || isJumpDown)
            {
                double ySpd = 0;
                if (isJump)
                {
                    if (unit.canCancel)
                        ySpd = level->properties.unitJumpSpeed;
                    else
                        ySpd = level->properties.jumpPadJumpSpeed;
                }
                else
                {
                    ySpd = -level->properties.unitFallSpeed;
                }

                /*if (unit.id == 0 && unit.pos.x > 17.35 && unit.pos.x < 17.40 && unit.pos.y < 3.014 && ySpd < 0)
                {
                        LOG("AAA");
                }*/

                double y = unit.pos.y + ySpd * rDt;

                int iy;

                //P saveP = unit.pos;

                if (isJump)
                {
                    iy = (int) (unit.pos.y + level->properties.unitSize.y - 0.01) + 1;
                }
                else
                {
                    iy = (int) unit.pos.y - 1;
                }

                for (int x = unit.pos.x - level->unitW05; x <= unit.pos.x + level->unitW05; ++x)
                {
                    if (x >= 0 && x <= level->w)
                    {
                        if (isJump)
                        {
                            if (iy >= level->h || level->getTile(x, iy) == WALL)
                            {
                                double ny = iy - level->properties.unitSize.y;
                                if (y >= ny)
                                {
                                    hit = true;
                                    y = ny - EPS;
                                }
                            }
                        }
                        else
                        {

                            if (iy < 0 || checkTile(level->getTile(x, iy), unit.onLadder, unit.action.jumpDown))
                            {
                                double ny = iy + 1.0;
                                if (y <= ny)
                                {
                                    hit = true;
                                    y = ny + EPS;
                                }
                            }
                        }
                    }
                }

                if (!hit && !isJump && !unit.action.jumpDown && !unit.onLadder)
                {
                    if (level->getTile(unit.pos.x, iy) == LADDER)
                    {
                        double ny = iy + 1.0;
                        if (y <= ny)
                        {
                            hit = true;
                            y = ny + EPS;

                            assert(y >= 1);
                        }
                    }
                }

                BBox b1 = unit.getBBox();

                double dy = y - unit.pos.y;
                double saveDy = dy;

                if (std::copysign(1.0, ySpd) == std::copysign(1.0, dy))
                {
                    for (const MyUnit &u2 : units)
                    {
                        if (&u2 != &unit)
                        {
                            BBox b2 = u2.getBBox();
                            dy = b2.calculateYOffset(b1, dy);
                        }
                    }
                }

                /*if (unit.id == 0 && unit.pos.x > 16 && unit.pos.x < 24 && unit.pos.y < 3 && ySpd < 0)
                {
                    LOG("BBB");
                }*/

                //assert(unit.pos.y >= 1);
                double prevY = unit.pos.y;
                unit.pos.y += dy;

                if (dy < 0)
                {
                    int iy = (int) prevY - 1;
                    for (int x = unit.pos.x - level->unitW05; x <= unit.pos.x + level->unitW05; ++x)
                    {
                        if (iy < 0 || checkTile(level->getTile(x, iy), unit.onLadder, unit.action.jumpDown))
                        {
                            double ny = iy + 1.0;
                            if (unit.pos.y < ny)
                            {
                                hit = true;
                                unit.pos.y = ny;

                                //LOG("FIX " << prevY << " " << unit.pos.y << " " << (unit.pos.y - prevY));
                            }
                        }
                    }
                }

                /*if (unit.id == 0 && unit.pos.x > 16 && unit.pos.x < 24 && unit.pos.y < 3 && ySpd < 0)
                {
                    LOG("CCC");
                }*/


                //assert(unit.pos.y >= 1);
                if (std::abs(dy) < std::abs(saveDy))
                    hit = true;
            }

            unit.onLadder = isOnLadder(unit, *this);
            if (unit.onLadder)
            {
                unit.canJump = true;
                unit.canCancel = true;
                unit.onGround = true;
                unit.maxTimeMicrotics = level->unitJumpTimeMicrotics;
            }
            else
            {
                if (hit)
                {
                    if (isJump)
                    {
                        unit.canJump = false;
                        unit.canCancel = false;
                        unit.onGround = false;
                        unit.maxTimeMicrotics = 0;
                    }
                    else if (isJumpDown)
                    {
                        unit.canJump = true;
                        unit.canCancel = true;
                        unit.onGround = true;
                        unit.maxTimeMicrotics = level->unitJumpTimeMicrotics;
                    }
                }
                else
                {
                    unit.onGround = false;

                    if (!isJump)
                        unit.maxTimeMicrotics = 0;

                    if (unit.maxTimeMicrotics > 0)
                    {
                        assert(unit.maxTimeMicrotics >= microtickRound);
                        unit.maxTimeMicrotics -= microtickRound;
                    }
                    else
                    {
                        unit.canJump = false;
                        unit.canCancel = false;
                    }
                }
            }
        }

        for (MyUnit &unit : units)
        {
            bool isJumpad = false;
            BBox ub;
            ub.minP = unit.pos - P(level->unitW05, 0);
            ub.maxP = unit.pos + P(level->unitW05, level->properties.unitSize.y);

            for (int y = ub.minP.y; y <= ub.maxP.y; ++y)
            {
                for (int x = ub.minP.x; x <= ub.maxP.x; ++x)
                {
                    Tile t = level->getTile(x, y);
                    if (t == JUMP_PAD)
                    {
                        BBox cb = BBox::cubeAt(x, y);
                        if (ub.intersects(cb))
                        {
                            isJumpad = true;
                            break;
                        }
                    }
                }
            }

            if (isJumpad)
            {
                unit.canJump = true;
                unit.canCancel = false;
                unit.maxTimeMicrotics = level->jumpPadJumpTimeMicrotics;
            }

            for (auto l_it = lootBoxes.begin(); l_it != lootBoxes.end();)
            {
                MyLootBox &l = *l_it;
                bool consumed = false;

                if (ub.intersects(l.getBBox()))
                {
                    if (l.type == MyLootBoxType ::WEAPON && (!unit.weapon || unit.action.swapWeapon))
                    {
                        unit.action.swapWeapon = false;
                        MyWeaponType old = unit.weapon.weaponType;

                        unit.weapon.weaponType = l.weaponType;
                        unit.weapon.params = getWeaponParams(unit.weapon.weaponType);
                        unit.weapon.magazine = unit.weapon.params->magazineSize;
                        unit.weapon.wasShooting = false;
                        unit.weapon.spread = unit.weapon.params->minSpread;
                        unit.weapon.fireTimerMicrotics = unit.weapon.params->reloadTimeMicrotiks;
                        unit.weapon.lastFireTick = -1;
                        unit.weapon.lastAngle = P(0, 0);

                        if (old != MyWeaponType ::NONE)
                        {
                            l.weaponType = old;
                        }
                        else
                        {
                            consumed = true;
                        }
                    }
                    else if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        if (unit.health < level->properties.unitMaxHealth)
                        {
                            unit.health = std::min(unit.health + l.health, level->properties.unitMaxHealth);
                            consumed = true;
                        }
                    }
                    else if (l.type == MyLootBoxType ::MINE)
                    {
                        unit.mines++;
                        consumed = true;
                    }
                }

                if (consumed)
                {
                    l_it = lootBoxes.erase(l_it);
                }
                else
                {
                    ++l_it;
                }
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics > 0)
                {
                    assert(unit.weapon.fireTimerMicrotics >= microtickRound);
                    unit.weapon.fireTimerMicrotics -= microtickRound;
                }
            }
        }

        for (auto it = bullets.begin(); it != bullets.end();)
        {
            MyBullet &b = *it;

            P oldPos = b.pos;

            b.pos += b.vel * rDt;

            bool erase = false;
            if (bulletLevelCollision(level, b))
            {
                erase = true;
            }

            if (!erase)
            {
                for (MyMine &m : mines)
                {
                    if (m.getBBox().intersects(b.getBBox()))
                    {
                        erase = true;
                        m.timerMicroticks = 0;
                        m.state = TRIGGERED;
                    }
                }
            }

            if (!erase)
            {
                for (MyUnit &u : units)
                {
                    if (u.id != b.unitId && u.getBBox().intersects(b.getBBox()))
                    {
                        damageUnit(b.side, u, *this, b.params->bulletDamage, b.unitId);

                        erase = true;
                        break;
                    }
                }
            }

            if (erase)
            {
                if (b.params->explDamage)
                {
                    BBox eb = b.getExplosionBBox();

                    for (MyUnit &u : units)
                    {
                        if (u.getBBox().intersects(eb))
                        {
                            damageUnit(b.side, u, *this, b.params->explDamage, b.unitId);
                        }
                    }

                    for (MyMine &m : mines)
                    {
                        if (m.getBBox().intersects(eb))
                        {
                            m.state = TRIGGERED;
                            m.timerMicroticks = 0;
                        }
                    }
                }

                it = bullets.erase(it);
            }
            else
            {
                ++it;
            }
        }

        microtickCount -= microtickRound;
    }

    bool minesFinished = false;

    while (!minesFinished)
    {
        minesFinished = true;

        for (MyMine &m : mines)
        {
            if (m.state == PREPARING)
            {
                if (m.timerMicroticks > 0)
                {
                    assert(m.timerMicroticks >= mc);
                    m.timerMicroticks -= mc;
                }
                else
                {
                    m.state = IDLE;
                }
            }
            else if (m.state == TRIGGERED)
            {
                if (m.timerMicroticks > 0)
                {
                    assert(m.timerMicroticks >= mc);
                    m.timerMicroticks -= mc;
                }
                else
                {
                    m.state = EXPLODED;
                    minesFinished = false;

                    BBox explBox = BBox::fromCenterAndHalfSize(m.getCenter(), level->properties.mineExplosionParams.radius);

                    for (MyMine &othMine : mines)
                    {
                        if (othMine.state != EXPLODED)
                        {
                            if (othMine.getBBox().intersects(explBox))
                            {
                                othMine.state = TRIGGERED;
                                othMine.timerMicroticks = 0;
                            }
                        }
                    }

                    for (MyUnit &u : units)
                    {
                        if (u.getBBox().intersects(explBox))
                        {
                            damageUnit(m.side, u, *this, level->properties.mineExplosionParams.damage, -1);
                        }
                    }
                }
            }
            else if (m.state == IDLE)
            {
                BBox mb = m.getBBox();
                mb.expand(level->properties.mineTriggerRadius);

                for (MyUnit &u : units)
                {
                    if (u.getBBox().intersects(mb))
                    {
                        m.state = TRIGGERED;
                        m.timerMicroticks = level->toMicroticks(level->properties.mineTriggerTime);
                    }
                }
            }
        }
    }

    filterVector(mines, [](const MyMine &m){return m.state == EXPLODED;});

    for (auto it = units.begin(); it != units.end();)
    {
        MyUnit &u = *it;

        if (u.health <= 0)
        {
            if (u.side == Side::ME)
                score[1] += level->properties.killScore;
            else
                score[0] += level->properties.killScore;

            if (log)
            {
                LOG("DEAD " << curTick << " " << units.size() << " " << score[0] << " " << score[1]);
            }

            it = units.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (mc == 1)
    {
        for (MyUnit &u : units)
        {
            u._vel = (u.pos - u._oldPos) / level->microtickT;
        }
    }
}

P MyLevel::rayWallCollision(const P &p, const P &ray) const {
    IP ip = IP(p);

    int dx = (int) std::copysign(1.0, ray.x);
    int dy = (int) std::copysign(1.0, ray.y);

    P dp = P(dx > 0 ? 1 : 0, dy > 0 ? 1 : 0);
    P invRay = 1.0 / ray;

    P s = p;

    for (int i = 0; i < 100; ++i)
    {
        if (!isValid(ip))
            return s;

        if (getTile(ip) == WALL)
            return s;

        P border = P(ip.x, ip.y) + dp;
        P dt = (border - s) * invRay;

        if (dt.x < dt.y)
        {
            ip.x += dx;
            s += ray * dt.x;
        }
        else if (dt.x > dt.y)
        {
            ip.y += dy;
            s += ray * dt.y;
        }
        else
        {
            ip.x += dx;
            ip.y += dy;
            s += ray * dt.y;
        }
    }

    LOG("WALL COL ERROR");
    return P(0, 0);
}

P MyLevel::bboxWallCollision(const BBox &b, const P &ray) const
{
    P res;
    double len = 1e9;

    for (int i = 0; i < 4; ++i)
    {
        P p = b.getCorner(i);
        P col = rayWallCollision(p, ray);

        double l = col.dist2(p);
        if (l < len)
        {
            len = l;
            res = p;
        }
    }

    return res;
}

P MyLevel::bboxWallCollision2(const BBox &b, const P &ray) const
{
    double len2 = 1e9;
    P dp;

    for (int i = 0; i < 4; ++i)
    {
        P p = b.getCorner(i);
        P col = rayWallCollision(p, ray);

        double l = col.dist2(p);
        if (l < len2)
        {
            len2 = l;
            dp = col - p;
        }
    }

    return b.getCenter() + dp;
}