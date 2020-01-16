#ifndef RAIC2019_GAMEUTILS_HPP
#define RAIC2019_GAMEUTILS_HPP

#include "Simulator.hpp"

inline void initializeWeaponParams(MyLevel &myLevel)
{
    for (int wt = 0; wt < 3; ++wt)
    {
        WeaponParams params = myLevel.properties.weaponParams.find((WeaponType) wt)->second;
        MyWeaponParams *myParams = getWeaponParams((MyWeaponType) wt);

        myParams->magazineSize = params.magazineSize;
        myParams->fireRateMicrotiks = myLevel.toMicroticks(params.fireRate);
        myParams->reloadTimeMicrotiks = myLevel.toMicroticks(params.reloadTime);
        myParams->minSpread = params.minSpread;
        myParams->maxSpread = params.maxSpread;
        myParams->recoil = params.recoil;
        myParams->aimSpeed = params.aimSpeed;
        myParams->bulletSpeed = params.bullet.speed;
        myParams->bulletSize = params.bullet.size;
        myParams->bulletDamage = params.bullet.damage;

        if (params.explosion)
        {
            myParams->explRadius = params.explosion->radius;
            myParams->explDamage = params.explosion->damage;
        }
        else
        {
            myParams->explRadius = 0;
            myParams->explDamage = 0;
        }
    }
}

inline void initializeLevelFromProps(MyLevel &myLevel)
{
    myLevel.unitW05 = myLevel.properties.unitSize.x * 0.5;
    myLevel.unitH05 = myLevel.properties.unitSize.y * 0.5;

    myLevel.microtickT = 1.0 / (myLevel.properties.ticksPerSecond * myLevel.properties.updatesPerTick);
    myLevel.unitJumpTimeMicrotics = myLevel.toMicroticks(myLevel.properties.unitJumpTime);
    myLevel.jumpPadJumpTimeMicrotics = myLevel.toMicroticks(myLevel.properties.jumpPadJumpTime);

    initializeWeaponParams(myLevel);
}

inline MyLevel convertLevel(const Game &game)
{
    MyLevel myLevel;

    myLevel.properties = game.properties;

    myLevel.w = game.level.tiles.size();
    myLevel.h = game.level.tiles[0].size();
    myLevel.tiles.reserve(myLevel.w * myLevel.h);

    for (int y = 0; y < myLevel.h; ++y)
    {
        for (int x = 0; x < myLevel.w; ++x)
        {
            myLevel.tiles.push_back(game.level.tiles[x][y]);
        }
    }

    initializeLevelFromProps(myLevel);

    LOG("T " << game.currentTick);

    return myLevel;
}

inline Simulator convertSimulator(const MyLevel *level, const Game &game, int myPlayerId)
{
    Simulator sim;

    sim.level = level;
    sim.curTick = game.currentTick;

    for (const Unit &unit : game.units)
    {
        MyUnit myUnit;

        myUnit.playerId = unit.playerId;
        myUnit.id = unit.id;
        myUnit.health = unit.health;
        myUnit.pos.x = unit.position.x;
        myUnit.pos.y = unit.position.y;
        myUnit.size.x = unit.size.x;
        myUnit.size.y = unit.size.y;
        myUnit.canJump = unit.jumpState.canJump;
        myUnit.maxTimeMicrotics = level->toMicroticks(unit.jumpState.maxTime);
        myUnit.canCancel = unit.jumpState.canCancel;
        myUnit.walkedRight = unit.walkedRight;
        myUnit.stand = unit.stand;
        myUnit.onGround = unit.onGround;
        myUnit.onLadder = unit.onLadder;
        myUnit.mines = unit.mines;
        myUnit.side = unit.playerId == myPlayerId ? Side::ME : Side::EN;

        if (unit.weapon)
        {
            myUnit.weapon.weaponType = (MyWeaponType) unit.weapon->typ;
            myUnit.weapon.params = getWeaponParams(myUnit.weapon.weaponType);

            myUnit.weapon.magazine = unit.weapon->magazine;
            myUnit.weapon.wasShooting = unit.weapon->wasShooting;
            myUnit.weapon.spread = unit.weapon->spread;

            if (unit.weapon->fireTimer)
            {
                myUnit.weapon.fireTimerMicrotics = *unit.weapon->fireTimer / level->microtickT + 0.5;
            }
            else
            {
                myUnit.weapon.fireTimerMicrotics = 0;
            }

            if (unit.weapon->lastAngle)
            {
                myUnit.weapon.lastAngle = P(*unit.weapon->lastAngle);
            }
            else
            {
                myUnit.weapon.lastAngle = P(0, 0);
            }

            if (unit.weapon->lastFireTick)
            {
                myUnit.weapon.lastFireTick = *unit.weapon->lastFireTick;
            }
            else
            {
                myUnit.weapon.lastFireTick = -1;
            }
        }
        else
        {
            myUnit.weapon.weaponType = MyWeaponType ::NONE;
            myUnit.weapon.params = nullptr;
        }

        sim.units.push_back(myUnit);
    }

    for (const LootBox &lootBox : game.lootBoxes)
    {
        MyLootBox myLootBox;
        myLootBox.pos.x = lootBox.position.x;
        myLootBox.pos.y = lootBox.position.y;
        myLootBox.size.x = lootBox.size.x;
        myLootBox.size.y = lootBox.size.y;

        {
            const std::shared_ptr<Item::Weapon> weapon = std::dynamic_pointer_cast<Item::Weapon>(lootBox.item);
            if (weapon)
            {
                myLootBox.type = MyLootBoxType ::WEAPON;
                myLootBox.weaponType = (MyWeaponType) weapon->weaponType;
                sim.lootBoxes.push_back(myLootBox);
                continue;
            }
        }

        {
            const std::shared_ptr<Item::HealthPack> healthPack = std::dynamic_pointer_cast<Item::HealthPack>(lootBox.item);
            if (healthPack)
            {
                myLootBox.type = MyLootBoxType ::HEALTH_PACK;
                myLootBox.health = healthPack->health;
                sim.lootBoxes.push_back(myLootBox);
                continue;
            }
        }

        {
            const std::shared_ptr<Item::Mine> mine = std::dynamic_pointer_cast<Item::Mine>(lootBox.item);
            if (mine)
            {
                myLootBox.type = MyLootBoxType ::MINE;
                sim.lootBoxes.push_back(myLootBox);
                continue;
            }
        }
    }

    for (const Bullet &b : game.bullets)
    {
        MyBullet mb;
        mb.unitId = b.unitId;

        if (b.playerId == myPlayerId)
            mb.side = Side::ME;
        else
            mb.side = Side::EN;

        mb.weaponType = (MyWeaponType) b.weaponType;
        mb.pos = P(b.position.x, b.position.y);
        mb.vel = P(b.velocity.x, b.velocity.y);
        mb.params = getWeaponParams(mb.weaponType);
        mb.size05 = b.size * 0.5;

        sim.bullets.push_back(mb);
    }

    for (const Mine &m : game.mines)
    {
        MyMine mine;
        mine.pos = P(m.position.x, m.position.y);
        mine.size = P(m.size.x, m.size.y);
        mine.state = m.state;
        mine.triggerRadius = m.triggerRadius;
        mine.side = m.playerId == myPlayerId ? Side::ME : Side::EN;

        if (m.timer)
        {
            mine.timerMicroticks = level->toMicroticks(*m.timer);
        }
        else
        {
            mine.timerMicroticks = -1;
        }

        sim.mines.push_back(mine);
    }

    for (const Player &p : game.players)
    {
        if (p.id == myPlayerId)
            sim.score[0] = p.score;
        else
            sim.score[1] = p.score;
    }

    sim.curTick = game.currentTick;

    return sim;
}

#endif
