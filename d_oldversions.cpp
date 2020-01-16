#include "d_oldversions.hpp"
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <functional>

#ifdef WRITE_LOG
#undef WRITE_LOG
#endif

#ifdef LOG
#undef LOG
#endif

#define WRITE_LOG(x) ;
#define LOG(x) ;

#define renderLine(a, b, c);
#define renderPath(a, b);

void emptyStrat::MyStrat::compute(const Simulator &_sim) {
    this->sim = _sim;

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            unit.action = MyAction();
        }
    }
}

void quickStartGuy::MyStrat::compute(const Simulator &_sim) {
    this->sim = _sim;

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            const MyUnit *nearestEnemy = nullptr;
            for (const MyUnit &other : sim.units)
            {
                if (other.side == Side::EN)
                {
                    if (nearestEnemy == nullptr ||
                        unit.pos.dist2(other.pos) <
                        unit.pos.dist2(nearestEnemy->pos))
                    {
                        nearestEnemy = &other;
                    }
                }
            }
            const MyLootBox *nearestWeapon = nullptr;
            for (const MyLootBox &lootBox : sim.lootBoxes)
            {
                if (lootBox.type == MyLootBoxType::WEAPON)
                {
                    if (nearestWeapon == nullptr ||
                        unit.pos.dist2(lootBox.pos) <
                        unit.pos.dist2(nearestWeapon->pos))
                    {
                        nearestWeapon = &lootBox;
                    }
                }
            }
            P targetPos = unit.pos;
            if (!unit.weapon && nearestWeapon) {
                targetPos = nearestWeapon->pos;
            } else if (nearestEnemy) {
                targetPos = nearestEnemy->pos;
            }

            P aim = P(0, 0);
            if (nearestEnemy != nullptr) {
                aim = nearestEnemy->pos - unit.pos;
            }
            bool jump = targetPos.y > unit.pos.y;
            if (targetPos.x > unit.pos.x && sim.level->getTile(size_t(unit.pos.x + 1), size_t(unit.pos.y)) == Tile::WALL)
            {
                jump = true;
            }
            if (targetPos.x < unit.pos.x && sim.level->getTile(size_t(unit.pos.x - 1), size_t(unit.pos.y)) == Tile::WALL)
            {
                jump = true;
            }
            MyAction &action = unit.action;
            action.velocity = targetPos.x - unit.pos.x;
            action.jump = jump;
            action.jumpDown = !action.jump;
            action.aim = aim;
            action.shoot = true;
            action.swapWeapon = false;
            action.plantMine = false;
        }
    }
}

namespace v1 {

    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start)
        {
            w = sim.level->w;
            h = sim.level->h;
            distMap.clear();
            distMap.resize(w * h, -1);

            IP ip = IP(start);
            ip.limitWH(w, h);

            std::deque<IP> pts;
            pts.push_back(ip);

            distMap[ip.ind(w)] = 0;

            while (!pts.empty())
            {
                IP p = pts.front();
                pts.pop_front();

                double v = distMap[p.ind(w)] + 1;

                for (int y = p.y - 1; y <= p.y + 1; ++y)
                {
                    if (sim.level->isValidY(y))
                    {
                        for (int x = p.x - 1; x <= p.x + 1; ++x)
                        {
                            if (sim.level->isValidX(x))
                            {
                                int ind = IP(x, y).ind(w);

                                if (distMap[ind] < 0)
                                {
                                    Tile t = sim.level->getTile(x, y);
                                    if (t != WALL)
                                    {
                                        distMap[ind] = v;
                                        pts.push_back(IP(x, y));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        double getDist(P p) const
        {
            IP ip = IP(p);
            ip.limitWH(w, h);

            double res = distMap[ip.ind(w)];
            if (res < 0)
                res = 1000;

            return res;
        }
    };

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                std::map<int, DistanceMap> distMap;

                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;

                const MyUnit *nearestEnemy = nullptr;

                {
                    double score = -10000;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;

                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                sim.tick();
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {
                                    for (const MyUnit &other : this->sim.units)
                                    {
                                        if (other.side == Side::EN)
                                        {
                                            if (nearestEnemy == nullptr ||
                                                unit.pos.dist2(other.pos) <
                                                unit.pos.dist2(nearestEnemy->pos))
                                            {
                                                nearestEnemy = &other;
                                            }
                                        }
                                    }



                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore += (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore -= ((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5;

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        unit.action.aim = nearestEnemy->pos - unit.pos;

                        unit.action.shoot = true;
                    }

                    return;
                }
            }
        }
    }

}


namespace v2 {


    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed;
                }

                {
                    double score = -10000;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;

                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                sim.tick();

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore += (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore -= ((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5;

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);
                        unit.action.aim = (enemyPos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->rayWallCollision(center, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        /*{
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            //if (!anyCol)
                            //    unit.action.shoot = false;
                        }*/
                    }

                    return;
                }
            }
        }
    }
}


namespace v3 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed;
                }

                {
                    double score = -10000;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;

                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                sim.tick();

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore += (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore -= ((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5;

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);
                        unit.action.aim = (enemyPos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->rayWallCollision(center, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v4 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }



                {
                    double score = -10000;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;

                            P myFirePos = unit.pos;
                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                sim.tick();

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore += (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore -= ((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5;

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->rayWallCollision(center, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v5 {


    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }



                {
                    double score = -10000;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore += (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        lscore -= ((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5;

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->rayWallCollision(center, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v6 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                int bestDir = 0;
                int bestJumpS = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }



                {
                    double score = -10000;



                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "rigth ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }

                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;
                                lscore += (sim.score[0] - sim.score[1])/10.0;

                                WRITE_LOG("P " << newUnit->pos << " " << lscore << " (" << newUnit->health << " " << unit.health << ") " << moveName);

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                double wscore = (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                lscore += wscore;

                                                WRITE_LOG("WS " << wscore << " " << lscore);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e1sc = (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        lscore += e1sc;

                                        WRITE_LOG("E1S " << e1sc << " " << lscore);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e2sc = -(((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5);
                                        lscore += e2sc;

                                        WRITE_LOG("E2S " << e2sc << " " << lscore);

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestDir = dir;
                                    bestJumpS = jumpS;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    if (bestDir == 0)
                    {
                        unit.action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (bestDir == 1)
                    {
                        unit.action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (bestJumpS == 0)
                    {
                        unit.action.jump = true;
                    }
                    else if (bestJumpS == 1)
                    {
                        unit.action.jumpDown = true;
                    }

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v8 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    struct Move {
        double vel = 0;
        int jump = 0;
    };

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }



                {
                    double score = -10000;



                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }

                            Move firstMove;
                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }
                                else
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;
                                lscore += (sim.score[0] - sim.score[1])/10.0;

                                WRITE_LOG("P " << newUnit->pos << " " << lscore << " (" << newUnit->health << " " << unit.health << ") " << moveName);

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                double wscore = (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                lscore += wscore;

                                                WRITE_LOG("WS " << wscore << " " << lscore);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }

                                }
                                else
                                {




                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e1sc = (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        lscore += e1sc;

                                        WRITE_LOG("E1S " << e1sc << " " << lscore);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e2sc = -(((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5);
                                        lscore += e2sc;

                                        WRITE_LOG("E2S " << e2sc << " " << lscore);

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                if (lscore > score)
                                                    score = lscore;
                                            }
                                        }
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestMove = firstMove;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    unit.action.velocity = bestMove.vel;
                    if (bestMove.jump == 1)
                        unit.action.jump = true;
                    else if (bestMove.jump == -1)
                        unit.action.jumpDown = true;

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v9 {


    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    struct Move {
        double vel = 0;
        int jump = 0;
    };

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }



                {
                    double score = -10000;



                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }

                            Move firstMove;
                            for (int tick = 0; tick < 30; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }
                                else
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;
                                lscore += (sim.score[0] - sim.score[1])/10.0;

                                WRITE_LOG("P " << newUnit->pos << " " << lscore << " (" << newUnit->health << " " << unit.health << ") " << moveName);

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                double wscore = (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                lscore += wscore;

                                                WRITE_LOG("WS " << wscore << " " << lscore);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }

                                }
                                else
                                {

                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e1sc = (100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos);
                                        lscore += e1sc;

                                        WRITE_LOG("E1S " << e1sc << " " << lscore);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e2sc = -(((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5);
                                        lscore += e2sc;

                                        WRITE_LOG("E2S " << e2sc << " " << lscore);

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestMove = firstMove;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    unit.action.velocity = bestMove.vel;
                    if (bestMove.jump == 1)
                        unit.action.jump = true;
                    else if (bestMove.jump == -1)
                        unit.action.jumpDown = true;

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision2(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;
                        }
                    }

                    return;
                }
            }
        }
    }
}

namespace v10 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    struct Move {
        double vel = 0;
        int jump = 0;
    };

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {
                    double score = -10000;



                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }

                            Move firstMove;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }
                                else
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;
                                lscore += (sim.score[0] - sim.score[1])/10.0;

                                WRITE_LOG("P " << newUnit->pos << " " << lscore << " (" << newUnit->health << " " << unit.health << ") " << moveName);

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                double wscore = (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                lscore += wscore;

                                                WRITE_LOG("WS " << wscore << " " << lscore);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }

                                }
                                else
                                {

                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double keepDist = 0;
                                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER || nearestEnemy->weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                            keepDist = 5;

                                        double e1sc = (100 - dist) / 1000.0 - std::abs(keepDist - dmap.getDist(nearestEnemy->pos));
                                        lscore += e1sc;

                                        WRITE_LOG("E1S " << e1sc << " " << lscore);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e2sc = -(((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5);
                                        lscore += e2sc;

                                        WRITE_LOG("E2S " << e2sc << " " << lscore);

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestMove = firstMove;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    unit.action.velocity = bestMove.vel;
                    if (bestMove.jump == 1)
                        unit.action.jump = true;
                    else if (bestMove.jump == -1)
                        unit.action.jumpDown = true;

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision2(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;
                            int rays = 0;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            rays++;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;

                            WRITE_LOG("RAYS " << rays);
                        }
                    }

                    return;
                }
            }
        }
    }

}


namespace v11 {

    void DistanceMap::compute(const Simulator &sim, P start) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        IP ip = IP(start);
        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    struct Move {
        double vel = 0;
        int jump = 0;
    };

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {
                    double score = -10000;



                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < 3; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }

                            Move firstMove;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }
                                else
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }
                            }


                            MyUnit *newUnit = sim.getUnit(unit.id);

                            if (newUnit)
                            {
                                double lscore = newUnit->health - unit.health;
                                lscore += (sim.score[0] - sim.score[1])/10.0;

                                WRITE_LOG("P " << newUnit->pos << " " << lscore << " (" << newUnit->health << " " << unit.health << ") " << moveName);

                                int posInd = IP(newUnit->pos).ind(sim.level->w);
                                if (!distMap.count(posInd))
                                {
                                    distMap[posInd].compute(sim, newUnit->pos);
                                }

                                DistanceMap &dmap = distMap[posInd];

                                if (!unit.weapon)
                                {
                                    if (newUnit->weapon)
                                    {
                                        score = 1000;
                                    }
                                    else
                                    {
                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::WEAPON)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                double wscore = (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                                lscore += wscore;

                                                WRITE_LOG("WS " << wscore << " " << lscore);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }

                                }
                                else
                                {

                                    if (unit.health > 80 || (unit.health >= nearestEnemy->health + 20 || newUnit->health > nearestEnemy->health + 20))
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double keepDist = 0;
                                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER || nearestEnemy->weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                            keepDist = 5;
                                        else if (magazineRel < 0.5)
                                        {
                                            keepDist = 10;
                                        }

                                        double e1sc = (100 - dist) / 1000.0 - std::abs(keepDist - dmap.getDist(nearestEnemy->pos));
                                        lscore += e1sc;

                                        WRITE_LOG("E1S " << e1sc << " " << lscore);
                                        if (lscore > score)
                                            score = lscore;
                                    }
                                    else
                                    {
                                        double dist = nearestEnemy->pos.dist(newUnit->pos);

                                        double e2sc = -(((100 - dist) / 1000.0 - dmap.getDist(nearestEnemy->pos))/5);
                                        lscore += e2sc;

                                        WRITE_LOG("E2S " << e2sc << " " << lscore);

                                        for (const MyLootBox &l : _sim.lootBoxes)
                                        {
                                            if (l.type == MyLootBoxType ::HEALTH_PACK)
                                            {
                                                double dist = l.pos.dist(newUnit->pos);

                                                lscore += (100 - dist) / 1000.0 - dmap.getDist(l.pos);
                                            }
                                        }

                                        if (lscore > score)
                                            score = lscore;
                                    }
                                }

                                if (score > bestScore)
                                {
                                    bestMove = firstMove;
                                    bestScore = score;
                                    bestMyFirePos = myFirePos;

                                    WRITE_LOG("SC " << bestScore << " " << newUnit->health);
                                }
                            }
                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    unit.action.velocity = bestMove.vel;
                    if (bestMove.jump == 1)
                        unit.action.jump = true;
                    else if (bestMove.jump == -1)
                        unit.action.jumpDown = true;

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision2(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;
                            int rays = 0;

                            P center = unit.getCenter();

                            for (int i = 0; i < RAYS_N; ++i)
                            {
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            rays++;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;

                            WRITE_LOG("RAYS " << rays);
                        }
                    }

                    return;
                }
            }
        }
    }
}


namespace v12 {


    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }






    struct Move {
        double vel = 0;
        int jump = 0;
    };

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0;
        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (unit.health > 80 || unit.health >= en.health /*|| hpScore == 0*/)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon && unit.weapon.fireTimerMicrotics > en.weapon.fireTimerMicrotics)
                                    keepDist = 10;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }

                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || jumpS == 3 && tick > 0)
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                        if (newEnemy->weapon)
                                            action.aim = newEnemy->weapon.lastAngle;
                                    }
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }


                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestScore = score;
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                unit.action = MyAction();

                //if (bestScore > 0)
                {
                    unit.action.velocity = bestMove.vel;
                    if (bestMove.jump == 1)
                        unit.action.jump = true;
                    else if (bestMove.jump == -1)
                        unit.action.jumpDown = true;

                    if (nearestEnemy)
                    {
                        //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                        if (nearestEnemy->canJump)
                            enemyPos = nearestEnemy->pos;
                        P aim = (enemyPos - unit.pos).norm();
                        unit.action.aim = aim;

                        renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                        if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                        {
                            double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                            if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                            {
                                unit.action.aim = unit.weapon.lastAngle;
                                WRITE_LOG("OLD AIM")
                            }

                            WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                        }
                        //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                        //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                        unit.action.shoot = true;

                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = unit.weapon.params->explRadius;
                            if (enemyDist > explRad)
                            {
                                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                                {
                                    const int RAYS_N = 10;

                                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                    bool suicideDanger = false;
                                    int enemyHitRays = 0;

                                    P center = unit.getCenter();
                                    BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                    for (int i = 0; i < RAYS_N; ++i)
                                    {
                                        P tp = sim.level->bboxWallCollision2(missile, ray);

                                        if (tp.dist(unit.pos) < explRad * 1.2)
                                        {
                                            suicideDanger = true;
                                        }

                                        if (nearestEnemy)
                                        {
                                            bool enemyCol;
                                            P enemyColP;
                                            std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                            if (enemyCol)
                                            {
                                                double d1 = tp.dist2(center);
                                                double d2 = enemyColP.dist2(center);

                                                if (d2 < d1)
                                                {
                                                    enemyHitRays++;
                                                    //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                                }
                                            }
                                        }

                                        ray = ray.rotate(ddir);
                                    }

                                    if (suicideDanger)
                                        WRITE_LOG("SC DANGER " << enemyHitRays);

                                    if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                    {
                                        unit.action.shoot = false;
                                        unit.action.reload = true;
                                        //LOG("DSH");
                                    }
                                }
                            }
                        }

                        else
                        {
                            const int RAYS_N = 10;

                            P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                            P ray = unit.action.aim.rotate(-unit.weapon.spread);

                            bool anyCol = false;
                            int rays = 0;

                            P center = unit.getCenter();

                            //BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(unit.weapon.weaponType)->bulletSize * 0.5);

                            for (int i = 0; i < RAYS_N; ++i)
                            {

                                //P tp = sim.level->bboxWallCollision2(missile, ray);
                                P tp = sim.level->rayWallCollision(center, ray);

                                //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                                if (nearestEnemy)
                                {
                                    bool enemyCol;
                                    P enemyColP;
                                    std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                    if (enemyCol)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = enemyColP.dist2(center);

                                        if (d2 < d1)
                                        {
                                            anyCol = true;
                                            rays++;
                                            renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                        }
                                    }
                                }

                                ray = ray.rotate(ddir);
                            }

                            if (!anyCol)
                                unit.action.shoot = false;

                            WRITE_LOG("RAYS " << rays);
                        }
                    }

                    //return;
                }
            }
        }
    }



}



namespace v13 {


    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }




    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;
        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (unit.health > 80 || unit.health >= en.health /*|| hpScore == 0*/)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon && unit.weapon.fireTimerMicrotics > en.weapon.fireTimerMicrotics)
                                    keepDist = 10;

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        this->sim = _sim;



        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = nullptr;

                for (const MyUnit &other : this->sim.units)
                {
                    if (other.side == Side::EN)
                    {
                        if (nearestEnemy == nullptr ||
                            unit.pos.dist2(other.pos) <
                            unit.pos.dist2(nearestEnemy->pos))
                        {
                            nearestEnemy = &other;
                        }
                    }
                }

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                        action.aim = su.weapon.lastAngle;

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                }



                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                /*if (nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                    {
                                        MyAction &action = newEnemy->action;
                                        action.shoot = true;
                                        if (newEnemy->weapon)
                                            action.aim = newEnemy->weapon.lastAngle;
                                    }
                                }*/



                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);

                // ============

                if (nearestEnemy)
                {
                    //LOG("AIM " << nearestEnemy->pos << " | " << enemyPos);

                    if (nearestEnemy->canJump)
                        enemyPos = nearestEnemy->pos;
                    P aim = (enemyPos - unit.pos).norm();
                    unit.action.aim = aim;

                    renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                    if (unit.weapon && unit.weapon.lastAngle != P(0, 0))
                    {
                        double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                        if (angle < unit.weapon.spread - 2.0/(enemyPos - unit.pos).len())
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                            WRITE_LOG("OLD AIM")
                        }

                        WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                    }
                    //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                    //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                    unit.action.shoot = true;

                    if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                    {
                        double explRad = unit.weapon.params->explRadius;
                        if (enemyDist > explRad)
                        {
                            if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                            {
                                const int RAYS_N = 10;

                                P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                                P ray = unit.action.aim.rotate(-unit.weapon.spread);

                                bool suicideDanger = false;
                                int enemyHitRays = 0;

                                P center = unit.getCenter();
                                BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->bulletSize * 0.5);

                                for (int i = 0; i < RAYS_N; ++i)
                                {
                                    P tp = sim.level->bboxWallCollision2(missile, ray);

                                    if (tp.dist(unit.pos) < explRad * 1.2)
                                    {
                                        suicideDanger = true;
                                    }

                                    if (nearestEnemy)
                                    {
                                        bool enemyCol;
                                        P enemyColP;
                                        std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                        if (enemyCol)
                                        {
                                            double d1 = tp.dist2(center);
                                            double d2 = enemyColP.dist2(center);

                                            if (d2 < d1)
                                            {
                                                enemyHitRays++;
                                                //renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                            }
                                        }
                                    }

                                    ray = ray.rotate(ddir);
                                }

                                if (suicideDanger)
                                    WRITE_LOG("SC DANGER " << enemyHitRays);

                                if (suicideDanger && enemyHitRays < RAYS_N * 0.7)
                                {
                                    unit.action.shoot = false;
                                    unit.action.reload = true;
                                    //LOG("DSH");
                                }
                            }
                        }
                    }

                    else
                    {
                        const int RAYS_N = 10;

                        P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                        P ray = unit.action.aim.rotate(-unit.weapon.spread);

                        bool anyCol = false;
                        int rays = 0;

                        P center = unit.getCenter();

                        //BBox missile = BBox::fromCenterAndHalfSize(center, getWeaponParams(unit.weapon.weaponType)->bulletSize * 0.5);

                        for (int i = 0; i < RAYS_N; ++i)
                        {

                            //P tp = sim.level->bboxWallCollision2(missile, ray);
                            P tp = sim.level->rayWallCollision(center, ray);

                            //renderLine(unit.getCenter(), tp, 0xff0000ffu);

                            if (nearestEnemy)
                            {
                                bool enemyCol;
                                P enemyColP;
                                std::tie(enemyCol, enemyColP) = nearestEnemy->getBBox().rayIntersection(center, ray);

                                if (enemyCol)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = enemyColP.dist2(center);

                                    if (d2 < d1)
                                    {
                                        anyCol = true;
                                        rays++;
                                        renderLine(unit.getCenter(), enemyColP, 0x00ff00ffu);
                                    }
                                }
                            }

                            ray = ray.rotate(ddir);
                        }

                        if (!anyCol)
                            unit.action.shoot = false;

                        WRITE_LOG("RAYS " << rays);
                    }
                }

                if (friendlyFire > 20)
                {
                    //unit.action.shoot = false;
                    //LOG("AA" << friendlyFire);
                }


                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;

            }
        }
    }



}



namespace v16 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }




    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;
        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (unit.health > 80 || unit.health >= en.health /*|| hpScore == 0*/)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon && unit.weapon.fireTimerMicrotics > en.weapon.fireTimerMicrotics)
                                    keepDist = 10;

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit)
    {
        IP ipos = IP(unit.pos);

        DistanceMap &dmap = getDistMap(ipos, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side == Side::EN)
            {
                double dist = dmap.getDist(other.pos);
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    void computeFire(Simulator &sim, MyStrat &strat, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    if (angle < unit.weapon.spread - 1.0/(enemyPos - unit.pos).len())
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    const int RAYS_N = 10;

                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    int enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }

                            if (directHitUnit)
                            {
                                if (directHitUnit->isMine())
                                    myUnitsHits++;
                                else
                                    enUnitsHits++;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits == 0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/



        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                        action.aim = su.weapon.lastAngle;

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire(sim, *this, {});

                                if (!fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire(sim, *this, enemyPos, unit.id);
                    }
                    else
                    {
                        if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                if (angle < unit.weapon.spread - 1.0/(enemyPos - unit.pos).len())
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    //WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }



}



namespace v17 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }





    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;
        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                else if (en.weapon && unit.weapon.fireTimerMicrotics > en.weapon.fireTimerMicrotics)
                                    keepDist = 10; // TODO

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)) << " EP " << en.pos << " MP " << unit.pos);
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit)
    {
        IP ipos = IP(unit.pos);

        DistanceMap &dmap = getDistMap(ipos, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side == Side::EN)
            {
                double dist = dmap.getDist(other.pos);
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    void computeFire(Simulator &sim, MyStrat &strat, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    if (angle < unit.weapon.spread - 1.0/(enemyPos - unit.pos).len())
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    const int RAYS_N = 10;

                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    int enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }

                            if (directHitUnit)
                            {
                                if (directHitUnit->isMine())
                                    myUnitsHits++;
                                else
                                    enUnitsHits++;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits == 0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/



        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = enemyDist / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                        action.aim = su.weapon.lastAngle;

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire(sim, *this, {});


                                {
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }


                                if (!fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire(sim, *this, enemyPos, unit.id);
                    }
                    else
                    {
                        if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                if (angle < unit.weapon.spread - 1.0/(enemyPos - unit.pos).len())
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }



}


namespace v18 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        /*double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                DistanceMap &dmap = getDistMap(IP(l.pos), strat);

                double minDist = 1e9;
                Side side = Side::NONE;
                for (const MyUnit &unit : sim.units)
                {
                    double dist = dmap.getDist(unit.pos);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                    }
                }


                if (side == Side::ME)
                    lbScore += 5;
                else
                    lbScore -= 5;
            }
        }

        score += lbScore;
        WRITE_LOG("++++++++++++ " << lbScore);*/

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                    score -= 1;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit)
    {
        IP ipos = IP(unit.pos);

        DistanceMap &dmap = getDistMap(ipos, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side == Side::EN)
            {
                double dist = dmap.getDist(other.pos);
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    void computeFire(Simulator &sim, MyStrat &strat, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    double enDist = (enemyPos - unit.pos).len();
                    double dang = 1.0 / enDist;

                    if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    const int RAYS_N = 10;

                    P ddir = P(unit.weapon.spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-unit.weapon.spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/



        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    if (unit.weapon.fireTimerMicrotics > unit.weapon.params->reloadTimeMicrotiks)
                        magazineRel = 0;
                    else
                        magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                        action.aim = su.weapon.lastAngle;

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire(sim, *this, {});


                                {
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }


                                if (!fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                sim.tick();

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire(sim, *this, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                double enDist = (enemyPos - unit.pos).len();
                                double dang = 1.0 / enDist;

                                if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }

}

namespace v20 {


    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }



    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, IP lastWalkableP)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                DistanceMap &dmap = getDistMap(IP(l.pos), strat);

                double minDist = 1e9;
                Side side = Side::NONE;
                double deltaHealth = 0;
                for (const MyUnit &unit : sim.units)
                {
                    double dist = dmap.getDist(unit.pos);
                    if (unit.isMine())
                        dist *= 1.1;

                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                        deltaHealth = 100 - unit.health;
                    }
                }

                if (side == Side::ME)
                    lbScore += (deltaHealth * 0.1);
                else
                    lbScore -= (deltaHealth * 0.1);
            }
        }

        score += lbScore;
        WRITE_LOG("++++++++++++ " << lbScore);

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                    score -= 1;

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                // WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit)
    {
        IP ipos = IP(unit.pos);

        DistanceMap &dmap = getDistMap(ipos, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side != unit.side)
            {
                double dist = dmap.getDist(other.pos);
                if (unit.weapon)
                {
                    /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                    dist += (1.0 - dt) * 1.0;

                    dist += other.health * 0.1;*/
                }
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    template<int RAYS_N, bool log>
    void computeFire(Simulator &sim, MyStrat &strat, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                double spread = unit.weapon.spread;
                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    double enDist = (enemyPos - unit.pos).len();
                    double dang = 1.0 / enDist;

                    if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }
                    /*else
                    {
                        double delta = normalizeAngle(unit.weapon.lastAngle.getAngle() - unit.action.aim.getAngle());
                        unit.weapon.lastAngle = unit.action.aim.norm();
                        spread += std::abs(delta);

                        if (spread > unit.weapon.params->maxSpread)
                            spread = unit.weapon.params->maxSpread;
                    }*/

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/



        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            int health = unit.health;
                            //std::vector<Simulator> saveSim;

                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                    {
                                        action.aim = su.weapon.lastAngle;

                                        if (tick >= 5 && su.isEnemy())
                                        {
                                            const MyUnit *een = getNearestEnemy(sim, *this, su);
                                            if (een)
                                            {
                                                action.aim = P(een->pos - su.pos).norm();
                                            }
                                        }
                                    }

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire<10, false>(sim, *this, {});

                                {
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }


                                if (!fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                //saveSim.push_back(sim);
                                sim.tick();

                                {
                                    for (MyUnit &u : sim.units)
                                    {
                                        if (u.isEnemy())
                                        {
                                            renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                            renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                        }
                                    }
                                }

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    if (simUnit->health != health)
                                    {
                                        uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                        renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                        WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                        health = simUnit->health;
                                    }
                                    else
                                    {
                                        uint32_t  c = 0x000000ccu;
                                        renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                    }
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, ipos);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                /*if (tick < 3)
                                    coef *= 0.95;
                                else
                                    coef *= 0.9;*/
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire<10, true>(sim, *this, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                double enDist = (enemyPos - unit.pos).len();
                                double dang = 1.0 / enDist;

                                if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }
}

namespace v21 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMap &getDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
            return it->second;

        DistanceMap &res = strat.distMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                DistanceMap &dmap = getDistMap(IP(l.pos), strat);

                double minDist = 1e9;
                Side side = Side::NONE;
                double deltaHealth = 0;
                for (const MyUnit &unit : sim.units)
                {
                    double dist = dmap.getDist(unit.pos);
                    if (unit.isMine())
                        dist *= 1.1;

                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                        deltaHealth = 100 - unit.health;
                    }
                }

                if (side == Side::ME)
                    lbScore += (deltaHealth * 0.1);
                else
                    lbScore -= (deltaHealth * 0.1);
            }
        }

        score += lbScore;
        //WRITE_LOG("++++++++++++ " << lbScore);

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                    score += unit.health * 0.5;
                else
                    score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                {
                    score -= 1;
                }

                IP ipos = IP(unit.pos);

                DistanceMap &dmap = getDistMap(ipos, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dmap.getDist(l.pos);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dmap.getDist(l.pos);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dmap.getDist(en.pos);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                // WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit)
    {
        IP ipos = IP(unit.pos);

        DistanceMap &dmap = getDistMap(ipos, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side != unit.side)
            {
                double dist = dmap.getDist(other.pos);
                if (unit.weapon)
                {
                    /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                    dist += (1.0 - dt) * 1.0;

                    dist += other.health * 0.1;*/
                }
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    template<int RAYS_N, bool log>
    void computeFire(Simulator &sim, MyStrat &strat, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                double spread = unit.weapon.spread;
                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    double enDist = (enemyPos - unit.pos).len();
                    double dang = 1.0 / enDist;

                    if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }
                    /*else
                    {
                        double delta = normalizeAngle(unit.weapon.lastAngle.getAngle() - unit.action.aim.getAngle());
                        unit.weapon.lastAngle = unit.action.aim.norm();
                        spread += std::abs(delta);

                        if (spread > unit.weapon.params->maxSpread)
                            spread = unit.weapon.params->maxSpread;
                    }*/

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;

                /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
                {
                    unit.action.reload = true;
                }*/
            }
        }
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        bool isLoosing = sim.score[0] <= sim.score[1];

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            int health = unit.health;
                            //std::vector<Simulator> saveSim;

                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                    {
                                        action.aim = su.weapon.lastAngle;

                                        if (tick >= 5 && su.isEnemy())
                                        {
                                            const MyUnit *een = getNearestEnemy(sim, *this, su);
                                            if (een)
                                            {
                                                action.aim = P(een->pos - su.pos).norm();
                                            }
                                        }
                                    }

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire<10, false>(sim, *this, {});

                                {
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }


                                if (!fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                //saveSim.push_back(sim);
                                sim.tick();

                                {
                                    for (MyUnit &u : sim.units)
                                    {
                                        if (u.isEnemy())
                                        {
                                            renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                            renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                        }
                                    }
                                }

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    if (simUnit->health != health)
                                    {
                                        uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                        renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                        WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                        health = simUnit->health;
                                    }
                                    else
                                    {
                                        uint32_t  c = 0x000000ccu;
                                        renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                    }
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, isLoosing);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                /*if (tick < 3)
                                    coef *= 0.95;
                                else
                                    coef *= 0.9;*/
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);
                WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire<10, true>(sim, *this, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                double enDist = (enemyPos - unit.pos).len();
                                double dang = 1.0 / enDist;

                                if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }


}


namespace v22 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void NavMeshV2::compute(const MyLevel &level)
    {
        w = level.w;
        h = level.h;

        nodes.resize(level.w * level.h);

        auto canMove = [&level](int x, int y) {
            return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
        };

        for (int y = 1; y < level.h - 1; ++y)
        {
            for (int x = 1; x < level.w - 1; ++x)
            {
                //if (x == 31 && y == 27)
                //    LOG("AA");

                if (canMove(x, y))
                {
                    std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                    bool onLadder = level.getTile(x, y) == LADDER;
                    bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                    bool onGround;

                    bool cm_l = canMove(x - 1, y);
                    bool cm_r = canMove(x + 1, y);
                    bool cm_u = canMove(x, y + 1);
                    bool cm_d = canMove(x, y - 1);
                    bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                    bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                    bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                    bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                    bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                    bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                    bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                    bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                    bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                    if (!isJumppadBody || isWallOrLadderAbove)
                    {
                        // move left
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_l)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1;
                            nd0.links.push_back(l);
                        }

                        // left down
                        if (!isJumppadBody && cm_ld)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1 - level.w;
                            nd0.links.push_back(l);
                        }

                        // move right
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_r)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1;
                            nd0.links.push_back(l);
                        }

                        // right down
                        if (!isJumppadBody && cm_rd)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1 - level.w;
                            nd0.links.push_back(l);
                        }


                        // moveDown

                        if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -level.w;
                            nd0.links.push_back(l);
                        }

                    }

                    // move up
                    if (cm_u)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = level.w;
                                node.links.push_back(l);
                            }
                        }

                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 3;
                            l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                            l.targetCanCancel = l.targetMomentum == 0;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }

                        // move left up
                        if (cm_lu)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = -1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = -1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }

                        // move right up
                        if (cm_ru)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = 1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = +1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }
                    }

                    // stop jumping
                    for (int i = 1; i <= 5; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // start jumping
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if (onGround)
                    {
                        Node& node = cellNodes[0];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 5;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // hit ceiling

                    bool ceiling = isWallOrLadderAbove
                                   || wall(level.getTile(x - 1, y + 2))
                                   || wall(level.getTile(x + 1, y + 2));

                    if (ceiling && !isJumppadCurrent)
                    {
                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }

                    if (!onLadder && !onLadderUpper)
                    {
                        // jump on jumppad
                        bool isJumppad = false;
                        for (int dy = -1; dy <= 2; ++dy)
                        {
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                                {
                                    isJumppad = true;
                                    break;
                                }
                            }
                        }

                        if (isJumppad)
                        {
                            for (int i = 0; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 1;
                                l.targetMomentum = 10;
                                l.targetCanCancel = false;
                                l.deltaPos = 0;
                                node.links.push_back(l);
                            }
                        }
                    }
                }
            }
        }
    }

    void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
    {
        w = navMesh.w;
        h = navMesh.h;

        distMap.clear();
        distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
        prevNode.clear();
        prevNode.resize(w * h * MOM_N, -1);
#endif

        typedef std::pair<uint16_t , PP> NLink;
        auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

        std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

        pts.push(std::make_pair(0, pp));
        distMap[pp.ind()] = 0;

        while (!pts.empty())
        {
            PP p = pts.top().second;
            pts.pop();

            int pInd = p.ind();
            uint16_t curDist = distMap[pInd];
            const Node &node = navMesh.nodes[p.pos][p.innerInd()];

            //LOG("FROM " << p.toString(w) << " d " << curDist);

            for (const auto &link : node.links)
            {
                PP v;
                v.pos = p.pos + link.deltaPos;
                v.upMomentum = link.targetMomentum;
                v.canCancel = link.targetCanCancel;

                uint16_t newD = curDist + link.cost;
                int vInd = v.ind();
                if (distMap[vInd] > newD)
                {
                    distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                    prevNode[vInd] = pInd;
#endif
                    pts.push(std::make_pair(newD, v));

                    //LOG("TO " << v.toString(w) << " d " << newD);
                }
            }
        }
    }


    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
    {
        PP p = strat.navMesh.makePP(u);
        int posInd = p.ind();

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
        {
            strat.lruCache.erase(it->second.lruCacheIt);
            strat.lruCache.push_front(posInd);
            it->second.lruCacheIt = strat.lruCache.begin();
            return it->second;
        }

        if (strat.lruCache.size() > MAX_DMAPS)
        {
            int last = strat.lruCache.back();
            strat.lruCache.pop_back();
            strat.distMap.erase(last);

            //LOG("REMOVE CACHE");
        }

        DistanceMapV2 &res = strat.distMap[posInd];
        res.compute(strat.navMesh, p);

        strat.lruCache.push_front(posInd);
        res.lruCacheIt = strat.lruCache.begin();
        //LOG("CASHE SIZE " << strat.lruCache.size());

        return res;
    }

    DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.simpleDistMap.find(posInd);
        if (it != strat.simpleDistMap.end())
            return it->second;

        DistanceMap &res = strat.simpleDistMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    struct DistComputer {
        DistanceMapV2 &dmap;
        DistanceMap &sdmap;
        MyStrat &strat;

        DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
        }

        double getDist(const P &p, double simpleDistMapCoef) const {

            double d1 = dmap.getDist(strat.navMesh.makePP(p));
            double d2 = sdmap.getDist(p);
            double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

            /*static double sd1 = 0;
            static double sd2 = 0;

            if (d1 < 200 && d2 < 200)
            {
                static int c = 0;
                sd1 += d1;
                sd2 += d2;
                ++c;
                if (c % 100000 == 0)
                    LOG(sd1 / sd2);
            }
            else
            {
                LOG(d1 << " " << d2);
            }*/
            return dist;
        }

        double getDistM0(const MyUnit &u, double simpleDistMapCoef)
        {
            double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
            return dist;
        }
    };

    double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                double minDist = 1e9;
                Side side = Side::NONE;
                double deltaHealth = 0;
                for (const MyUnit &unit : sim.units)
                {
                    DistComputer dcomp(unit, strat);

                    double dist = dcomp.getDist(l.pos, sdScore);
                    if (unit.isMine())
                        dist *= 1.1;

                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                        deltaHealth = 100 - unit.health;
                    }
                }

                if (side == Side::ME)
                    lbScore += (deltaHealth * 0.1);
                else
                    lbScore -= (deltaHealth * 0.1);
            }
        }

        score += lbScore;
        //WRITE_LOG("++++++++++++ " << lbScore);

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                    score += unit.health * 0.5;
                else
                    score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                {
                    score -= 1;
                }

                IP ipos = IP(unit.pos);

                DistComputer dcomp(unit, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dcomp.getDistM0(en, sdScore);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                // WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
    {
        IP ipos = IP(unit.pos);

        DistComputer dcomp(unit, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side != unit.side)
            {
                double dist = dcomp.getDistM0(other, sdScore);

                if (unit.weapon)
                {
                    /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                    dist += (1.0 - dt) * 1.0;

                    dist += other.health * 0.1;*/
                }
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    template<int RAYS_N, bool log>
    void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                double spread = unit.weapon.spread;
                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    double enDist = (enemyPos - unit.pos).len();
                    double dang = 1.0 / enDist;

                    if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }
                    /*else
                    {
                        double delta = normalizeAngle(unit.weapon.lastAngle.getAngle() - unit.action.aim.getAngle());
                        unit.weapon.lastAngle = unit.action.aim.norm();
                        spread += std::abs(delta);

                        if (spread > unit.weapon.params->maxSpread)
                            spread = unit.weapon.params->maxSpread;
                    }*/

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;

                /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
                {
                    unit.action.reload = true;
                }*/
            }
        }
    }

    void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
    {
#ifdef ENABLE_LOGGING
        DistanceMapV2 dmap = getDistMap(from, s);

        PP p = s.navMesh.makePP(to);
        std::vector<int> nodes;
        nodes.push_back(p.ind());

        int ind = p.ind();
        while(true)
        {
            ind = dmap.prevNode[ind];
            if (ind == -1)
                break;

            nodes.push_back(ind);
        }

        renderPath(*s.sim.level, nodes);
#endif
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
        //LOG(SDMAP_COEF);

        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        bool isLoosing = sim.score[0] <= sim.score[1];

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/

        if (!navMesh.w)
        {
            navMesh.compute(*sim.level);
        }

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

                if (nearestEnemy)
                    renderPathTo(*this, unit, *nearestEnemy);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            int health = unit.health;
                            //std::vector<Simulator> saveSim;

                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                    {
                                        action.aim = su.weapon.lastAngle;

                                        if (tick >= 5 && su.isEnemy())
                                        {
                                            const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                            if (een)
                                            {
                                                action.aim = P(een->pos - su.pos).norm();
                                            }
                                        }
                                    }

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire<10, false>(sim, *this, sdScore, {});

                                /*{
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }*/


                                if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                //saveSim.push_back(sim);
                                sim.tick();

                                {
                                    for (MyUnit &u : sim.units)
                                    {
                                        if (u.isEnemy())
                                        {
                                            renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                            renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                        }
                                    }
                                }

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    if (simUnit->health != health)
                                    {
                                        uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                        renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                        WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                        health = simUnit->health;
                                    }
                                    else
                                    {
                                        uint32_t  c = 0x000000ccu;
                                        renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                    }
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, isLoosing, sdScore);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                /*if (tick < 3)
                                    coef *= 0.95;
                                else
                                    coef *= 0.9;*/
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);
                WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                double enDist = (enemyPos - unit.pos).len();
                                double dang = 1.0 / enDist;

                                if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }

}



namespace v25 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void NavMeshV2::compute(const MyLevel &level)
    {
        w = level.w;
        h = level.h;

        nodes.resize(level.w * level.h);

        auto canMove = [&level](int x, int y) {
            return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
        };

        for (int y = 1; y < level.h - 1; ++y)
        {
            for (int x = 1; x < level.w - 1; ++x)
            {
                //if (x == 31 && y == 27)
                //    LOG("AA");

                if (canMove(x, y))
                {
                    std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                    bool onLadder = level.getTile(x, y) == LADDER;
                    bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                    bool onGround;

                    bool cm_l = canMove(x - 1, y);
                    bool cm_r = canMove(x + 1, y);
                    bool cm_u = canMove(x, y + 1);
                    bool cm_d = canMove(x, y - 1);
                    bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                    bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                    bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                    bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                    bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                    bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                    bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                    bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                    bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                    if (!isJumppadBody || isWallOrLadderAbove)
                    {
                        // move left
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_l)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1;
                            nd0.links.push_back(l);
                        }

                        // left down
                        if (!isJumppadBody && cm_ld)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1 - level.w;
                            nd0.links.push_back(l);
                        }

                        // move right
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_r)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1;
                            nd0.links.push_back(l);
                        }

                        // right down
                        if (!isJumppadBody && cm_rd)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1 - level.w;
                            nd0.links.push_back(l);
                        }


                        // moveDown

                        if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -level.w;
                            nd0.links.push_back(l);
                        }

                    }

                    // move up
                    if (cm_u)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = level.w;
                                node.links.push_back(l);
                            }
                        }

                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 3;
                            l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                            l.targetCanCancel = l.targetMomentum == 0;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }

                        // move left up
                        if (cm_lu)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = -1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = -1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }

                        // move right up
                        if (cm_ru)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = 1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = +1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }
                    }

                    // stop jumping
                    for (int i = 1; i <= 5; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // start jumping
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if (onGround)
                    {
                        Node& node = cellNodes[0];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 5;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // hit ceiling

                    bool ceiling = isWallOrLadderAbove
                                   || wall(level.getTile(x - 1, y + 2))
                                   || wall(level.getTile(x + 1, y + 2));

                    if (ceiling && !isJumppadCurrent)
                    {
                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }

                    if (!onLadder && !onLadderUpper)
                    {
                        // jump on jumppad
                        bool isJumppad = false;
                        for (int dy = -1; dy <= 2; ++dy)
                        {
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                                {
                                    isJumppad = true;
                                    break;
                                }
                            }
                        }

                        if (isJumppad)
                        {
                            for (int i = 0; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 1;
                                l.targetMomentum = 10;
                                l.targetCanCancel = false;
                                l.deltaPos = 0;
                                node.links.push_back(l);
                            }
                        }
                    }
                }
            }
        }
    }

    void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
    {
        w = navMesh.w;
        h = navMesh.h;

        distMap.clear();
        distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
        prevNode.clear();
        prevNode.resize(w * h * MOM_N, -1);
#endif

        typedef std::pair<uint16_t , PP> NLink;
        auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

        std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

        pts.push(std::make_pair(0, pp));
        distMap[pp.ind()] = 0;

        while (!pts.empty())
        {
            PP p = pts.top().second;
            pts.pop();

            int pInd = p.ind();
            uint16_t curDist = distMap[pInd];
            const Node &node = navMesh.nodes[p.pos][p.innerInd()];

            //LOG("FROM " << p.toString(w) << " d " << curDist);

            for (const auto &link : node.links)
            {
                PP v;
                v.pos = p.pos + link.deltaPos;
                v.upMomentum = link.targetMomentum;
                v.canCancel = link.targetCanCancel;

                uint16_t newD = curDist + link.cost;
                int vInd = v.ind();
                if (distMap[vInd] > newD)
                {
                    distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                    prevNode[vInd] = pInd;
#endif
                    pts.push(std::make_pair(newD, v));

                    //LOG("TO " << v.toString(w) << " d " << newD);
                }
            }
        }
    }


    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
    {
        PP p = strat.navMesh.makePP(u);
        int posInd = p.ind();

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
        {
            strat.lruCache.erase(it->second.lruCacheIt);
            strat.lruCache.push_front(posInd);
            it->second.lruCacheIt = strat.lruCache.begin();
            return it->second;
        }

        if (strat.lruCache.size() > MAX_DMAPS)
        {
            int last = strat.lruCache.back();
            strat.lruCache.pop_back();
            strat.distMap.erase(last);

            //LOG("REMOVE CACHE");
        }

        DistanceMapV2 &res = strat.distMap[posInd];
        res.compute(strat.navMesh, p);

        strat.lruCache.push_front(posInd);
        res.lruCacheIt = strat.lruCache.begin();
        //LOG("CASHE SIZE " << strat.lruCache.size());

        return res;
    }

    DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.simpleDistMap.find(posInd);
        if (it != strat.simpleDistMap.end())
            return it->second;

        DistanceMap &res = strat.simpleDistMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    struct DistComputer {
        DistanceMapV2 &dmap;
        DistanceMap &sdmap;
        MyStrat &strat;

        DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
        }

        double getDist(const P &p, double simpleDistMapCoef) const {

            double d1 = dmap.getDist(strat.navMesh.makePP(p));
            double d2 = sdmap.getDist(p);
            double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

            /*static double sd1 = 0;
            static double sd2 = 0;

            if (d1 < 200 && d2 < 200)
            {
                static int c = 0;
                sd1 += d1;
                sd2 += d2;
                ++c;
                if (c % 100000 == 0)
                    LOG(sd1 / sd2);
            }
            else
            {
                LOG(d1 << " " << d2);
            }*/
            return dist;
        }

        double getDistM0(const MyUnit &u, double simpleDistMapCoef)
        {
            double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
            return dist;
        }
    };

    double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                double minDist = 1e9;
                Side side = Side::NONE;
                double deltaHealth = 0;
                for (const MyUnit &unit : sim.units)
                {
                    DistComputer dcomp(unit, strat);

                    double dist = dcomp.getDist(l.pos, sdScore);
                    if (unit.isMine())
                        dist *= 1.1;

                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                        deltaHealth = 100 - unit.health;
                    }
                }

                if (side == Side::ME)
                    lbScore += (deltaHealth * 0.1);
                else
                    lbScore -= (deltaHealth * 0.1);
            }
        }

        score += lbScore;
        //WRITE_LOG("++++++++++++ " << lbScore);

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                    score += unit.health * 0.5;
                else
                    score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                {
                    score -= 1;
                }

                IP ipos = IP(unit.pos);

                DistComputer dcomp(unit, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dcomp.getDistM0(en, sdScore);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                // WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                if (curHealth[unit.id] < 100)
                    score += hpScore * (100 - curHealth[unit.id]) / 200.0;

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
    {
        IP ipos = IP(unit.pos);

        DistComputer dcomp(unit, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side != unit.side)
            {
                double dist = dcomp.getDistM0(other, sdScore);

                if (unit.weapon)
                {
                    /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                    dist += (1.0 - dt) * 1.0;

                    dist += other.health * 0.1;*/
                }
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }

    template<int RAYS_N, bool log>
    void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                double spread = unit.weapon.spread;
                if (unit.weapon.lastAngle != P(0, 0))
                {
                    double angle = std::acos(dot(unit.weapon.lastAngle, aim));
                    double enDist = (enemyPos - unit.pos).len();
                    double dang = 1.0 / enDist;

                    if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                        //WRITE_LOG("OLD AIM")
                    }
                    /*else
                    {
                        double delta = normalizeAngle(unit.weapon.lastAngle.getAngle() - unit.action.aim.getAngle());
                        unit.weapon.lastAngle = unit.action.aim.norm();
                        spread += std::abs(delta);

                        if (spread > unit.weapon.params->maxSpread)
                            spread = unit.weapon.params->maxSpread;
                    }*/

                    //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                }
                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = true;

                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.dist(mu.pos);

                                if (mu.isMine() && dist < explRad * 1.2)
                                {
                                    myUnitsHits++;
                                }
                                else if (mu.isEnemy() && dist < explRad)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }

            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;

                /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
                {
                    unit.action.reload = true;
                }*/
            }
        }
    }

    void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
    {
#ifdef ENABLE_LOGGING
        DistanceMapV2 dmap = getDistMap(from, s);

        PP p = s.navMesh.makePP(to);
        std::vector<int> nodes;
        nodes.push_back(p.ind());

        int ind = p.ind();
        while(true)
        {
            ind = dmap.prevNode[ind];
            if (ind == -1)
                break;

            nodes.push_back(ind);
        }

        renderPath(*s.sim.level, nodes);
#endif
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
        //LOG(SDMAP_COEF);

        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        bool isLoosing = sim.score[0] <= sim.score[1];

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/

        if (!navMesh.w)
        {
            navMesh.compute(*sim.level);
        }

        std::map<int, int> curHealth;
        for (MyUnit &unit : sim.units)
        {
            curHealth[unit.id] = unit.health;
        }

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

                if (nearestEnemy)
                    renderPathTo(*this, unit, *nearestEnemy);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            int health = unit.health;
                            //std::vector<Simulator> saveSim;

                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                    {
                                        action.aim = su.weapon.lastAngle;

                                        if (tick >= 5 && su.isEnemy())
                                        {
                                            const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                            if (een)
                                            {
                                                action.aim = P(een->pos - su.pos).norm();
                                            }
                                        }
                                    }

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire<10, false>(sim, *this, sdScore, {});

                                /*{
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }*/


                                if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                //saveSim.push_back(sim);
                                sim.tick();

                                {
                                    for (MyUnit &u : sim.units)
                                    {
                                        if (u.isEnemy())
                                        {
                                            renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                            renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                        }
                                    }
                                }

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    if (simUnit->health != health)
                                    {
                                        uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                        renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                        WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                        health = simUnit->health;
                                    }
                                    else
                                    {
                                        uint32_t  c = 0x000000ccu;
                                        renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                    }
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                /*if (tick < 3)
                                    coef *= 0.95;
                                else
                                    coef *= 0.9;*/
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);
                WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

                // ============

                if (nearestEnemy->canJump)
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                            if (unit.weapon.lastAngle != P(0, 0))
                            {
                                double angle = std::acos(dot(unit.weapon.lastAngle, bestFire));
                                double enDist = (enemyPos - unit.pos).len();
                                double dang = 1.0 / enDist;

                                if (angle + dang < unit.weapon.spread || angle + unit.weapon.spread < 0.5*dang)
                                {
                                    unit.action.aim = unit.weapon.lastAngle;
                                    //WRITE_LOG("OLD AIM")
                                }

                                //WRITE_LOG("A " << angle << " " << unit.weapon.spread)
                            }
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }

}



namespace v26 {

void DistanceMap::compute(const Simulator &sim, IP ip) {
    w = sim.level->w;
    h = sim.level->h;
    distMap.clear();
    distMap.resize(w * h, -1);

    ip.limitWH(w, h);

    std::deque<IP> pts;
    pts.push_back(ip);

    distMap[ip.ind(w)] = 0;

    while (!pts.empty())
    {
        IP p = pts.front();
        pts.pop_front();

        double v = distMap[p.ind(w)] + 1;

        for (int y = p.y - 1; y <= p.y + 1; ++y)
        {
            if (sim.level->isValidY(y))
            {
                for (int x = p.x - 1; x <= p.x + 1; ++x)
                {
                    if (sim.level->isValidX(x))
                    {
                        int ind = IP(x, y).ind(w);

                        if (distMap[ind] < 0)
                        {
                            Tile t = sim.level->getTile(x, y);
                            if (t != WALL)
                            {
                                distMap[ind] = v;
                                pts.push_back(IP(x, y));
                            }
                        }
                    }
                }
            }
        }
    }
}


void NavMeshV2::compute(const MyLevel &level)
{
    w = level.w;
    h = level.h;

    nodes.resize(level.w * level.h);

    auto canMove = [&level](int x, int y) {
        return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
    };

    for (int y = 1; y < level.h - 1; ++y)
    {
        for (int x = 1; x < level.w - 1; ++x)
        {
            //if (x == 31 && y == 27)
            //    LOG("AA");

            if (canMove(x, y))
            {
                std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                bool onLadder = level.getTile(x, y) == LADDER;
                bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                bool onGround;

                bool cm_l = canMove(x - 1, y);
                bool cm_r = canMove(x + 1, y);
                bool cm_u = canMove(x, y + 1);
                bool cm_d = canMove(x, y - 1);
                bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                if (!isJumppadBody || isWallOrLadderAbove)
                {
                    // move left
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_l)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1;
                        nd0.links.push_back(l);
                    }

                    // left down
                    if (!isJumppadBody && cm_ld)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1 - level.w;
                        nd0.links.push_back(l);
                    }

                    // move right
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_r)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1;
                        nd0.links.push_back(l);
                    }

                    // right down
                    if (!isJumppadBody && cm_rd)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1 - level.w;
                        nd0.links.push_back(l);
                    }


                    // moveDown

                    if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -level.w;
                        nd0.links.push_back(l);
                    }

                }

                // move up
                if (cm_u)
                {
                    if (!isJumppadBody)
                    {
                        for (int i = 1; i <= 5; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 6;
                            l.targetMomentum = i - 1;
                            l.targetCanCancel = true;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }
                    }

                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 3;
                        l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                        l.targetCanCancel = l.targetMomentum == 0;
                        l.deltaPos = level.w;
                        node.links.push_back(l);
                    }

                    // move left up
                    if (cm_lu)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = -1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = -1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }

                    // move right up
                    if (cm_ru)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = 1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = +1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }
                }

                // stop jumping
                for (int i = 1; i <= 5; ++i)
                {
                    Node& node = cellNodes[i];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 0;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // start jumping
                onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                           || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                           || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                           || onLadder;

                if (onGround)
                {
                    Node& node = cellNodes[0];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 5;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // hit ceiling

                bool ceiling = isWallOrLadderAbove
                               || wall(level.getTile(x - 1, y + 2))
                               || wall(level.getTile(x + 1, y + 2));

                if (ceiling && !isJumppadCurrent)
                {
                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }
                }

                if (!onLadder && !onLadderUpper)
                {
                    // jump on jumppad
                    bool isJumppad = false;
                    for (int dy = -1; dy <= 2; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                            {
                                isJumppad = true;
                                break;
                            }
                        }
                    }

                    if (isJumppad)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 10;
                            l.targetCanCancel = false;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }
                }
            }
        }
    }
}

void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
{
    w = navMesh.w;
    h = navMesh.h;

    distMap.clear();
    distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
    prevNode.clear();
    prevNode.resize(w * h * MOM_N, -1);
#endif

    typedef std::pair<uint16_t , PP> NLink;
    auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

    std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

    pts.push(std::make_pair(0, pp));
    distMap[pp.ind()] = 0;

    while (!pts.empty())
    {
        PP p = pts.top().second;
        pts.pop();

        int pInd = p.ind();
        uint16_t curDist = distMap[pInd];
        const Node &node = navMesh.nodes[p.pos][p.innerInd()];

        //LOG("FROM " << p.toString(w) << " d " << curDist);

        for (const auto &link : node.links)
        {
            PP v;
            v.pos = p.pos + link.deltaPos;
            v.upMomentum = link.targetMomentum;
            v.canCancel = link.targetCanCancel;

            uint16_t newD = curDist + link.cost;
            int vInd = v.ind();
            if (distMap[vInd] > newD)
            {
                distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                prevNode[vInd] = pInd;
#endif
                pts.push(std::make_pair(newD, v));

                //LOG("TO " << v.toString(w) << " d " << newD);
            }
        }
    }
}


void applyMove(const Move &m, MyAction &action)
{
    action.velocity = m.vel;
    if (m.jump == 1)
        action.jump = true;
    else if (m.jump == -1)
        action.jumpDown = true;
}

DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
{
    PP p = strat.navMesh.makePP(u);
    int posInd = p.ind();

    auto it = strat.distMap.find(posInd);
    if (it != strat.distMap.end())
    {
        strat.lruCache.erase(it->second.lruCacheIt);
        strat.lruCache.push_front(posInd);
        it->second.lruCacheIt = strat.lruCache.begin();
        return it->second;
    }

    if (strat.lruCache.size() > MAX_DMAPS)
    {
        int last = strat.lruCache.back();
        strat.lruCache.pop_back();
        strat.distMap.erase(last);

        //LOG("REMOVE CACHE");
    }

    DistanceMapV2 &res = strat.distMap[posInd];
    res.compute(strat.navMesh, p);

    strat.lruCache.push_front(posInd);
    res.lruCacheIt = strat.lruCache.begin();
    //LOG("CASHE SIZE " << strat.lruCache.size());

    return res;
}

DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
{
    int posInd = p.ind(strat.sim.level->w);

    auto it = strat.simpleDistMap.find(posInd);
    if (it != strat.simpleDistMap.end())
        return it->second;

    DistanceMap &res = strat.simpleDistMap[posInd];
    res.compute(strat.sim, p);
    return res;
}

struct DistComputer {
    DistanceMapV2 &dmap;
    DistanceMap &sdmap;
    MyStrat &strat;

    DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
    }

    double getDist(const P &p, double simpleDistMapCoef) const {

        double d1 = dmap.getDist(strat.navMesh.makePP(p));
        double d2 = sdmap.getDist(p);
        double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

        /*static double sd1 = 0;
        static double sd2 = 0;

        if (d1 < 200 && d2 < 200)
        {
            static int c = 0;
            sd1 += d1;
            sd2 += d2;
            ++c;
            if (c % 100000 == 0)
                LOG(sd1 / sd2);
        }
        else
        {
            LOG(d1 << " " << d2);
        }*/
        return dist;
    }

    double getDistM0(const MyUnit &u, double simpleDistMapCoef)
    {
        double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
        return dist;
    }
};

double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth)
{
    double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

    double lbScore = 0;
    for (const MyLootBox &l : sim.lootBoxes)
    {
        if (l.type == MyLootBoxType ::HEALTH_PACK)
        {
            double minDist = 1e9;
            Side side = Side::NONE;
            double deltaHealth = 0;
            for (const MyUnit &unit : sim.units)
            {
                DistComputer dcomp(unit, strat);

                double dist = dcomp.getDist(l.pos, sdScore);
                if (unit.isMine())
                    dist *= 1.1;

                if (dist < minDist)
                {
                    minDist = dist;
                    side = unit.side;
                    deltaHealth = 100 - unit.health;
                }
            }

            if (side == Side::ME)
                lbScore += (deltaHealth * 0.1);
            else
                lbScore -= (deltaHealth * 0.1);
        }
    }

    score += lbScore;
    //WRITE_LOG("++++++++++++ " << lbScore);

    for (const MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                score += unit.health * 0.5;
            else
                score += unit.health;

            if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
            {
                score -= 1;
            }

            IP ipos = IP(unit.pos);

            DistComputer dcomp(unit, strat);

            double magazineRel = 1.0;
            if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
            {
                magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
            }

            bool reload = false;
            if (unit.weapon)
            {
                score += 100;

                reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
            }
            else
            {
                double wScore = 0;
                double wCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::WEAPON)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        wScore += 50.0 / (dist + 1.0);
                        wCount++;
                    }
                }

                if (wCount > 0)
                    score += wScore / wCount;
            }

            double hpScore = 0;

            {
                double hScore = 0;
                double hCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        hScore += 50.0 / (dist + 1.0);
                        hCount++;
                    }
                }

                if (hCount > 0)
                    hpScore = hScore / hCount;
            }

            double enScore = 0;
            if (unit.weapon)
            {
                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        bool enReload = false;
                        if (en.weapon)
                            enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                        double dist = dcomp.getDistM0(en, sdScore);

                        if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                        {
                            enScore -= (10 - dist)*2;
                            // WRITE_LOG("===")
                        }


                        if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                        {
                            double keepDist = 0;


                            if ((reload /*|| magazineRel < 0.5*/))
                                keepDist = 10;

                            if (en.weapon)
                            {
                                const double D = 5;
                                if (dist < D)
                                {
                                    bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                    bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                    if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                        enScore -= (D - dist)*5;

                                    /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                    WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                    WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                    if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                        enScore -= (D - dist)*2;
                                    else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                        enScore += (D - dist)*2;
                                }
                            }

                            //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            //    keepDist = 5;
                            //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                            //    keepDist = 3;

                            //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                            enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                        }
                        else
                        {
                            enScore -= 50.0 / (dist + 5.0);
                        }
                    }
                }

                score += enScore;
            }

            if (enScore < 0 && unit.health < 100)
            {
                score += hpScore;
            }

            if (curHealth[unit.id] < 100)
                score += hpScore * (100 - curHealth[unit.id]) / 200.0;

            //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
        }
        else
        {
            score -= unit.health * 0.5;
        }
    }

    return score;
}

const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
{
    IP ipos = IP(unit.pos);

    DistComputer dcomp(unit, strat);

    const MyUnit *nearestEnemy = nullptr;
    double dmapDist = 1e9;

    for (const MyUnit &other : sim.units)
    {
        if (other.side != unit.side)
        {
            double dist = dcomp.getDistM0(other, sdScore);

            if (unit.weapon)
            {
                /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                dist += (1.0 - dt) * 1.0;

                dist += other.health * 0.1;*/
            }
            if (dist < dmapDist)
            {
                dmapDist = dist;
                nearestEnemy = &other;
            }
        }
    }

    return nearestEnemy;
}



void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
        bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
        if (!b1 && !b2)
        {
            unit.action.aim = unit.weapon.lastAngle;
            return;
        }

        if (b1 && b2)
            return;

        if (b1)
        {
            double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
        else if (b2)
        {
            double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P((ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
    }
}

void normalizeAim(MyUnit &unit, const P &enemyPos)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        P center = unit.getCenter();
        BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
        P lowestAngle = (enBox.getCorner(0) - center).norm();
        P largestAngle = lowestAngle;
        for (int i = 1; i < 4; ++i)
        {
            P angle = (enBox.getCorner(i) - center).norm();
            if (isAngleLessThan(angle, lowestAngle))
                lowestAngle = angle;

            if (isAngleLessThan(largestAngle, angle))
                largestAngle = angle;
        }

        normalizeAim(unit, lowestAngle, largestAngle);
    }
}

template<int RAYS_N, bool log>
void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
{
    for (MyUnit &unit : sim.units)
    {
        if (unitId != -1 && unit.id != unitId)
            continue;

        if (unit.side == Side::ME && unit.weapon)
        {
            const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

            if (!nearestEnemy)
                return;

            P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
            P aim = (enemyPos - unit.pos).norm();
            unit.action.aim = aim;

            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

            normalizeAim(unit, enemyPos);

            //unit.action.aim = (enemyPos - bestMyFirePos).norm();
            //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
            unit.action.shoot = true;

            double spread = unit.weapon.spread;
            if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
            {
                //const int RAYS_N = 10;

                P ddir = P(spread * 2.0 / (RAYS_N - 1));
                P ray = unit.action.aim.rotate(-spread);

                P center = unit.getCenter();

                MyWeaponParams *wparams = unit.weapon.params;
                BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                int myUnitsHits = 0;
                double enUnitsHits = 0;

                for (int i = 0; i < RAYS_N; ++i)
                {
                    P tp = sim.level->bboxWallCollision2(missile, ray);

                    MyUnit *directHitUnit = nullptr;
                    double directHitDist = 1e9;
                    for (MyUnit &mu : sim.units) // direct hit
                    {
                        if (mu.id != unit.id)
                        {
                            auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                            if (isCollided)
                            {
                                double d1 = tp.dist2(center);
                                double d2 = colP.dist2(center);

                                if (d2 < d1 && d2 < directHitDist)
                                {
                                    directHitDist = d2;
                                    directHitUnit = &mu;
                                }
                            }
                        }
                    }

                    if (directHitUnit)
                    {
                        if (directHitUnit->isMine())
                            myUnitsHits++;
                        else
                        {
                            BBox b2 = directHitUnit->getBBox();
                            double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                            double expD = dst / 10.0;
                            if (expD > 0.4)
                                expD = 0.4;
                            b2.expand(-expD);

                            auto [isCollided, colP] = b2.rayIntersection(center, ray);
                            if (isCollided)
                            {
                                if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                {
                                    enUnitsHits += 0.09;
                                }
                                else
                                {
                                    enUnitsHits++;
                                }
                                //LOG("EXP " << expD);
                            }
                            else
                                enUnitsHits += 0.09;
                        }
                    }

                    // Explosion damage
                    if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                    {
                        double explRad = wparams->explRadius;

                        for (MyUnit &mu : sim.units)
                        {
                            double dist = tp.dist(mu.pos);

                            if (mu.isMine() && dist < explRad * 1.2)
                            {
                                myUnitsHits++;
                            }
                            else if (mu.isEnemy() && dist < explRad)
                            {
                                enUnitsHits++;
                            }
                        }
                    }

                    ray = ray.rotate(ddir);
                }

                if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                    unit.action.shoot = false;

                if (log)
                    WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
            }

        }

        if (unit.isMine())
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                unit.action.swapWeapon = true;

            /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
            {
                unit.action.reload = true;
            }*/
        }
    }
}

void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
{
#ifdef ENABLE_LOGGING
    DistanceMapV2 dmap = getDistMap(from, s);

    PP p = s.navMesh.makePP(to);
    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = dmap.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    renderPath(*s.sim.level, nodes);
#endif
}

void MyStrat::compute(const Simulator &_sim)
{
    double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
    //LOG(SDMAP_COEF);

    std::map<int, Move> enemyMoves;

    if (_sim.curTick > 0)
    {
        for (const MyUnit &u : _sim.units)
        {
            if (u.isEnemy())
            {
                const MyUnit *oldUnit = this->sim.getUnit(u.id);
                if (oldUnit)
                {
                    Move &m = enemyMoves[u.id];
                    m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                    if (u.pos.y > oldUnit->pos.y)
                    {
                        m.jump = 1;
                    }
                    else if (u.pos.y > oldUnit->pos.y && u.canJump)
                    {
                        m.jump = -1;
                    }
                }
            }
        }
    }

    this->sim = _sim;

    bool isLoosing = sim.score[0] <= sim.score[1];

    /*for (MyUnit &u : sim.units)
    {
        if (u.side == Side::ME)
        {
            u.action = MyAction();
        }
    }

    return;*/

    if (!navMesh.w)
    {
        navMesh.compute(*sim.level);
    }

    std::map<int, int> curHealth;
    for (MyUnit &unit : sim.units)
    {
        curHealth[unit.id] = unit.health;
    }

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

            double bestScore = -100000;
            Move bestMove;
            P bestFire = P(0, 0);
            double friendlyFire = 0;
            P bestMyFirePos = unit.pos;

            const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

            if (nearestEnemy)
                renderPathTo(*this, unit, *nearestEnemy);

            P enemyPos = P(0, 0);
            const int TICKN = 25;
            int bulletFlyTime = 0;
            double enemyDist = 1000.0;
            int ticksToFire = 0;
            if (nearestEnemy && unit.weapon)
            {
                enemyPos = nearestEnemy->pos;
                enemyDist = enemyPos.dist(unit.pos);
                bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                ticksToFire = unit.weapon.fireTimerMicrotics / 100;
            }

            if (bulletFlyTime >= TICKN)
                bulletFlyTime = TICKN - 1;


            {

                int jumpN = 3;
                if (unit.canJump)
                    jumpN = 4;

                for (int dir = 0; dir < 3; ++dir)
                {
                    for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                    {
                        Simulator sim = _sim;
                        sim.enFireAtCenter = true;
                        sim.score[0] = 0;
                        sim.score[1] = 0;
                        sim.selfFire[0] = 0;
                        sim.selfFire[1] = 0;
                        sim.friendlyFireByUnitId.clear();

                        P myFirePos = unit.pos;
                        P fireAim = P(0, 0);

                        P p1 = unit.pos;

                        std::string moveName;
                        if (dir == 0)
                        {
                            moveName = "left ";
                        }
                        else if (dir == 1)
                        {
                            moveName = "right ";
                        }

                        if (jumpS == 0)
                        {
                            moveName += "up";
                        }
                        else if (jumpS == 1)
                        {
                            moveName += "down";
                        }
                        else if (jumpS == 3)
                        {
                            moveName += "downup";
                        }

                        Move firstMove;
                        IP ipos = IP(unit.pos);

                        double score = 0;
                        double coef = 1.0;
                        bool fired = false;
                        int health = unit.health;
                        //std::vector<Simulator> saveSim;

                        for (int tick = 0; tick < TICKN; ++tick)
                        {
                            MyUnit *simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                                break;

                            double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                            simUnit->action = MyAction();
                            for (MyUnit &su : sim.units)
                            {
                                su.action = MyAction();

                                MyAction &action = su.action;
                                action.shoot = true;
                                if (su.weapon)
                                {
                                    action.aim = su.weapon.lastAngle;

                                    if (tick >= 5 && su.isEnemy())
                                    {
                                        const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                        if (een)
                                        {
                                            action.aim = P(een->pos - su.pos).norm();
                                        }
                                    }
                                }

                                if (su.side == Side::ME && su.id != unit.id)
                                {
                                    applyMove(myMoves[su.id], action);
                                }
                                else if (tick <= 5 && su.isEnemy())
                                {
                                    Move &em = enemyMoves[su.id];
                                    applyMove(em, su.action);
                                }
                            }

                            if (dir == 0)
                            {
                                simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                            }
                            else if (dir == 1)
                            {
                                simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                            }



                            if (jumpS == 0)
                            {
                                simUnit->action.jump = true;
                            }
                            else if (jumpS == 1)
                            {
                                simUnit->action.jumpDown = true;
                            }

                            if (jumpS == 3 && tick == 0)
                            {
                                simUnit->action.jump = false;
                            }

                            if (jumpS == 2 || (jumpS == 3 && tick > 0))
                            {
                                if (dir == 0)
                                {
                                    IP ip = IP(simUnit->pos - P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                                else if (dir == 1)
                                {
                                    IP ip = IP(simUnit->pos + P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                            }

                            if (tick == 0)
                            {
                                firstMove.vel = simUnit->action.velocity;
                                if (simUnit->action.jump)
                                    firstMove.jump = 1;
                                else if (simUnit->action.jumpDown)
                                    firstMove.jump = -1;
                            }

                            computeFire<10, false>(sim, *this, sdScore, {});

                            /*{
                                for (MyBullet &b : sim.bullets)
                                {
                                    BBox bb = b.getBBox();

                                    uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                    renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                    renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                }
                            }*/


                            if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                            {
                                fireAim = simUnit->action.aim;
                                fired = true;
                            }

                            //saveSim.push_back(sim);
                            sim.tick();

                            {
                                for (MyUnit &u : sim.units)
                                {
                                    if (u.isEnemy())
                                    {
                                        renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                        renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                    }
                                }
                            }

                            simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                            {
                                score -= 1000;
                                break;
                            }

                            {
                                if (simUnit->health != health)
                                {
                                    uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                    renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                    WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                    health = simUnit->health;
                                }
                                else
                                {
                                    uint32_t  c = 0x000000ccu;
                                    renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                }
                            }

                            {
                                P p2 = simUnit->pos;
                                renderLine(p1, p2, 0x000000ffu);
                                p1 = p2;
                            }

                            if (tick == bulletFlyTime && nearestEnemy)
                            {
                                MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                if (newEnemy)
                                    enemyPos = newEnemy->pos;
                            }

                            if (tick == ticksToFire && nearestEnemy)
                            {
                                myFirePos = simUnit->pos;
                            }

                            /*IP newIpos = IP(unit.pos);

                            if (newIpos != ipos)
                            {
                                if (navMesh.get(newIpos).canWalk)
                                {
                                    ipos = newIpos;
                                }
                            }*/

                            double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth);
                            if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                            {
                                WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                            }

                            score += lscore * coef;

                            /*if (tick < 3)
                                coef *= 0.95;
                            else
                                coef *= 0.9;*/
                            coef *= 0.95;
                        }

                        WRITE_LOG(moveName << " " << score);

                        if (score > bestScore)
                        {
                            bestMove = firstMove;
                            bestFire = fireAim;
                            bestScore = score;
                            friendlyFire = sim.friendlyFireByUnitId[unit.id];
                            bestMyFirePos = myFirePos;

                            WRITE_LOG("SC " << moveName << " " << score);
                        }


                    }
                }
            }

            // ============

            myMoves[unit.id] = bestMove;
            unit.action = MyAction();
            applyMove(bestMove, unit.action);
            WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

            // ============

            if (nearestEnemy->canJump)
                enemyPos = nearestEnemy->pos;

            {
                renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                }
                else
                {
                    if (unit.weapon.fireTimerMicrotics > 400)
                    {
                        unit.action.aim = (enemyPos - unit.pos).norm();
                    }
                    else if (bestFire != P(0, 0))
                    {
                        unit.action.aim = bestFire;

                        //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                        normalizeAim(unit, enemyPos);
                    }
                    else
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                    }
                }

                WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
            }

        }
    }
}
}


namespace v27 {

void DistanceMap::compute(const Simulator &sim, IP ip) {
    w = sim.level->w;
    h = sim.level->h;
    distMap.clear();
    distMap.resize(w * h, -1);

    ip.limitWH(w, h);

    std::deque<IP> pts;
    pts.push_back(ip);

    distMap[ip.ind(w)] = 0;

    while (!pts.empty())
    {
        IP p = pts.front();
        pts.pop_front();

        double v = distMap[p.ind(w)] + 1;

        for (int y = p.y - 1; y <= p.y + 1; ++y)
        {
            if (sim.level->isValidY(y))
            {
                for (int x = p.x - 1; x <= p.x + 1; ++x)
                {
                    if (sim.level->isValidX(x))
                    {
                        int ind = IP(x, y).ind(w);

                        if (distMap[ind] < 0)
                        {
                            Tile t = sim.level->getTile(x, y);
                            if (t != WALL)
                            {
                                distMap[ind] = v;
                                pts.push_back(IP(x, y));
                            }
                        }
                    }
                }
            }
        }
    }
}


void NavMeshV2::compute(const MyLevel &level)
{
    w = level.w;
    h = level.h;

    nodes.resize(level.w * level.h);

    auto canMove = [&level](int x, int y) {
        return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
    };

    for (int y = 1; y < level.h - 1; ++y)
    {
        for (int x = 1; x < level.w - 1; ++x)
        {
            //if (x == 31 && y == 27)
            //    LOG("AA");

            if (canMove(x, y))
            {
                std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                bool onLadder = level.getTile(x, y) == LADDER;
                bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                bool onGround;

                bool cm_l = canMove(x - 1, y);
                bool cm_r = canMove(x + 1, y);
                bool cm_u = canMove(x, y + 1);
                bool cm_d = canMove(x, y - 1);
                bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                if (!isJumppadBody || isWallOrLadderAbove)
                {
                    // move left
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_l)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1;
                        nd0.links.push_back(l);
                    }

                    // left down
                    if (!isJumppadBody && cm_ld)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1 - level.w;
                        nd0.links.push_back(l);
                    }

                    // move right
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_r)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1;
                        nd0.links.push_back(l);
                    }

                    // right down
                    if (!isJumppadBody && cm_rd)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1 - level.w;
                        nd0.links.push_back(l);
                    }


                    // moveDown

                    if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -level.w;
                        nd0.links.push_back(l);
                    }

                }

                // move up
                if (cm_u)
                {
                    if (!isJumppadBody)
                    {
                        for (int i = 1; i <= 5; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 6;
                            l.targetMomentum = i - 1;
                            l.targetCanCancel = true;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }
                    }

                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 3;
                        l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                        l.targetCanCancel = l.targetMomentum == 0;
                        l.deltaPos = level.w;
                        node.links.push_back(l);
                    }

                    // move left up
                    if (cm_lu)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = -1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = -1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }

                    // move right up
                    if (cm_ru)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = 1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = +1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }
                }

                // stop jumping
                for (int i = 1; i <= 5; ++i)
                {
                    Node& node = cellNodes[i];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 0;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // start jumping
                onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                           || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                           || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                           || onLadder;

                if (onGround)
                {
                    Node& node = cellNodes[0];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 5;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // hit ceiling

                bool ceiling = isWallOrLadderAbove
                               || wall(level.getTile(x - 1, y + 2))
                               || wall(level.getTile(x + 1, y + 2));

                if (ceiling && !isJumppadCurrent)
                {
                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }
                }

                if (!onLadder && !onLadderUpper)
                {
                    // jump on jumppad
                    bool isJumppad = false;
                    for (int dy = -1; dy <= 2; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                            {
                                isJumppad = true;
                                break;
                            }
                        }
                    }

                    if (isJumppad)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 10;
                            l.targetCanCancel = false;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }
                }
            }
        }
    }
}

void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
{
    w = navMesh.w;
    h = navMesh.h;

    distMap.clear();
    distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
    prevNode.clear();
    prevNode.resize(w * h * MOM_N, -1);
#endif

    typedef std::pair<uint16_t , PP> NLink;
    auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

    std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

    pts.push(std::make_pair(0, pp));
    distMap[pp.ind()] = 0;

    while (!pts.empty())
    {
        PP p = pts.top().second;
        pts.pop();

        int pInd = p.ind();
        uint16_t curDist = distMap[pInd];
        const Node &node = navMesh.nodes[p.pos][p.innerInd()];

        //LOG("FROM " << p.toString(w) << " d " << curDist);

        for (const auto &link : node.links)
        {
            PP v;
            v.pos = p.pos + link.deltaPos;
            v.upMomentum = link.targetMomentum;
            v.canCancel = link.targetCanCancel;

            uint16_t newD = curDist + link.cost;
            int vInd = v.ind();
            if (distMap[vInd] > newD)
            {
                distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                prevNode[vInd] = pInd;
#endif
                pts.push(std::make_pair(newD, v));

                //LOG("TO " << v.toString(w) << " d " << newD);
            }
        }
    }
}


void applyMove(const Move &m, MyAction &action)
{
    action.velocity = m.vel;
    if (m.jump == 1)
        action.jump = true;
    else if (m.jump == -1)
        action.jumpDown = true;
}

DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
{
    PP p = strat.navMesh.makePP(u);
    int posInd = p.ind();

    auto it = strat.distMap.find(posInd);
    if (it != strat.distMap.end())
    {
        strat.lruCache.erase(it->second.lruCacheIt);
        strat.lruCache.push_front(posInd);
        it->second.lruCacheIt = strat.lruCache.begin();
        return it->second;
    }

    if (strat.lruCache.size() > MAX_DMAPS)
    {
        int last = strat.lruCache.back();
        strat.lruCache.pop_back();
        strat.distMap.erase(last);

        //LOG("REMOVE CACHE");
    }

    DistanceMapV2 &res = strat.distMap[posInd];
    res.compute(strat.navMesh, p);

    strat.lruCache.push_front(posInd);
    res.lruCacheIt = strat.lruCache.begin();
    //LOG("CASHE SIZE " << strat.lruCache.size());

    return res;
}

DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
{
    int posInd = p.ind(strat.sim.level->w);

    auto it = strat.simpleDistMap.find(posInd);
    if (it != strat.simpleDistMap.end())
        return it->second;

    DistanceMap &res = strat.simpleDistMap[posInd];
    res.compute(strat.sim, p);
    return res;
}

struct DistComputer {
    DistanceMapV2 &dmap;
    DistanceMap &sdmap;
    MyStrat &strat;

    DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
    }

    double getDist(const P &p, double simpleDistMapCoef) const {

        double d1 = dmap.getDist(strat.navMesh.makePP(p));
        double d2 = sdmap.getDist(p);
        double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

        /*static double sd1 = 0;
        static double sd2 = 0;

        if (d1 < 200 && d2 < 200)
        {
            static int c = 0;
            sd1 += d1;
            sd2 += d2;
            ++c;
            if (c % 100000 == 0)
                LOG(sd1 / sd2);
        }
        else
        {
            LOG(d1 << " " << d2);
        }*/
        return dist;
    }

    double getDistM0(const MyUnit &u, double simpleDistMapCoef)
    {
        double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
        return dist;
    }
};

double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth)
{
    double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

    double lbScore = 0;
    for (const MyLootBox &l : sim.lootBoxes)
    {
        if (l.type == MyLootBoxType ::HEALTH_PACK)
        {
            double minDist = 1e9;
            Side side = Side::NONE;
            double deltaHealth = 0;
            for (const MyUnit &unit : sim.units)
            {
                DistComputer dcomp(unit, strat);

                double dist = dcomp.getDist(l.pos, sdScore);
                if (unit.isMine())
                    dist *= 1.1;

                if (dist < minDist)
                {
                    minDist = dist;
                    side = unit.side;
                    deltaHealth = 100 - unit.health;
                }
            }

            if (side == Side::ME)
                lbScore += (deltaHealth * 0.1);
            else
                lbScore -= (deltaHealth * 0.1);
        }
    }

    if (lbScore > 30)
        lbScore = 30;

    score += lbScore;
    //WRITE_LOG("++++++++++++ " << lbScore);

    for (const MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                score += unit.health * 0.5;
            else
                score += unit.health;

            if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
            {
                score -= 1;
            }

            IP ipos = IP(unit.pos);

            DistComputer dcomp(unit, strat);

            double magazineRel = 1.0;
            if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
            {
                magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
            }

            bool reload = false;
            if (unit.weapon)
            {
                score += 100;

                reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
            }
            else
            {
                double wScore = 0;
                double wCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::WEAPON)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        wScore += 50.0 / (dist + 1.0);
                        wCount++;
                    }
                }

                if (wCount > 0)
                    score += wScore / wCount;
            }

            double hpScore = 0;

            {
                double hScore = 0;
                double hCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        hScore += 50.0 / (dist + 1.0);
                        hCount++;
                    }
                }

                if (hCount > 0)
                    hpScore = hScore / hCount;
            }

            double enScore = 0;
            if (unit.weapon)
            {
                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        bool enReload = false;
                        if (en.weapon)
                            enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                        double dist = dcomp.getDistM0(en, sdScore);

                        if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                        {
                            enScore -= (10 - dist)*2;
                            // WRITE_LOG("===")
                        }


                        if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                        {
                            double keepDist = 0;


                            if ((reload /*|| magazineRel < 0.5*/))
                                keepDist = 10;

                            if (en.weapon)
                            {
                                const double D = 5;
                                if (dist < D)
                                {
                                    bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                    bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                    if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                        enScore -= (D - dist)*5;

                                    /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                    WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                    WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                    if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                        enScore -= (D - dist)*2;
                                    else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                        enScore += (D - dist)*2;
                                }
                            }

                            //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            //    keepDist = 5;
                            //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                            //    keepDist = 3;

                            //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)));
                            enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                        }
                        else
                        {
                            enScore -= 50.0 / (dist + 5.0);
                        }
                    }
                }

                score += enScore;
            }

            if (enScore < 0 && unit.health < 100)
            {
                score += hpScore;
            }

            if (curHealth[unit.id] < 100)
                score += hpScore * (100 - curHealth[unit.id]) / 200.0;

            //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health);
        }
        else
        {
            score -= unit.health * 0.5;
        }
    }

    return score;
}

const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
{
    IP ipos = IP(unit.pos);

    DistComputer dcomp(unit, strat);

    const MyUnit *nearestEnemy = nullptr;
    double dmapDist = 1e9;

    for (const MyUnit &other : sim.units)
    {
        if (other.side != unit.side)
        {
            double dist = dcomp.getDistM0(other, sdScore);

            if (unit.weapon)
            {
                /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                dist += (1.0 - dt) * 1.0;

                dist += other.health * 0.1;*/
            }
            if (dist < dmapDist)
            {
                dmapDist = dist;
                nearestEnemy = &other;
            }
        }
    }

    return nearestEnemy;
}



void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
        bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
        if (!b1 && !b2)
        {
            unit.action.aim = unit.weapon.lastAngle;
            return;
        }

        if (b1 && b2)
            return;

        if (b1)
        {
            double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
        else if (b2)
        {
            double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P((ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
    }
}

void normalizeAim(MyUnit &unit, const P &enemyPos)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        P center = unit.getCenter();
        BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
        P lowestAngle = (enBox.getCorner(0) - center).norm();
        P largestAngle = lowestAngle;
        for (int i = 1; i < 4; ++i)
        {
            P angle = (enBox.getCorner(i) - center).norm();
            if (isAngleLessThan(angle, lowestAngle))
                lowestAngle = angle;

            if (isAngleLessThan(largestAngle, angle))
                largestAngle = angle;
        }

        normalizeAim(unit, lowestAngle, largestAngle);
    }
}

template<int RAYS_N, bool log>
void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
{
    for (MyUnit &unit : sim.units)
    {
        if (unitId != -1 && unit.id != unitId)
            continue;

        if (unit.side == Side::ME && unit.weapon)
        {
            const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

            if (!nearestEnemy)
                return;

            P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
            P aim = (enemyPos - unit.pos).norm();
            unit.action.aim = aim;

            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

            normalizeAim(unit, enemyPos);

            //unit.action.aim = (enemyPos - bestMyFirePos).norm();
            //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
            unit.action.shoot = true;

            double spread = unit.weapon.spread;
            if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
            {
                //const int RAYS_N = 10;

                P ddir = P(spread * 2.0 / (RAYS_N - 1));
                P ray = unit.action.aim.rotate(-spread);

                P center = unit.getCenter();

                MyWeaponParams *wparams = unit.weapon.params;
                BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                int myUnitsHits = 0;
                double enUnitsHits = 0;

                for (int i = 0; i < RAYS_N; ++i)
                {
                    P tp = sim.level->bboxWallCollision2(missile, ray);

                    MyUnit *directHitUnit = nullptr;
                    double directHitDist = 1e9;
                    for (MyUnit &mu : sim.units) // direct hit
                    {
                        if (mu.id != unit.id)
                        {
                            auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                            if (isCollided)
                            {
                                double d1 = tp.dist2(center);
                                double d2 = colP.dist2(center);

                                if (d2 < d1 && d2 < directHitDist)
                                {
                                    directHitDist = d2;
                                    directHitUnit = &mu;
                                }
                            }
                        }
                    }

                    if (directHitUnit)
                    {
                        if (directHitUnit->isMine())
                            myUnitsHits++;
                        else
                        {
                            BBox b2 = directHitUnit->getBBox();
                            double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                            double expD = dst / 10.0;
                            if (expD > 0.4)
                                expD = 0.4;
                            b2.expand(-expD);

                            auto [isCollided, colP] = b2.rayIntersection(center, ray);
                            if (isCollided)
                            {
                                if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                {
                                    enUnitsHits += 0.09;
                                }
                                else
                                {
                                    enUnitsHits++;
                                }
                                //LOG("EXP " << expD);
                            }
                            else
                                enUnitsHits += 0.09;
                        }
                    }

                    // Explosion damage
                    if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                    {
                        double explRad = wparams->explRadius;

                        for (MyUnit &mu : sim.units)
                        {
                            double dist = tp.dist(mu.pos);

                            if (mu.isMine() && dist < explRad * 1.2)
                            {
                                myUnitsHits++;
                            }
                            else if (mu.isEnemy() && dist < explRad * 0.7)
                            {
                                enUnitsHits++;
                            }
                        }
                    }

                    ray = ray.rotate(ddir);
                }

                if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                    unit.action.shoot = false;

                if (log)
                    WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
            }

        }

        if (unit.isMine())
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                unit.action.swapWeapon = true;

            /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
            {
                unit.action.reload = true;
            }*/
        }
    }
}

void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
{
#ifdef ENABLE_LOGGING
    DistanceMapV2 dmap = getDistMap(from, s);

    PP p = s.navMesh.makePP(to);
    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = dmap.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    renderPath(*s.sim.level, nodes);
#endif
}

void MyStrat::compute(const Simulator &_sim)
{
    double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
    //LOG(SDMAP_COEF);

    std::map<int, Move> enemyMoves;

    if (_sim.curTick > 0)
    {
        for (const MyUnit &u : _sim.units)
        {
            if (u.isEnemy())
            {
                const MyUnit *oldUnit = this->sim.getUnit(u.id);
                if (oldUnit)
                {
                    Move &m = enemyMoves[u.id];
                    m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                    if (u.pos.y > oldUnit->pos.y)
                    {
                        m.jump = 1;
                    }
                    else if (u.pos.y > oldUnit->pos.y && u.canJump)
                    {
                        m.jump = -1;
                    }
                }
            }
        }
    }

    this->sim = _sim;

    bool isLoosing = sim.score[0] <= sim.score[1];

    /*for (MyUnit &u : sim.units)
    {
        if (u.side == Side::ME)
        {
            u.action = MyAction();
        }
    }

    return;*/

    if (!navMesh.w)
    {
        navMesh.compute(*sim.level);
    }

    std::map<int, int> curHealth;
    for (MyUnit &unit : sim.units)
    {
        curHealth[unit.id] = unit.health;
    }

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

            double bestScore = -100000;
            Move bestMove;
            P bestFire = P(0, 0);
            double friendlyFire = 0;
            P bestMyFirePos = unit.pos;

            const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

            if (nearestEnemy)
                renderPathTo(*this, unit, *nearestEnemy);

            P enemyPos = P(0, 0);
            const int TICKN = 25;
            int bulletFlyTime = 0;
            double enemyDist = 1000.0;
            int ticksToFire = 0;
            if (nearestEnemy && unit.weapon)
            {
                enemyPos = nearestEnemy->pos;
                enemyDist = enemyPos.dist(unit.pos);
                bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                ticksToFire = unit.weapon.fireTimerMicrotics / 100;
            }

            if (bulletFlyTime >= TICKN)
                bulletFlyTime = TICKN - 1;


            {

                int jumpN = 3;
                if (unit.canJump)
                    jumpN = 4;

                for (int dir = 0; dir < 3; ++dir)
                {
                    for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                    {
                        Simulator sim = _sim;
                        sim.enFireAtCenter = true;
                        sim.score[0] = 0;
                        sim.score[1] = 0;
                        sim.selfFire[0] = 0;
                        sim.selfFire[1] = 0;
                        sim.friendlyFireByUnitId.clear();

                        P myFirePos = unit.pos;
                        P fireAim = P(0, 0);

                        P p1 = unit.pos;

                        std::string moveName;
                        if (dir == 0)
                        {
                            moveName = "left ";
                        }
                        else if (dir == 1)
                        {
                            moveName = "right ";
                        }

                        if (jumpS == 0)
                        {
                            moveName += "up";
                        }
                        else if (jumpS == 1)
                        {
                            moveName += "down";
                        }
                        else if (jumpS == 3)
                        {
                            moveName += "downup";
                        }

                        Move firstMove;
                        IP ipos = IP(unit.pos);

                        double score = 0;
                        double coef = 1.0;
                        bool fired = false;
                        int health = unit.health;
                        //std::vector<Simulator> saveSim;

                        for (int tick = 0; tick < TICKN; ++tick)
                        {
                            MyUnit *simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                                break;

                            double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                            simUnit->action = MyAction();
                            for (MyUnit &su : sim.units)
                            {
                                su.action = MyAction();

                                MyAction &action = su.action;
                                action.shoot = true;
                                if (su.weapon)
                                {
                                    action.aim = su.weapon.lastAngle;

                                    if (tick >= 5 && su.isEnemy())
                                    {
                                        const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                        if (een)
                                        {
                                            action.aim = P(een->pos - su.pos).norm();
                                        }
                                    }
                                }

                                if (su.side == Side::ME && su.id != unit.id)
                                {
                                    applyMove(myMoves[su.id], action);
                                }
                                else if (tick <= 5 && su.isEnemy())
                                {
                                    Move &em = enemyMoves[su.id];
                                    applyMove(em, su.action);
                                }
                            }

                            if (dir == 0)
                            {
                                simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                            }
                            else if (dir == 1)
                            {
                                simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                            }



                            if (jumpS == 0)
                            {
                                simUnit->action.jump = true;
                            }
                            else if (jumpS == 1)
                            {
                                simUnit->action.jumpDown = true;
                            }

                            if (jumpS == 3 && tick == 0)
                            {
                                simUnit->action.jump = false;
                            }

                            if (jumpS == 2 || (jumpS == 3 && tick > 0))
                            {
                                if (dir == 0)
                                {
                                    IP ip = IP(simUnit->pos - P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                                else if (dir == 1)
                                {
                                    IP ip = IP(simUnit->pos + P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                            }

                            if (tick == 0)
                            {
                                firstMove.vel = simUnit->action.velocity;
                                if (simUnit->action.jump)
                                    firstMove.jump = 1;
                                else if (simUnit->action.jumpDown)
                                    firstMove.jump = -1;
                            }

                            computeFire<10, false>(sim, *this, sdScore, {});

                            /*{
                                for (MyBullet &b : sim.bullets)
                                {
                                    BBox bb = b.getBBox();

                                    uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                    renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                    renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                }
                            }*/


                            if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                            {
                                fireAim = simUnit->action.aim;
                                fired = true;
                            }

                            //saveSim.push_back(sim);
                            sim.tick();

                            {
                                for (MyUnit &u : sim.units)
                                {
                                    if (u.isEnemy())
                                    {
                                        renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                        renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                    }
                                }
                            }

                            simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                            {
                                score -= 1000;
                                break;
                            }

                            {
                                if (simUnit->health != health)
                                {
                                    uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                    renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                    WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                    health = simUnit->health;
                                }
                                else
                                {
                                    uint32_t  c = 0x000000ccu;
                                    renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                }
                            }

                            {
                                P p2 = simUnit->pos;
                                renderLine(p1, p2, 0x000000ffu);
                                p1 = p2;
                            }

                            if (tick == bulletFlyTime && nearestEnemy)
                            {
                                MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                if (newEnemy)
                                    enemyPos = newEnemy->pos;
                            }

                            if (tick == ticksToFire && nearestEnemy)
                            {
                                myFirePos = simUnit->pos;
                            }

                            /*IP newIpos = IP(unit.pos);

                            if (newIpos != ipos)
                            {
                                if (navMesh.get(newIpos).canWalk)
                                {
                                    ipos = newIpos;
                                }
                            }*/

                            double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth);
                            if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                            {
                                WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                            }

                            score += lscore * coef;

                            /*if (tick < 3)
                                coef *= 0.95;
                            else
                                coef *= 0.9;*/
                            coef *= 0.95;
                        }

                        WRITE_LOG(moveName << " " << score);

                        if (score > bestScore)
                        {
                            bestMove = firstMove;
                            bestFire = fireAim;
                            bestScore = score;
                            friendlyFire = sim.friendlyFireByUnitId[unit.id];
                            bestMyFirePos = myFirePos;

                            WRITE_LOG("SC " << moveName << " " << score);
                        }


                    }
                }
            }

            // ============

            myMoves[unit.id] = bestMove;
            unit.action = MyAction();
            applyMove(bestMove, unit.action);
            WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

            // ============

            if (nearestEnemy->canJump)
                enemyPos = nearestEnemy->pos;

            {
                renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                }
                else
                {
                    if (unit.weapon.fireTimerMicrotics > 400)
                    {
                        unit.action.aim = (enemyPos - unit.pos).norm();
                    }
                    else if (bestFire != P(0, 0))
                    {
                        unit.action.aim = bestFire;

                        //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                        normalizeAim(unit, enemyPos);
                    }
                    else
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                    }
                }

                WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
            }

        }
    }
}

}


namespace v28 {

void DistanceMap::compute(const Simulator &sim, IP ip) {
    w = sim.level->w;
    h = sim.level->h;
    distMap.clear();
    distMap.resize(w * h, -1);

    ip.limitWH(w, h);

    std::deque<IP> pts;
    pts.push_back(ip);

    distMap[ip.ind(w)] = 0;

    while (!pts.empty())
    {
        IP p = pts.front();
        pts.pop_front();

        double v = distMap[p.ind(w)] + 1;

        for (int y = p.y - 1; y <= p.y + 1; ++y)
        {
            if (sim.level->isValidY(y))
            {
                for (int x = p.x - 1; x <= p.x + 1; ++x)
                {
                    if (sim.level->isValidX(x))
                    {
                        int ind = IP(x, y).ind(w);

                        if (distMap[ind] < 0)
                        {
                            Tile t = sim.level->getTile(x, y);
                            if (t != WALL)
                            {
                                distMap[ind] = v;
                                pts.push_back(IP(x, y));
                            }
                        }
                    }
                }
            }
        }
    }
}


void NavMeshV2::compute(const MyLevel &level)
{
    w = level.w;
    h = level.h;

    nodes.resize(level.w * level.h);

    auto canMove = [&level](int x, int y) {
        return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
    };

    for (int y = 1; y < level.h - 1; ++y)
    {
        for (int x = 1; x < level.w - 1; ++x)
        {
            //if (x == 31 && y == 27)
            //    LOG("AA");

            if (canMove(x, y))
            {
                std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                bool onLadder = level.getTile(x, y) == LADDER;
                bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                bool onGround;

                bool cm_l = canMove(x - 1, y);
                bool cm_r = canMove(x + 1, y);
                bool cm_u = canMove(x, y + 1);
                bool cm_d = canMove(x, y - 1);
                bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                if (!isJumppadBody || isWallOrLadderAbove)
                {
                    // move left
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_l)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1;
                        nd0.links.push_back(l);
                    }

                    // left down
                    if (!isJumppadBody && cm_ld)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1 - level.w;
                        nd0.links.push_back(l);
                    }

                    // move right
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_r)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1;
                        nd0.links.push_back(l);
                    }

                    // right down
                    if (!isJumppadBody && cm_rd)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1 - level.w;
                        nd0.links.push_back(l);
                    }


                    // moveDown

                    if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -level.w;
                        nd0.links.push_back(l);
                    }

                }

                // move up
                if (cm_u)
                {
                    if (!isJumppadBody)
                    {
                        for (int i = 1; i <= 5; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 6;
                            l.targetMomentum = i - 1;
                            l.targetCanCancel = true;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }
                    }

                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 3;
                        l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                        l.targetCanCancel = l.targetMomentum == 0;
                        l.deltaPos = level.w;
                        node.links.push_back(l);
                    }

                    // move left up
                    if (cm_lu)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = -1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = -1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }

                    // move right up
                    if (cm_ru)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = 1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = +1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }
                }

                // stop jumping
                for (int i = 1; i <= 5; ++i)
                {
                    Node& node = cellNodes[i];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 0;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // start jumping
                onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                           || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                           || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                           || onLadder;

                if (onGround)
                {
                    Node& node = cellNodes[0];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 5;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // hit ceiling

                bool ceiling = isWallOrLadderAbove
                               || wall(level.getTile(x - 1, y + 2))
                               || wall(level.getTile(x + 1, y + 2));

                if (ceiling && !isJumppadCurrent)
                {
                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }
                }

                if (!onLadder && !onLadderUpper)
                {
                    // jump on jumppad
                    bool isJumppad = false;
                    for (int dy = -1; dy <= 2; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                            {
                                isJumppad = true;
                                break;
                            }
                        }
                    }

                    if (isJumppad)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 10;
                            l.targetCanCancel = false;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }
                }
            }
        }
    }
}

void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
{
    w = navMesh.w;
    h = navMesh.h;

    distMap.clear();
    distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
    prevNode.clear();
    prevNode.resize(w * h * MOM_N, -1);
#endif

    typedef std::pair<uint16_t , PP> NLink;
    auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

    std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

    pts.push(std::make_pair(0, pp));
    distMap[pp.ind()] = 0;

    while (!pts.empty())
    {
        PP p = pts.top().second;
        pts.pop();

        int pInd = p.ind();
        uint16_t curDist = distMap[pInd];
        const Node &node = navMesh.nodes[p.pos][p.innerInd()];

        //LOG("FROM " << p.toString(w) << " d " << curDist);

        for (const auto &link : node.links)
        {
            PP v;
            v.pos = p.pos + link.deltaPos;
            v.upMomentum = link.targetMomentum;
            v.canCancel = link.targetCanCancel;

            uint16_t newD = curDist + link.cost;
            int vInd = v.ind();
            if (distMap[vInd] > newD)
            {
                distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                prevNode[vInd] = pInd;
#endif
                pts.push(std::make_pair(newD, v));

                //LOG("TO " << v.toString(w) << " d " << newD);
            }
        }
    }
}


void applyMove(const Move &m, MyAction &action)
{
    action.velocity = m.vel;
    if (m.jump == 1)
        action.jump = true;
    else if (m.jump == -1)
        action.jumpDown = true;
}

DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
{
    PP p = strat.navMesh.makePP(u);
    int posInd = p.ind();

    auto it = strat.distMap.find(posInd);
    if (it != strat.distMap.end())
    {
        strat.lruCache.erase(it->second.lruCacheIt);
        strat.lruCache.push_front(posInd);
        it->second.lruCacheIt = strat.lruCache.begin();
        return it->second;
    }

    if (strat.lruCache.size() > MAX_DMAPS)
    {
        int last = strat.lruCache.back();
        strat.lruCache.pop_back();
        strat.distMap.erase(last);

        //LOG("REMOVE CACHE");
    }

    DistanceMapV2 &res = strat.distMap[posInd];
    res.compute(strat.navMesh, p);

    strat.lruCache.push_front(posInd);
    res.lruCacheIt = strat.lruCache.begin();
    //LOG("CASHE SIZE " << strat.lruCache.size());

    return res;
}

DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
{
    int posInd = p.ind(strat.sim.level->w);

    auto it = strat.simpleDistMap.find(posInd);
    if (it != strat.simpleDistMap.end())
        return it->second;

    DistanceMap &res = strat.simpleDistMap[posInd];
    res.compute(strat.sim, p);
    return res;
}

struct DistComputer {
    DistanceMapV2 &dmap;
    DistanceMap &sdmap;
    MyStrat &strat;

    DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
    }

    double getDist(const P &p, double simpleDistMapCoef) const {

        double d1 = dmap.getDist(strat.navMesh.makePP(p));
        double d2 = sdmap.getDist(p);
        double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

        /*static double sd1 = 0;
        static double sd2 = 0;

        if (d1 < 200 && d2 < 200)
        {
            static int c = 0;
            sd1 += d1;
            sd2 += d2;
            ++c;
            if (c % 100000 == 0)
                LOG(sd1 / sd2);
        }
        else
        {
            LOG(d1 << " " << d2);
        }*/
        return dist;
    }

    double getDistM0(const MyUnit &u, double simpleDistMapCoef)
    {
        double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
        return dist;
    }
};

double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth, int curTick, const std::vector<std::pair<P, double/*T*/>> &RLColPoints)
{
    double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

    double s0 = score;
    double lbScore = 0;
    for (const MyLootBox &l : sim.lootBoxes)
    {
        if (l.type == MyLootBoxType ::HEALTH_PACK)
        {
            double minDist = 1e9;
            Side side = Side::NONE;
            double deltaHealth = 0;
            for (const MyUnit &unit : sim.units)
            {
                DistComputer dcomp(unit, strat);

                double dist = dcomp.getDist(l.pos, sdScore);
                if (unit.isMine())
                    dist *= 1.1;

                if (dist < minDist)
                {
                    minDist = dist;
                    side = unit.side;
                    deltaHealth = 100 - unit.health;
                }
            }

            if (side == Side::ME)
                lbScore += (deltaHealth * 0.1);
            else
                lbScore -= (deltaHealth * 0.1);
        }
    }

    if (lbScore > 30)
        lbScore = 30;

    score += lbScore;
    //WRITE_LOG("++++++++++++ " << lbScore);

    for (const MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                score += unit.health * 0.5;
            else
                score += unit.health;

            if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
            {
                score -= 1;
            }

            IP ipos = IP(unit.pos);

            DistComputer dcomp(unit, strat);

            double magazineRel = 1.0;
            if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
            {
                magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
            }

            bool reload = false;
            if (unit.weapon)
            {
                score += 100;

                reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
            }
            else
            {
                double wScore = 0;
                double wCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::WEAPON)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        wScore += 50.0 / (dist + 1.0);
                        wCount++;
                    }
                }

                if (wCount > 0)
                    score += wScore / wCount;
            }

            double hpScore = 0;

            {
                double hScore = 0;
                double hCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        hScore += 50.0 / (dist + 1.0);
                        hCount++;
                    }
                }

                if (hCount > 0)
                    hpScore = hScore / hCount;
            }

            double enScore = 0;
            if (unit.weapon)
            {
                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        bool enReload = false;
                        if (en.weapon)
                            enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                        double dist = dcomp.getDistM0(en, sdScore);

                        if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                        {
                            enScore -= (10 - dist)*2;
                            // WRITE_LOG("===")
                        }


                        if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                        {
                            double keepDist = 0;


                            if ((reload /*|| magazineRel < 0.5*/))
                                keepDist = 10;

                            if (en.weapon)
                            {
                                const double D = 5;
                                if (dist < D)
                                {
                                    bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                    bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                    if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                        enScore -= (D - dist)*5;

                                    /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                    WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                    WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                    if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                        enScore -= (D - dist)*2;
                                    else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                        enScore += (D - dist)*2;
                                }
                            }

                            //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            //    keepDist = 5;
                            //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                            //    keepDist = 3;

                            WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)) << " ep " << en.pos << " eh " << en.health);
                            if (keepDist > 0)
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                            else
                                enScore += 50.0 / (dist + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                        }
                        else
                        {
                            enScore -= 50.0 / (dist + 5.0);
                        }
                    }
                }

                score += enScore;
            }

            for (const MyUnit &en : sim.units)
            {
                if (en.side == Side::EN)
                {
                    if (en.weapon && en.pos.dist(unit.pos) < 5 && dot((unit.pos - en.pos).norm(), en.weapon.lastAngle.norm()) >= 0.5)
                    {
                        if (en.weapon.fireTimerMicrotics <= 100 && (!unit.canJump || !unit.canCancel))
                        {
                            score -= 10.0;
                        }
                    }
                }
            }

            double rlScore = 0;

            if (!RLColPoints.empty())
            {
                BBox bb = unit.getBBox();
                bb.expand(getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->explRadius + 0.1);

                for (const auto &p : RLColPoints)
                {
                    if (p.second < 15 && bb.contains(p.first) && curTick <= (p.second + 1))
                    {
                        double ticksToExpl = p.second - curTick;
                        rlScore -= 2.0 * std::max(0.0, (10.0 - p.first.dist(unit.getCenter()))) / (ticksToExpl + 1.0);
                        //WRITE_LOG("RL " << ticksToExpl << " t " << p.second << " s " << rlScore << " d " << p.first.dist(unit.getCenter()));
                        //LOG(rlScore);
                    }

                }
            }



            if (enScore < 0 && unit.health < 100)
            {
                score += hpScore;
            }

            if (curHealth[unit.id] < 100)
                score += hpScore * (100 - curHealth[unit.id]) / 200.0;

            score += rlScore;

            //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health << " s0 " << s0 << " lb " << lbScore << " rl " << rlScore);
        }
        else
        {
            score -= unit.health * 0.5;
        }
    }

    return score;
}

const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
{
    IP ipos = IP(unit.pos);

    DistComputer dcomp(unit, strat);

    const MyUnit *nearestEnemy = nullptr;
    double dmapDist = 1e9;

    for (const MyUnit &other : sim.units)
    {
        if (other.side != unit.side)
        {
            double dist = dcomp.getDistM0(other, sdScore);

            if (unit.weapon)
            {
                /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                dist += (1.0 - dt) * 1.0;

                dist += other.health * 0.1;*/
            }
            if (dist < dmapDist)
            {
                dmapDist = dist;
                nearestEnemy = &other;
            }
        }
    }

    return nearestEnemy;
}



void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
        bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
        if (!b1 && !b2)
        {
            unit.action.aim = unit.weapon.lastAngle;
            return;
        }

        if (b1 && b2)
            return;

        if (b1)
        {
            double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
        else if (b2)
        {
            double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P((ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
    }
}

void normalizeAim(MyUnit &unit, const P &enemyPos)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        P center = unit.getCenter();
        BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
        P lowestAngle = (enBox.getCorner(0) - center).norm();
        P largestAngle = lowestAngle;
        for (int i = 1; i < 4; ++i)
        {
            P angle = (enBox.getCorner(i) - center).norm();
            if (isAngleLessThan(angle, lowestAngle))
                lowestAngle = angle;

            if (isAngleLessThan(largestAngle, angle))
                largestAngle = angle;
        }

        normalizeAim(unit, lowestAngle, largestAngle);
    }
}

template<int RAYS_N, bool log>
void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
{
    for (MyUnit &unit : sim.units)
    {
        if (unitId != -1 && unit.id != unitId)
            continue;

        if (unit.side == Side::ME && unit.weapon)
        {
            const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

            if (!nearestEnemy)
                return;

            P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
            P aim = (enemyPos - unit.pos).norm();
            unit.action.aim = aim;

            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

            normalizeAim(unit, enemyPos);

            //unit.action.aim = (enemyPos - bestMyFirePos).norm();
            //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
            unit.action.shoot = true;

            double spread = unit.weapon.spread;
            if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
            {
                //const int RAYS_N = 10;

                P ddir = P(spread * 2.0 / (RAYS_N - 1));
                P ray = unit.action.aim.rotate(-spread);

                P center = unit.getCenter();

                MyWeaponParams *wparams = unit.weapon.params;
                BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                int myUnitsHits = 0;
                double enUnitsHits = 0;

                for (int i = 0; i < RAYS_N; ++i)
                {
                    P tp = sim.level->bboxWallCollision2(missile, ray);

                    MyUnit *directHitUnit = nullptr;
                    double directHitDist = 1e9;
                    for (MyUnit &mu : sim.units) // direct hit
                    {
                        if (mu.id != unit.id)
                        {
                            auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                            if (isCollided)
                            {
                                double d1 = tp.dist2(center);
                                double d2 = colP.dist2(center);

                                if (d2 < d1 && d2 < directHitDist)
                                {
                                    directHitDist = d2;
                                    directHitUnit = &mu;
                                }
                            }
                        }
                    }

                    if (directHitUnit)
                    {
                        if (directHitUnit->isMine())
                            myUnitsHits++;
                        else
                        {
                            BBox b2 = directHitUnit->getBBox();
                            double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                            double expD = dst / 10.0;
                            if (expD > 0.4)
                                expD = 0.4;
                            b2.expand(-expD);

                            auto [isCollided, colP] = b2.rayIntersection(center, ray);
                            if (isCollided)
                            {
                                if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                {
                                    enUnitsHits += 0.09;
                                }
                                else
                                {
                                    enUnitsHits++;
                                }
                                //LOG("EXP " << expD);
                            }
                            else
                                enUnitsHits += 0.09;
                        }
                    }

                    // Explosion damage
                    if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                    {
                        double explRad = wparams->explRadius;

                        for (MyUnit &mu : sim.units)
                        {
                            double dist = tp.dist(mu.pos);

                            if (mu.isMine() && dist < explRad * 1.2)
                            {
                                myUnitsHits++;
                            }
                            else if (mu.isEnemy() && dist < explRad * 0.7)
                            {
                                enUnitsHits++;
                            }
                        }
                    }

                    ray = ray.rotate(ddir);
                }

                if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                    unit.action.shoot = false;

                if (log)
                    WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
            }

        }

        if (unit.isMine())
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                unit.action.swapWeapon = true;

            //if (unit.weapon.weaponType == MyWeaponType ::ASSAULT_RIFLE /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
            //    unit.action.swapWeapon = true;

            /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
            {
                unit.action.reload = true;
            }*/
        }
    }
}

void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
{
#ifdef ENABLE_LOGGING
    DistanceMapV2 dmap = getDistMap(from, s);

    PP p = s.navMesh.makePP(to);
    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = dmap.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    renderPath(*s.sim.level, nodes);
#endif
}

void MyStrat::compute(const Simulator &_sim)
{
    double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
    //LOG(SDMAP_COEF);

    std::map<int, Move> enemyMoves;

    if (_sim.curTick > 0)
    {
        for (const MyUnit &u : _sim.units)
        {
            if (u.isEnemy())
            {
                const MyUnit *oldUnit = this->sim.getUnit(u.id);
                if (oldUnit)
                {
                    Move &m = enemyMoves[u.id];
                    m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                    if (u.pos.y > oldUnit->pos.y)
                    {
                        m.jump = 1;
                    }
                    else if (u.pos.y > oldUnit->pos.y && u.canJump)
                    {
                        m.jump = -1;
                    }
                }
            }
        }
    }

    this->sim = _sim;

    bool isLoosing = sim.score[0] <= sim.score[1];

    /*for (MyUnit &u : sim.units)
    {
        if (u.side == Side::ME)
        {
            u.action = MyAction();
        }
    }

    return;*/

    if (!navMesh.w)
    {
        navMesh.compute(*sim.level);
    }

    std::map<int, int> curHealth;
    for (MyUnit &unit : sim.units)
    {
        curHealth[unit.id] = unit.health;
    }

    std::vector<std::pair<P, double/*T*/>> RLColPoints;
    for (MyBullet &b : sim.bullets)
    {
        if (b.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
        {
            BBox bbox = b.getBBox();
            P tp = sim.level->bboxWallCollision2(bbox, b.vel);
            double t = (tp - b.pos).len() / b.params->bulletSpeed * 60;
            RLColPoints.emplace_back(tp, t);
        }
    }

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

            double bestScore = -100000;
            Move bestMove;
            P bestFire = P(0, 0);
            double friendlyFire = 0;
            P bestMyFirePos = unit.pos;

            const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

            if (nearestEnemy)
                renderPathTo(*this, unit, *nearestEnemy);

            P enemyPos = P(0, 0);
            const int TICKN = 25;
            int bulletFlyTime = 0;
            double enemyDist = 1000.0;
            int ticksToFire = 0;
            if (nearestEnemy && unit.weapon)
            {
                enemyPos = nearestEnemy->pos;
                enemyDist = enemyPos.dist(unit.pos);
                bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                ticksToFire = unit.weapon.fireTimerMicrotics / 100;
            }

            if (bulletFlyTime >= TICKN)
                bulletFlyTime = TICKN - 1;


            {

                int jumpN = 3;
                if (unit.canJump)
                    jumpN = 4;

                for (int dir = 0; dir < 3; ++dir)
                {
                    for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                    {
                        Simulator sim = _sim;
                        sim.enFireAtCenter = true;
                        sim.score[0] = 0;
                        sim.score[1] = 0;
                        sim.selfFire[0] = 0;
                        sim.selfFire[1] = 0;
                        sim.friendlyFireByUnitId.clear();

                        P myFirePos = unit.pos;
                        P fireAim = P(0, 0);

                        P p1 = unit.pos;

                        std::string moveName;
                        if (dir == 0)
                        {
                            moveName = "left ";
                        }
                        else if (dir == 1)
                        {
                            moveName = "right ";
                        }

                        if (jumpS == 0)
                        {
                            moveName += "up";
                        }
                        else if (jumpS == 1)
                        {
                            moveName += "down";
                        }
                        else if (jumpS == 3)
                        {
                            moveName += "downup";
                        }

                        Move firstMove;
                        IP ipos = IP(unit.pos);

                        double score = 0;
                        double coef = 1.0;
                        bool fired = false;
                        int health = unit.health;
                        //std::vector<Simulator> saveSim;

                        for (int tick = 0; tick < TICKN; ++tick)
                        {
                            MyUnit *simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                                break;

                            double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                            simUnit->action = MyAction();
                            for (MyUnit &su : sim.units)
                            {
                                su.action = MyAction();

                                MyAction &action = su.action;
                                action.shoot = true;
                                if (su.weapon)
                                {
                                    action.aim = su.weapon.lastAngle;

                                    if (tick >= 5 && su.isEnemy())
                                    {
                                        const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                        if (een)
                                        {
                                            action.aim = P(een->pos - su.pos).norm();
                                        }
                                    }
                                }

                                if (su.side == Side::ME && su.id != unit.id)
                                {
                                    applyMove(myMoves[su.id], action);
                                }
                                else if (tick <= 5 && su.isEnemy())
                                {
                                    Move &em = enemyMoves[su.id];
                                    applyMove(em, su.action);
                                }
                            }

                            if (dir == 0)
                            {
                                simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                            }
                            else if (dir == 1)
                            {
                                simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                            }



                            if (jumpS == 0)
                            {
                                simUnit->action.jump = true;
                            }
                            else if (jumpS == 1)
                            {
                                simUnit->action.jumpDown = true;
                            }

                            if (jumpS == 3 && tick == 0)
                            {
                                simUnit->action.jump = false;
                            }

                            if (jumpS == 2 || (jumpS == 3 && tick > 0))
                            {
                                if (dir == 0)
                                {
                                    IP ip = IP(simUnit->pos - P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                                else if (dir == 1)
                                {
                                    IP ip = IP(simUnit->pos + P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                            }

                            if (tick == 0)
                            {
                                firstMove.vel = simUnit->action.velocity;
                                if (simUnit->action.jump)
                                    firstMove.jump = 1;
                                else if (simUnit->action.jumpDown)
                                    firstMove.jump = -1;
                            }

                            computeFire<10, false>(sim, *this, sdScore, {});

                            /*{
                                for (MyBullet &b : sim.bullets)
                                {
                                    BBox bb = b.getBBox();

                                    uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                    renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                    renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                }
                            }*/


                            if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                            {
                                fireAim = simUnit->action.aim;
                                fired = true;
                            }

                            //saveSim.push_back(sim);
                            sim.tick();

                            {
                                for (MyUnit &u : sim.units)
                                {
                                    if (u.isEnemy())
                                    {
                                        renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                        renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                    }
                                }
                            }

                            simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                            {
                                score -= 1000;
                                break;
                            }

                            {
                                if (simUnit->health != health)
                                {
                                    uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                    renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                    WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                    health = simUnit->health;
                                }
                                else
                                {
                                    uint32_t  c = 0x000000ccu;
                                    renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                }
                            }

                            {
                                P p2 = simUnit->pos;
                                renderLine(p1, p2, 0x000000ffu);
                                p1 = p2;
                            }

                            if (tick == bulletFlyTime && nearestEnemy)
                            {
                                MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                if (newEnemy)
                                    enemyPos = newEnemy->pos;
                            }

                            if (tick == ticksToFire && nearestEnemy)
                            {
                                myFirePos = simUnit->pos;
                            }

                            /*IP newIpos = IP(unit.pos);

                            if (newIpos != ipos)
                            {
                                if (navMesh.get(newIpos).canWalk)
                                {
                                    ipos = newIpos;
                                }
                            }*/

                            double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth, tick, RLColPoints);
                            if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                            {
                                WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                            }

                            score += lscore * coef;

                            /*if (tick < 3)
                                coef *= 0.95;
                            else
                                coef *= 0.9;*/
                            coef *= 0.95;
                        }

                        WRITE_LOG(moveName << " " << score);

                        if (score > bestScore)
                        {
                            bestMove = firstMove;
                            bestFire = fireAim;
                            bestScore = score;
                            friendlyFire = sim.friendlyFireByUnitId[unit.id];
                            bestMyFirePos = myFirePos;

                            WRITE_LOG("SC " << moveName << " " << score);
                        }


                    }
                }
            }

            // ============

            myMoves[unit.id] = bestMove;
            unit.action = MyAction();
            applyMove(bestMove, unit.action);
            WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

            // ============

            if (nearestEnemy->canJump)
                enemyPos = nearestEnemy->pos;

            {
                renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                }
                else
                {
                    if (unit.weapon.fireTimerMicrotics > 400)
                    {
                        unit.action.aim = (enemyPos - unit.pos).norm();
                    }
                    else if (bestFire != P(0, 0))
                    {
                        unit.action.aim = bestFire;

                        //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                        normalizeAim(unit, enemyPos);
                    }
                    else
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                    }
                }

                WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
            }

        }
    }
}

}


namespace v36 {

    void DistanceMap::compute(const Simulator &sim, IP ip) {
        w = sim.level->w;
        h = sim.level->h;
        distMap.clear();
        distMap.resize(w * h, -1);

        ip.limitWH(w, h);

        std::deque<IP> pts;
        pts.push_back(ip);

        distMap[ip.ind(w)] = 0;

        while (!pts.empty())
        {
            IP p = pts.front();
            pts.pop_front();

            double v = distMap[p.ind(w)] + 1;

            for (int y = p.y - 1; y <= p.y + 1; ++y)
            {
                if (sim.level->isValidY(y))
                {
                    for (int x = p.x - 1; x <= p.x + 1; ++x)
                    {
                        if (sim.level->isValidX(x))
                        {
                            int ind = IP(x, y).ind(w);

                            if (distMap[ind] < 0)
                            {
                                Tile t = sim.level->getTile(x, y);
                                if (t != WALL)
                                {
                                    distMap[ind] = v;
                                    pts.push_back(IP(x, y));
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void NavMeshV2::compute(const MyLevel &level)
    {
        w = level.w;
        h = level.h;

        nodes.resize(level.w * level.h);

        auto canMove = [&level](int x, int y) {
            return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
        };

        for (int y = 1; y < level.h - 1; ++y)
        {
            for (int x = 1; x < level.w - 1; ++x)
            {
                //if (x == 31 && y == 27)
                //    LOG("AA");

                if (canMove(x, y))
                {
                    std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                    bool onLadder = level.getTile(x, y) == LADDER;
                    bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                    bool onGround;

                    bool cm_l = canMove(x - 1, y);
                    bool cm_r = canMove(x + 1, y);
                    bool cm_u = canMove(x, y + 1);
                    bool cm_d = canMove(x, y - 1);
                    bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                    bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                    bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                    bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                    bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                    bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                    bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                    bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                    bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                    if (!isJumppadBody || isWallOrLadderAbove)
                    {
                        // move left
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_l)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1;
                            nd0.links.push_back(l);
                        }

                        // left down
                        if (!isJumppadBody && cm_ld)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -1 - level.w;
                            nd0.links.push_back(l);
                        }

                        // move right
                        onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                                   || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                                   || onLadder;

                        if ((onGround || onLadderUpper) && cm_r)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1;
                            nd0.links.push_back(l);
                        }

                        // right down
                        if (!isJumppadBody && cm_rd)
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 1 - level.w;
                            nd0.links.push_back(l);
                        }


                        // moveDown

                        if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                        {
                            Node &nd0 = cellNodes[0];
                            Link l;
                            l.cost = 6;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = -level.w;
                            nd0.links.push_back(l);
                        }

                    }

                    // move up
                    if (cm_u)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = level.w;
                                node.links.push_back(l);
                            }
                        }

                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 3;
                            l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                            l.targetCanCancel = l.targetMomentum == 0;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }

                        // move left up
                        if (cm_lu)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = -1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = -1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }

                        // move right up
                        if (cm_ru)
                        {
                            if (!isJumppadBody)
                            {
                                for (int i = 1; i <= 5; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = i - 1;
                                    l.targetCanCancel = true;
                                    l.deltaPos = 1 + level.w;
                                    node.links.push_back(l);
                                }
                            }

                            if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                            {
                                for (int i = 7; i < 16; ++i)
                                {
                                    Node& node = cellNodes[i];

                                    Link l;
                                    l.cost = 6;
                                    l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                    l.targetCanCancel = l.targetMomentum == 0;
                                    l.deltaPos = +1 + 2 * level.w;
                                    node.links.push_back(l);
                                }
                            }
                        }
                    }

                    // stop jumping
                    for (int i = 1; i <= 5; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // start jumping
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if (onGround)
                    {
                        Node& node = cellNodes[0];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 5;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }

                    // hit ceiling

                    bool ceiling = isWallOrLadderAbove
                                   || wall(level.getTile(x - 1, y + 2))
                                   || wall(level.getTile(x + 1, y + 2));

                    if (ceiling && !isJumppadCurrent)
                    {
                        for (int i = 6; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 0;
                            l.targetCanCancel = true;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }

                    if (!onLadder && !onLadderUpper)
                    {
                        // jump on jumppad
                        bool isJumppad = false;
                        for (int dy = -1; dy <= 2; ++dy)
                        {
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                                {
                                    isJumppad = true;
                                    break;
                                }
                            }
                        }

                        if (isJumppad)
                        {
                            for (int i = 0; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 1;
                                l.targetMomentum = 10;
                                l.targetCanCancel = false;
                                l.deltaPos = 0;
                                node.links.push_back(l);
                            }
                        }
                    }
                }
            }
        }
    }

    void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
    {
        w = navMesh.w;
        h = navMesh.h;

        distMap.clear();
        distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
        prevNode.clear();
        prevNode.resize(w * h * MOM_N, -1);
#endif

        typedef std::pair<uint16_t , PP> NLink;
        auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

        std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

        pts.push(std::make_pair(0, pp));
        distMap[pp.ind()] = 0;

        while (!pts.empty())
        {
            PP p = pts.top().second;
            pts.pop();

            int pInd = p.ind();
            uint16_t curDist = distMap[pInd];
            const Node &node = navMesh.nodes[p.pos][p.innerInd()];

            //LOG("FROM " << p.toString(w) << " d " << curDist);

            for (const auto &link : node.links)
            {
                PP v;
                v.pos = p.pos + link.deltaPos;
                v.upMomentum = link.targetMomentum;
                v.canCancel = link.targetCanCancel;

                uint16_t newD = curDist + link.cost;
                int vInd = v.ind();
                if (distMap[vInd] > newD)
                {
                    distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                    prevNode[vInd] = pInd;
#endif
                    pts.push(std::make_pair(newD, v));

                    //LOG("TO " << v.toString(w) << " d " << newD);
                }
            }
        }
    }


    void applyMove(const Move &m, MyAction &action)
    {
        action.velocity = m.vel;
        if (m.jump == 1)
            action.jump = true;
        else if (m.jump == -1)
            action.jumpDown = true;
    }

    DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
    {
        PP p = strat.navMesh.makePP(u);
        int posInd = p.ind();

        auto it = strat.distMap.find(posInd);
        if (it != strat.distMap.end())
        {
            strat.lruCache.erase(it->second.lruCacheIt);
            strat.lruCache.push_front(posInd);
            it->second.lruCacheIt = strat.lruCache.begin();
            return it->second;
        }

        if (strat.lruCache.size() > MAX_DMAPS)
        {
            int last = strat.lruCache.back();
            strat.lruCache.pop_back();
            strat.distMap.erase(last);

            //LOG("REMOVE CACHE");
        }

        DistanceMapV2 &res = strat.distMap[posInd];
        res.compute(strat.navMesh, p);

        strat.lruCache.push_front(posInd);
        res.lruCacheIt = strat.lruCache.begin();
        //LOG("CASHE SIZE " << strat.lruCache.size());

        return res;
    }

    DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
    {
        int posInd = p.ind(strat.sim.level->w);

        auto it = strat.simpleDistMap.find(posInd);
        if (it != strat.simpleDistMap.end())
            return it->second;

        DistanceMap &res = strat.simpleDistMap[posInd];
        res.compute(strat.sim, p);
        return res;
    }

    struct DistComputer {
        DistanceMapV2 &dmap;
        DistanceMap &sdmap;
        MyStrat &strat;

        DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
        }

        double getDist(const P &p, double simpleDistMapCoef) const {

            double d1 = dmap.getDist(strat.navMesh.makePP(p));
            double d2 = sdmap.getDist(p);
            double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

            /*static double sd1 = 0;
            static double sd2 = 0;

            if (d1 < 200 && d2 < 200)
            {
                static int c = 0;
                sd1 += d1;
                sd2 += d2;
                ++c;
                if (c % 100000 == 0)
                    LOG(sd1 / sd2);
            }
            else
            {
                LOG(d1 << " " << d2);
            }*/
            return dist;
        }

        double getDistM0(const MyUnit &u, double simpleDistMapCoef)
        {
            double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
            return dist;
        }
    };

    double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth, int curTick, const std::vector<std::pair<P, double/*T*/>> &RLColPoints)
    {
        double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

        double s0 = score;
        double lbScore = 0;
        for (const MyLootBox &l : sim.lootBoxes)
        {
            if (l.type == MyLootBoxType ::HEALTH_PACK)
            {
                double minDist = 1e9;
                Side side = Side::NONE;
                double deltaHealth = 0;
                for (const MyUnit &unit : sim.units)
                {
                    DistComputer dcomp(unit, strat);

                    double dist = dcomp.getDist(l.pos, sdScore);
                    if (unit.isMine())
                        dist *= 1.1;

                    if (dist < minDist)
                    {
                        minDist = dist;
                        side = unit.side;
                        deltaHealth = 100 - curHealth[unit.id];
                    }
                }

                if (side == Side::ME)
                    lbScore += (deltaHealth * 0.1);
                else
                    lbScore -= (deltaHealth * 0.1);
            }
        }

        if (lbScore > 30)
            lbScore = 30;

        score += lbScore;
        //WRITE_LOG("++++++++++++ " << lbScore);

        for (const MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                    score += unit.health * 0.5;
                else
                    score += unit.health;

                if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
                {
                    score -= 1;
                }

                IP ipos = IP(unit.pos);

                DistComputer dcomp(unit, strat);

                double magazineRel = 1.0;
                if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
                {
                    magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
                }

                bool reload = false;
                if (unit.weapon)
                {
                    score += 100;

                    reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
                }
                else
                {
                    double wScore = 0;
                    double wCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::WEAPON)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            wScore += 50.0 / (dist + 1.0);
                            wCount++;
                        }
                    }

                    if (wCount > 0)
                        score += wScore / wCount;
                }

                double hpScore = 0;

                {
                    double hScore = 0;
                    double hCount = 0;

                    for (const MyLootBox &l : sim.lootBoxes)
                    {
                        if (l.type == MyLootBoxType ::HEALTH_PACK)
                        {
                            double dist = dcomp.getDist(l.pos, sdScore);
                            hScore += 50.0 / (dist + 1.0);
                            hCount++;
                        }
                    }

                    if (hCount > 0)
                        hpScore = hScore / hCount;
                }

                double enScore = 0;
                if (unit.weapon)
                {
                    for (const MyUnit &en : sim.units)
                    {
                        if (en.side == Side::EN)
                        {
                            bool enReload = false;
                            if (en.weapon)
                                enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                            double dist = dcomp.getDistM0(en, sdScore);

                            if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                            {
                                enScore -= (10 - dist)*2;
                                // WRITE_LOG("===")
                            }


                            if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                            {
                                double keepDist = 0;


                                if ((reload /*|| magazineRel < 0.5*/))
                                    keepDist = 10;

                                if (en.weapon)
                                {
                                    const double D = 5;
                                    if (dist < D)
                                    {
                                        bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                        bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                        if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                            enScore -= (D - dist)*5;

                                        /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                        WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                        WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                        if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                            enScore -= (D - dist)*2;
                                        else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                            enScore += (D - dist)*2;
                                    }
                                }

                                //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                //    keepDist = 5;
                                //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                                //    keepDist = 3;

                                WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)) << " ep " << en.pos << " eh " << en.health);
                                if (keepDist > 0)
                                    enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                                else
                                    enScore += 50.0 / (dist + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                            }
                            else
                            {
                                enScore -= 50.0 / (dist + 5.0);
                            }
                        }
                    }

                    score += enScore;
                }

                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        if (en.weapon && en.pos.dist(unit.pos) < 5 && dot((unit.pos - en.pos).norm(), en.weapon.lastAngle.norm()) >= 0.5)
                        {
                            if (en.weapon.fireTimerMicrotics <= 100 && (!unit.canJump || !unit.canCancel))
                            {
                                score -= 10.0;
                            }
                        }
                    }
                }

                double rlScore = 0;

                if (!RLColPoints.empty())
                {
                    BBox bb = unit.getBBox();
                    bb.expand(getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->explRadius + 0.1);

                    for (const auto &p : RLColPoints)
                    {
                        if (p.second < 15 && bb.contains(p.first) && curTick <= (p.second + 1))
                        {
                            double ticksToExpl = p.second - curTick;
                            rlScore -= 2.0 * std::max(0.0, (10.0 - p.first.dist(unit.getCenter()))) / (ticksToExpl + 1.0);
                            //WRITE_LOG("RL " << ticksToExpl << " t " << p.second << " s " << rlScore << " d " << p.first.dist(unit.getCenter()));
                            //LOG(rlScore);
                        }

                    }
                }



                if (enScore < 0 && unit.health < 100)
                {
                    score += hpScore;
                }

                if (curHealth[unit.id] < 100)
                    score += hpScore * (100 - curHealth[unit.id]) / 200.0;

                score += rlScore;

                //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health << " s0 " << s0 << " lb " << lbScore << " rl " << rlScore);
            }
            else
            {
                score -= unit.health * 0.5;
            }
        }

        return score;
    }

    const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
    {
        IP ipos = IP(unit.pos);

        DistComputer dcomp(unit, strat);

        const MyUnit *nearestEnemy = nullptr;
        double dmapDist = 1e9;

        for (const MyUnit &other : sim.units)
        {
            if (other.side != unit.side)
            {
                double dist = dcomp.getDistM0(other, sdScore);

                if (unit.weapon)
                {
                    /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                    dist += (1.0 - dt) * 1.0;

                    dist += other.health * 0.1;*/
                }
                if (dist < dmapDist)
                {
                    dmapDist = dist;
                    nearestEnemy = &other;
                }
            }
        }

        return nearestEnemy;
    }



    void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
    {
        if (unit.weapon.lastAngle != P(0, 0))
        {
            bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
            bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
            if (!b1 && !b2)
            {
                unit.action.aim = unit.weapon.lastAngle;
                return;
            }

            if (b1 && b2)
                return;

            if (b1)
            {
                double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
                //assert(ang > unit.weapon.spread);
                P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
                unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
            }
            else if (b2)
            {
                double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
                //assert(ang > unit.weapon.spread);
                P newAngle = P((ang - unit.weapon.spread) * 0.5);
                unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
            }
        }
    }

    void normalizeAim(MyUnit &unit, const P &enemyPos)
    {
        if (unit.weapon.lastAngle != P(0, 0))
        {
            P center = unit.getCenter();
            BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
            P lowestAngle = (enBox.getCorner(0) - center).norm();
            P largestAngle = lowestAngle;
            for (int i = 1; i < 4; ++i)
            {
                P angle = (enBox.getCorner(i) - center).norm();
                if (isAngleLessThan(angle, lowestAngle))
                    lowestAngle = angle;

                if (isAngleLessThan(largestAngle, angle))
                    largestAngle = angle;
            }

            normalizeAim(unit, lowestAngle, largestAngle);
        }
    }

    bool onLadderOrOnGround(const MyLevel &level, const MyUnit &unit)
    {
        bool enOnLadder = level.getTileSafe(IP(unit.pos)) == LADDER || level.getTileSafe(IP(unit.pos.x, unit.pos.y + 1)) == LADDER;
        if (enOnLadder)
            return true;
        if ((unit.pos.y - std::floor(unit.pos.y)) < 0.001)
        {
            bool onGround = wallOrPlatform(level.getTileSafe(IP(unit.pos.x, unit.pos.y - 1)))
                            || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y)) != WALL)
                            || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y)) != WALL)
            ;

            return onGround;
        }
        return false;

    }

    template<int RAYS_N, bool log>
    void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
    {
        for (MyUnit &unit : sim.units)
        {
            if (unitId != -1 && unit.id != unitId)
                continue;

            if (unit.side == Side::ME && unit.weapon)
            {
                const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

                if (!nearestEnemy)
                    return;

                P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
                P aim = (enemyPos - unit.pos).norm();
                unit.action.aim = aim;

                //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

                normalizeAim(unit, enemyPos);

                //unit.action.aim = (enemyPos - bestMyFirePos).norm();
                //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
                unit.action.shoot = false;


                if (!nearestEnemy->canJump || !nearestEnemy->canCancel || onLadderOrOnGround(*sim.level, *nearestEnemy))
                {
                    unit.action.shoot = true;

                    double spread = unit.weapon.spread;
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        //const int RAYS_N = 10;

                        P ddir = P(spread * 2.0 / (RAYS_N - 1));
                        P ray = unit.action.aim.rotate(-spread);

                        P center = unit.getCenter();

                        MyWeaponParams *wparams = unit.weapon.params;
                        BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                        int myUnitsHits = 0;
                        double enUnitsHits = 0;

                        for (int i = 0; i < RAYS_N; ++i)
                        {
                            P tp = sim.level->bboxWallCollision2(missile, ray);

                            MyUnit *directHitUnit = nullptr;
                            double directHitDist = 1e9;
                            for (MyUnit &mu : sim.units) // direct hit
                            {
                                if (mu.id != unit.id)
                                {
                                    auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                    if (isCollided)
                                    {
                                        double d1 = tp.dist2(center);
                                        double d2 = colP.dist2(center);

                                        if (d2 < d1 && d2 < directHitDist)
                                        {
                                            directHitDist = d2;
                                            directHitUnit = &mu;
                                        }
                                    }
                                }
                            }

                            if (directHitUnit)
                            {
                                if (directHitUnit->isMine())
                                    myUnitsHits++;
                                else
                                {
                                    BBox b2 = directHitUnit->getBBox();
                                    double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                    double expD = dst / 10.0;
                                    if (expD > 0.4)
                                        expD = 0.4;
                                    b2.expand(-expD);

                                    auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                    if (isCollided)
                                    {
                                        if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                        {
                                            enUnitsHits += 0.09;
                                        }
                                        else
                                        {
                                            enUnitsHits++;
                                        }
                                        //LOG("EXP " << expD);
                                    }
                                    else
                                        enUnitsHits += 0.09;
                                }
                            }

                            // Explosion damage
                            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            {
                                double explRad = wparams->explRadius;

                                for (MyUnit &mu : sim.units)
                                {
                                    double dist = tp.maxDist(mu.getCenter());

                                    if (mu.isMine() && dist < (explRad + 1))
                                    {
                                        myUnitsHits++;

                                        if (log)
                                            renderLine(mu.getCenter(), tp, 0xff0000ffu);
                                    }
                                    else if (mu.isEnemy() && dist < explRad * 0.7)
                                    {
                                        enUnitsHits++;
                                    }
                                }
                            }

                            ray = ray.rotate(ddir);
                        }

                        if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                            unit.action.shoot = false;

                        if (log)
                            WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                    }
                }
            }

            if (unit.isMine())
            {
                if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                    unit.action.swapWeapon = true;

                //if (unit.weapon.weaponType == MyWeaponType ::ASSAULT_RIFLE /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                //    unit.action.swapWeapon = true;

                /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
                {
                    unit.action.reload = true;
                }*/
            }
        }
    }

    void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
    {
#ifdef ENABLE_LOGGING
        DistanceMapV2 dmap = getDistMap(from, s);

        PP p = s.navMesh.makePP(to);
        std::vector<int> nodes;
        nodes.push_back(p.ind());

        int ind = p.ind();
        while(true)
        {
            ind = dmap.prevNode[ind];
            if (ind == -1)
                break;

            nodes.push_back(ind);
        }

        renderPath(*s.sim.level, nodes);
#endif
    }

    void MyStrat::compute(const Simulator &_sim)
    {
        double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
        //LOG(SDMAP_COEF);

        std::map<int, Move> enemyMoves;

        if (_sim.curTick > 0)
        {
            for (const MyUnit &u : _sim.units)
            {
                if (u.isEnemy())
                {
                    const MyUnit *oldUnit = this->sim.getUnit(u.id);
                    if (oldUnit)
                    {
                        Move &m = enemyMoves[u.id];
                        m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                        if (u.pos.y > oldUnit->pos.y)
                        {
                            m.jump = 1;
                        }
                        else if (u.pos.y > oldUnit->pos.y && u.canJump)
                        {
                            m.jump = -1;
                        }
                    }
                }
            }
        }

        this->sim = _sim;

        bool isLoosing = sim.score[0] <= sim.score[1];

        /*for (MyUnit &u : sim.units)
        {
            if (u.side == Side::ME)
            {
                u.action = MyAction();
            }
        }

        return;*/

        if (!navMesh.w)
        {
            navMesh.compute(*sim.level);
        }

        std::map<int, int> curHealth;
        for (MyUnit &unit : sim.units)
        {
            curHealth[unit.id] = unit.health;
        }

        std::vector<std::pair<P, double/*T*/>> RLColPoints;
        for (MyBullet &b : sim.bullets)
        {
            if (b.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
            {
                BBox bbox = b.getBBox();
                P tp = sim.level->bboxWallCollision2(bbox, b.vel);
                double t = (tp - b.pos).len() / b.params->bulletSpeed * 60;
                RLColPoints.emplace_back(tp, t);
            }
        }

        for (MyUnit &unit : sim.units)
        {
            if (unit.side == Side::ME)
            {
                WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

                double bestScore = -100000;
                Move bestMove;
                P bestFire = P(0, 0);
                double friendlyFire = 0;
                P bestMyFirePos = unit.pos;

                const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

                if (nearestEnemy)
                    renderPathTo(*this, unit, *nearestEnemy);

                P enemyPos = P(0, 0);
                const int TICKN = 25;
                int bulletFlyTime = 0;
                double enemyDist = 1000.0;
                int ticksToFire = 0;
                if (nearestEnemy && unit.weapon)
                {
                    enemyPos = nearestEnemy->pos;
                    enemyDist = enemyPos.dist(unit.pos);
                    bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                    ticksToFire = unit.weapon.fireTimerMicrotics / 100;
                }

                if (bulletFlyTime >= TICKN)
                    bulletFlyTime = TICKN - 1;


                {

                    int jumpN = 3;
                    if (unit.canJump)
                        jumpN = 4;

                    for (int dir = 0; dir < 3; ++dir)
                    {
                        for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                        {
                            Simulator sim = _sim;
                            sim.enFireAtCenter = true;
                            sim.score[0] = 0;
                            sim.score[1] = 0;
                            sim.selfFire[0] = 0;
                            sim.selfFire[1] = 0;
                            sim.friendlyFireByUnitId.clear();

                            P myFirePos = unit.pos;
                            P fireAim = P(0, 0);

                            P p1 = unit.pos;

                            std::string moveName;
                            if (dir == 0)
                            {
                                moveName = "left ";
                            }
                            else if (dir == 1)
                            {
                                moveName = "right ";
                            }

                            if (jumpS == 0)
                            {
                                moveName += "up";
                            }
                            else if (jumpS == 1)
                            {
                                moveName += "down";
                            }
                            else if (jumpS == 3)
                            {
                                moveName += "downup";
                            }

                            Move firstMove;
                            IP ipos = IP(unit.pos);

                            double score = 0;
                            double coef = 1.0;
                            bool fired = false;
                            int health = unit.health;
                            //std::vector<Simulator> saveSim;

                            for (int tick = 0; tick < TICKN; ++tick)
                            {
                                MyUnit *simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                    break;

                                double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                                simUnit->action = MyAction();
                                for (MyUnit &su : sim.units)
                                {
                                    su.action = MyAction();

                                    MyAction &action = su.action;
                                    action.shoot = true;
                                    if (su.weapon)
                                    {
                                        action.aim = su.weapon.lastAngle;

                                        if (tick >= 5 && su.isEnemy())
                                        {
                                            const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                            if (een)
                                            {
                                                action.aim = P(een->pos - su.pos).norm();
                                            }
                                        }
                                    }

                                    if (su.side == Side::ME && su.id != unit.id)
                                    {
                                        applyMove(myMoves[su.id], action);
                                    }
                                    else if (tick <= 5 && su.isEnemy())
                                    {
                                        Move &em = enemyMoves[su.id];
                                        applyMove(em, su.action);
                                    }
                                }

                                if (dir == 0)
                                {
                                    simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                                }
                                else if (dir == 1)
                                {
                                    simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                                }



                                if (jumpS == 0)
                                {
                                    simUnit->action.jump = true;
                                }
                                else if (jumpS == 1)
                                {
                                    simUnit->action.jumpDown = true;
                                }

                                if (jumpS == 3 && tick == 0)
                                {
                                    simUnit->action.jump = false;
                                }

                                if (jumpS == 2 || (jumpS == 3 && tick > 0))
                                {
                                    if (dir == 0)
                                    {
                                        IP ip = IP(simUnit->pos - P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                    else if (dir == 1)
                                    {
                                        IP ip = IP(simUnit->pos + P(0.5, 0));

                                        if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                        {
                                            simUnit->action.jump = true;
                                        }
                                    }
                                }

                                if (tick == 0)
                                {
                                    firstMove.vel = simUnit->action.velocity;
                                    if (simUnit->action.jump)
                                        firstMove.jump = 1;
                                    else if (simUnit->action.jumpDown)
                                        firstMove.jump = -1;
                                }

                                computeFire<10, false>(sim, *this, sdScore, {});

                                /*{
                                    for (MyBullet &b : sim.bullets)
                                    {
                                        BBox bb = b.getBBox();

                                        uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                        renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                        renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                    }
                                }*/


                                if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                                {
                                    fireAim = simUnit->action.aim;
                                    fired = true;
                                }

                                //saveSim.push_back(sim);
                                sim.tick();

                                {
                                    for (MyUnit &u : sim.units)
                                    {
                                        if (u.isEnemy())
                                        {
                                            renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                            renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                        }
                                    }
                                }

                                simUnit = sim.getUnit(unit.id);
                                if (simUnit == nullptr)
                                {
                                    score -= 1000;
                                    break;
                                }

                                {
                                    if (simUnit->health != health)
                                    {
                                        uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                        renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                        WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                        health = simUnit->health;
                                    }
                                    else
                                    {
                                        uint32_t  c = 0x000000ccu;
                                        renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                        renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                    }
                                }

                                {
                                    P p2 = simUnit->pos;
                                    renderLine(p1, p2, 0x000000ffu);
                                    p1 = p2;
                                }

                                if (tick == bulletFlyTime && nearestEnemy)
                                {
                                    MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                    if (newEnemy)
                                        enemyPos = newEnemy->pos;
                                }

                                if (tick == ticksToFire && nearestEnemy)
                                {
                                    myFirePos = simUnit->pos;
                                }

                                /*IP newIpos = IP(unit.pos);

                                if (newIpos != ipos)
                                {
                                    if (navMesh.get(newIpos).canWalk)
                                    {
                                        ipos = newIpos;
                                    }
                                }*/

                                double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth, tick, RLColPoints);
                                if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                                {
                                    WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                                }

                                score += lscore * coef;

                                /*if (tick < 3)
                                    coef *= 0.95;
                                else
                                    coef *= 0.9;*/
                                coef *= 0.95;
                            }

                            WRITE_LOG(moveName << " " << score);

                            if (score > bestScore)
                            {
                                bestMove = firstMove;
                                bestFire = fireAim;
                                bestScore = score;
                                friendlyFire = sim.friendlyFireByUnitId[unit.id];
                                bestMyFirePos = myFirePos;

                                WRITE_LOG("SC " << moveName << " " << score);
                            }


                        }
                    }
                }

                // ============

                myMoves[unit.id] = bestMove;
                unit.action = MyAction();
                applyMove(bestMove, unit.action);
                WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

                // ============

                if (nearestEnemy->canJump && onLadderOrOnGround(*sim.level, *nearestEnemy))
                    enemyPos = nearestEnemy->pos;

                {
                    renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                    renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
                }

                if (unit.weapon)
                {
                    if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                    {
                        computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                    }
                    else
                    {
                        if (unit.weapon.fireTimerMicrotics > 400)
                        {
                            unit.action.aim = (enemyPos - unit.pos).norm();
                        }
                        else if (bestFire != P(0, 0))
                        {
                            unit.action.aim = bestFire;

                            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                            normalizeAim(unit, enemyPos);
                        }
                        else
                        {
                            unit.action.aim = unit.weapon.lastAngle;
                        }
                    }

                    WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle);
                }

            }
        }
    }

}


namespace v38 {

void DistanceMap::compute(const Simulator &sim, IP ip) {
    w = sim.level->w;
    h = sim.level->h;
    distMap.clear();
    distMap.resize(w * h, -1);

    ip.limitWH(w, h);

    std::deque<IP> pts;
    pts.push_back(ip);

    distMap[ip.ind(w)] = 0;

    while (!pts.empty())
    {
        IP p = pts.front();
        pts.pop_front();

        double v = distMap[p.ind(w)] + 1;

        for (int y = p.y - 1; y <= p.y + 1; ++y)
        {
            if (sim.level->isValidY(y))
            {
                for (int x = p.x - 1; x <= p.x + 1; ++x)
                {
                    if (sim.level->isValidX(x))
                    {
                        int ind = IP(x, y).ind(w);

                        if (distMap[ind] < 0)
                        {
                            Tile t = sim.level->getTile(x, y);
                            if (t != WALL)
                            {
                                distMap[ind] = v;
                                pts.push_back(IP(x, y));
                            }
                        }
                    }
                }
            }
        }
    }
}


void NavMeshV2::compute(const MyLevel &level)
{
    w = level.w;
    h = level.h;

    nodes.resize(level.w * level.h);

    auto canMove = [&level](int x, int y) {
        return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
    };

    for (int y = 1; y < level.h - 1; ++y)
    {
        for (int x = 1; x < level.w - 1; ++x)
        {
            //if (x == 31 && y == 27)
            //    LOG("AA");

            if (canMove(x, y))
            {
                std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                bool onLadder = level.getTile(x, y) == LADDER;
                bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                bool onGround;

                bool cm_l = canMove(x - 1, y);
                bool cm_r = canMove(x + 1, y);
                bool cm_u = canMove(x, y + 1);
                bool cm_d = canMove(x, y - 1);
                bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                if (!isJumppadBody || isWallOrLadderAbove)
                {
                    // move left
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_l)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1;
                        nd0.links.push_back(l);
                    }

                    // left down
                    if (!isJumppadBody && cm_ld)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1 - level.w;
                        nd0.links.push_back(l);
                    }

                    // move right
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_r)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1;
                        nd0.links.push_back(l);
                    }

                    // right down
                    if (!isJumppadBody && cm_rd)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1 - level.w;
                        nd0.links.push_back(l);
                    }


                    // moveDown

                    if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -level.w;
                        nd0.links.push_back(l);
                    }

                }

                // move up
                if (cm_u)
                {
                    if (!isJumppadBody)
                    {
                        for (int i = 1; i <= 5; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 6;
                            l.targetMomentum = i - 1;
                            l.targetCanCancel = true;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }
                    }

                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 3;
                        l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                        l.targetCanCancel = l.targetMomentum == 0;
                        l.deltaPos = level.w;
                        node.links.push_back(l);
                    }

                    // move left up
                    if (cm_lu)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = -1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = -1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }

                    // move right up
                    if (cm_ru)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = 1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = +1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }
                }

                // stop jumping
                for (int i = 1; i <= 5; ++i)
                {
                    Node& node = cellNodes[i];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 0;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // start jumping
                onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                           || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                           || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                           || onLadder;

                if (onGround)
                {
                    Node& node = cellNodes[0];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 5;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // hit ceiling

                bool ceiling = isWallOrLadderAbove
                               || wall(level.getTile(x - 1, y + 2))
                               || wall(level.getTile(x + 1, y + 2));

                if (ceiling && !isJumppadCurrent)
                {
                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }
                }

                if (!onLadder && !onLadderUpper)
                {
                    // jump on jumppad
                    bool isJumppad = false;
                    for (int dy = -1; dy <= 2; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                            {
                                isJumppad = true;
                                break;
                            }
                        }
                    }

                    if (isJumppad)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 10;
                            l.targetCanCancel = false;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }
                }
            }
        }
    }
}

void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
{
    w = navMesh.w;
    h = navMesh.h;

    distMap.clear();
    distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
    prevNode.clear();
    prevNode.resize(w * h * MOM_N, -1);
#endif

    typedef std::pair<uint16_t , PP> NLink;
    auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

    std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

    pts.push(std::make_pair(0, pp));
    distMap[pp.ind()] = 0;

    while (!pts.empty())
    {
        PP p = pts.top().second;
        pts.pop();

        int pInd = p.ind();
        uint16_t curDist = distMap[pInd];
        const Node &node = navMesh.nodes[p.pos][p.innerInd()];

        //LOG("FROM " << p.toString(w) << " d " << curDist);

        for (const auto &link : node.links)
        {
            PP v;
            v.pos = p.pos + link.deltaPos;
            v.upMomentum = link.targetMomentum;
            v.canCancel = link.targetCanCancel;

            uint16_t newD = curDist + link.cost;
            int vInd = v.ind();
            if (distMap[vInd] > newD)
            {
                distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                prevNode[vInd] = pInd;
#endif
                pts.push(std::make_pair(newD, v));

                //LOG("TO " << v.toString(w) << " d " << newD);
            }
        }
    }
}


void applyMove(const Move &m, MyAction &action)
{
    action.velocity = m.vel;
    if (m.jump == 1)
        action.jump = true;
    else if (m.jump == -1)
        action.jumpDown = true;
}

DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
{
    PP p = strat.navMesh.makePP(u);
    int posInd = p.ind();

    auto it = strat.distMap.find(posInd);
    if (it != strat.distMap.end())
    {
        strat.lruCache.erase(it->second.lruCacheIt);
        strat.lruCache.push_front(posInd);
        it->second.lruCacheIt = strat.lruCache.begin();
        return it->second;
    }

    if (strat.lruCache.size() > MAX_DMAPS)
    {
        int last = strat.lruCache.back();
        strat.lruCache.pop_back();
        strat.distMap.erase(last);

        //LOG("REMOVE CACHE");
    }

    DistanceMapV2 &res = strat.distMap[posInd];
    res.compute(strat.navMesh, p);

    strat.lruCache.push_front(posInd);
    res.lruCacheIt = strat.lruCache.begin();
    //LOG("CASHE SIZE " << strat.lruCache.size());

    return res;
}

DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
{
    int posInd = p.ind(strat.sim.level->w);

    auto it = strat.simpleDistMap.find(posInd);
    if (it != strat.simpleDistMap.end())
        return it->second;

    DistanceMap &res = strat.simpleDistMap[posInd];
    res.compute(strat.sim, p);
    return res;
}

struct DistComputer {
    DistanceMapV2 &dmap;
    DistanceMap &sdmap;
    MyStrat &strat;

    DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
    }

    double getDist(const P &p, double simpleDistMapCoef) const {

        double d1 = dmap.getDist(strat.navMesh.makePP(p));
        double d2 = sdmap.getDist(p);
        double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

        /*static double sd1 = 0;
        static double sd2 = 0;

        if (d1 < 200 && d2 < 200)
        {
            static int c = 0;
            sd1 += d1;
            sd2 += d2;
            ++c;
            if (c % 100000 == 0)
                LOG(sd1 / sd2);
        }
        else
        {
            LOG(d1 << " " << d2);
        }*/
        return dist;
    }

    double getDistM0(const MyUnit &u, double simpleDistMapCoef)
    {
        double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
        return dist;
    }
};

double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth, int curTick, const std::vector<std::pair<P, double/*T*/>> &RLColPoints)
{
    double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

    double s0 = score;
    double lbScore = 0;
    for (const MyLootBox &l : sim.lootBoxes)
    {
        if (l.type == MyLootBoxType ::HEALTH_PACK)
        {
            double minDist = 1e9;
            Side side = Side::NONE;
            double deltaHealth = 0;
            for (const MyUnit &unit : sim.units)
            {
                DistComputer dcomp(unit, strat);

                double dist = dcomp.getDist(l.pos, sdScore);
                if (unit.isMine())
                    dist *= 1.1;

                if (dist < minDist)
                {
                    minDist = dist;
                    side = unit.side;
                    deltaHealth = 100 - curHealth[unit.id];
                }
            }

            if (side == Side::ME)
                lbScore += (deltaHealth * 0.1);
            else
                lbScore -= (deltaHealth * 0.1);
        }
    }

    if (lbScore > 30)
        lbScore = 30;

    score += lbScore;
    //WRITE_LOG("++++++++++++ " << lbScore);

    for (const MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                score += unit.health * 0.5;
            else
                score += unit.health;

            if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
            {
                score -= 1;
            }

            IP ipos = IP(unit.pos);

            DistComputer dcomp(unit, strat);

            double magazineRel = 1.0;
            if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
            {
                magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
            }

            bool reload = false;
            if (unit.weapon)
            {
                score += 100;

                reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
            }
            else
            {
                double wScore = 0;
                double wCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::WEAPON)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        wScore += 50.0 / (dist + 1.0);
                        wCount++;
                    }
                }

                if (wCount > 0)
                    score += wScore / wCount;
            }

            double hpScore = 0;
            double mineScore = 0;

            {
                double hScore = 0;
                double hCount = 0;
                double mScore = 0;
                double mCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        hScore += 50.0 / (dist + 1.0);
                        hCount++;
                    }
                    else if (l.type == MyLootBoxType ::MINE)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        mScore += 5.0 / (dist + 1.0);
                        mCount++;
                    }
                }

                if (hCount > 0)
                    hpScore = hScore / hCount;

                if (mCount > 0)
                    mineScore = mScore / mCount;
            }

            double enScore = 0;
            if (unit.weapon)
            {
                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        bool enReload = false;
                        if (en.weapon)
                            enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                        double dist = dcomp.getDistM0(en, sdScore);

                        if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                        {
                            enScore -= (10 - dist)*2;
                            // WRITE_LOG("===")
                        }


                        if (unit.health > 80 || unit.health >= en.health || hpScore == 0)
                        {
                            double keepDist = 0;


                            if ((reload /*|| magazineRel < 0.5*/))
                                keepDist = 10;

                            if (en.weapon)
                            {
                                const double D = 5;
                                if (dist < D)
                                {
                                    bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                    bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                    if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                        enScore -= (D - dist)*5;

                                    /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                    WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                    WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                    if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                        enScore -= (D - dist)*2;
                                    else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                        enScore += (D - dist)*2;
                                }
                            }

                            //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            //    keepDist = 5;
                            //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                            //    keepDist = 3;

                            //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)) << " ep " << en.pos << " eh " << en.health);
                            if (keepDist > 0)
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                            else
                                enScore += 50.0 / (dist + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                        }
                        else
                        {
                            enScore -= 50.0 / (dist + 5.0);
                        }
                    }
                }

                score += enScore;
            }

            for (const MyUnit &en : sim.units)
            {
                if (en.side == Side::EN)
                {
                    if (en.weapon && en.pos.dist(unit.pos) < 5 && dot((unit.pos - en.pos).norm(), en.weapon.lastAngle.norm()) >= 0.5)
                    {
                        if (en.weapon.fireTimerMicrotics <= 100 && (!unit.canJump || !unit.canCancel))
                        {
                            score -= 10.0;
                        }
                    }
                }
            }

            double rlScore = 0;

            if (!RLColPoints.empty())
            {
                BBox bb = unit.getBBox();
                bb.expand(getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->explRadius + 0.1);

                for (const auto &p : RLColPoints)
                {
                    if (p.second < 15 && bb.contains(p.first) && curTick <= (p.second + 1))
                    {
                        double ticksToExpl = p.second - curTick;
                        rlScore -= 2.0 * std::max(0.0, (10.0 - p.first.dist(unit.getCenter()))) / (ticksToExpl + 1.0);
                        //WRITE_LOG("RL " << ticksToExpl << " t " << p.second << " s " << rlScore << " d " << p.first.dist(unit.getCenter()));
                        //LOG(rlScore);
                    }

                }
            }



            if (enScore < 0 && unit.health < 100)
            {
                score += hpScore;
            }

            score += mineScore;

            if (curHealth[unit.id] < 100)
                score += hpScore * (100 - curHealth[unit.id]) / 200.0;

            score += rlScore;

            //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health << " s0 " << s0 << " lb " << lbScore << " rl " << rlScore);
        }
        else
        {
            score -= unit.health * 0.5;
        }
    }

    return score;
}

const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
{
    IP ipos = IP(unit.pos);

    DistComputer dcomp(unit, strat);

    const MyUnit *nearestEnemy = nullptr;
    double dmapDist = 1e9;

    for (const MyUnit &other : sim.units)
    {
        if (other.side != unit.side)
        {
            double dist = dcomp.getDistM0(other, sdScore);

            if (unit.weapon)
            {
                /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                dist += (1.0 - dt) * 1.0;

                dist += other.health * 0.1;*/
            }
            if (dist < dmapDist)
            {
                dmapDist = dist;
                nearestEnemy = &other;
            }
        }
    }

    return nearestEnemy;
}



void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
        bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
        if (!b1 && !b2)
        {
            unit.action.aim = unit.weapon.lastAngle;
            return;
        }

        if (b1 && b2)
            return;

        if (b1)
        {
            double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
        else if (b2)
        {
            double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P((ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
    }
}

void normalizeAim(MyUnit &unit, const P &enemyPos)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        P center = unit.getCenter();
        BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
        P lowestAngle = (enBox.getCorner(0) - center).norm();
        P largestAngle = lowestAngle;
        for (int i = 1; i < 4; ++i)
        {
            P angle = (enBox.getCorner(i) - center).norm();
            if (isAngleLessThan(angle, lowestAngle))
                lowestAngle = angle;

            if (isAngleLessThan(largestAngle, angle))
                largestAngle = angle;
        }

        normalizeAim(unit, lowestAngle, largestAngle);
    }
}

bool onLadderOrOnGround(const MyLevel &level, const MyUnit &unit)
{
    bool enOnLadder = level.getTileSafe(IP(unit.pos)) == LADDER || level.getTileSafe(IP(unit.pos.x, unit.pos.y + 1)) == LADDER;
    if (enOnLadder)
        return true;
    if ((unit.pos.y - std::floor(unit.pos.y)) < 0.001)
    {
        bool onGround = wallOrPlatform(level.getTileSafe(IP(unit.pos.x, unit.pos.y - 1)))
                || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y)) != WALL)
                || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y)) != WALL)
        ;

        return onGround;
    }
    return false;

}

template<int RAYS_N, bool log>
void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
{
    for (MyUnit &unit : sim.units)
    {
        if (unitId != -1 && unit.id != unitId)
            continue;

        if (unit.side == Side::ME && unit.weapon)
        {
            const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

            if (!nearestEnemy)
                return;

            P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
            P aim = (enemyPos - unit.pos).norm();
            unit.action.aim = aim;

            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

            normalizeAim(unit, enemyPos);

            //unit.action.aim = (enemyPos - bestMyFirePos).norm();
            //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
            unit.action.shoot = false;

            bool isNearby = unit.pos.dist(nearestEnemy->pos) < 1.5 || (std::abs(unit.pos.x - nearestEnemy->pos.x) < 0.5 && std::abs(unit.pos.y - nearestEnemy->pos.y) < 2.0);

            if (!nearestEnemy->canJump || !nearestEnemy->canCancel || onLadderOrOnGround(*sim.level, *nearestEnemy) || isNearby)
            {
                unit.action.shoot = true;

                double spread = unit.weapon.spread;
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.maxDist(mu.getCenter());

                                if (mu.isMine() && dist < (explRad + 1))
                                {
                                    myUnitsHits++;

                                    if (log)
                                        renderLine(mu.getCenter(), tp, 0xff0000ffu);
                                }
                                else if (mu.isEnemy() && dist < explRad * 0.7)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }
            }
        }

        if (unit.isMine())
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                unit.action.swapWeapon = true;

            //if (unit.weapon.weaponType == MyWeaponType ::ASSAULT_RIFLE /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
            //    unit.action.swapWeapon = true;

            /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
            {
                unit.action.reload = true;
            }*/
        }
    }
}

void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
{
#ifdef ENABLE_LOGGING
    DistanceMapV2 dmap = getDistMap(from, s);

    PP p = s.navMesh.makePP(to);
    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = dmap.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    renderPath(*s.sim.level, nodes);
#endif
}

void MyStrat::compute(const Simulator &_sim)
{
    double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
    //LOG(SDMAP_COEF);

    std::map<int, Move> enemyMoves;

    if (_sim.curTick > 0)
    {
        for (const MyUnit &u : _sim.units)
        {
            if (u.isEnemy())
            {
                const MyUnit *oldUnit = this->sim.getUnit(u.id);
                if (oldUnit)
                {
                    Move &m = enemyMoves[u.id];
                    m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                    if (u.pos.y > oldUnit->pos.y)
                    {
                        m.jump = 1;
                    }
                    else if (u.pos.y > oldUnit->pos.y && u.canJump)
                    {
                        m.jump = -1;
                    }
                }
            }
        }
    }

    this->sim = _sim;

    bool isLoosing = sim.score[0] <= sim.score[1];

    /*for (MyUnit &u : sim.units)
    {
        if (u.side == Side::ME)
        {
            u.action = MyAction();
        }
    }

    return;*/

    if (!navMesh.w)
    {
        navMesh.compute(*sim.level);
    }

    std::map<int, int> curHealth;
    for (MyUnit &unit : sim.units)
    {
        curHealth[unit.id] = unit.health;
    }

    std::vector<std::pair<P, double/*T*/>> RLColPoints;
    for (MyBullet &b : sim.bullets)
    {
        if (b.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
        {
            BBox bbox = b.getBBox();
            P tp = sim.level->bboxWallCollision2(bbox, b.vel);
            double t = (tp - b.pos).len() / b.params->bulletSpeed * 60;
            RLColPoints.emplace_back(tp, t);
        }
    }

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

            double bestScore = -100000;
            Move bestMove;
            P bestFire = P(0, 0);
            double friendlyFire = 0;
            P bestMyFirePos = unit.pos;

            const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

            if (nearestEnemy)
                renderPathTo(*this, unit, *nearestEnemy);

            P enemyPos = P(0, 0);
            const int TICKN = 25;
            int bulletFlyTime = 0;
            double enemyDist = 1000.0;
            int ticksToFire = 0;
            if (nearestEnemy && unit.weapon)
            {
                enemyPos = nearestEnemy->pos;
                enemyDist = enemyPos.dist(unit.pos);
                bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                ticksToFire = unit.weapon.fireTimerMicrotics / 100;
            }

            if (bulletFlyTime >= TICKN)
                bulletFlyTime = TICKN - 1;


            {

                int jumpN = 3;
                if (unit.canJump)
                    jumpN = 4;

                for (int dir = 0; dir < 3; ++dir)
                {
                    for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                    {
                        Simulator sim = _sim;
                        sim.enFireAtCenter = true;
                        sim.score[0] = 0;
                        sim.score[1] = 0;
                        sim.selfFire[0] = 0;
                        sim.selfFire[1] = 0;
                        sim.friendlyFireByUnitId.clear();

                        P myFirePos = unit.pos;
                        P fireAim = P(0, 0);

                        P p1 = unit.pos;

                        std::string moveName;
                        if (dir == 0)
                        {
                            moveName = "left ";
                        }
                        else if (dir == 1)
                        {
                            moveName = "right ";
                        }

                        if (jumpS == 0)
                        {
                            moveName += "up";
                        }
                        else if (jumpS == 1)
                        {
                            moveName += "down";
                        }
                        else if (jumpS == 3)
                        {
                            moveName += "downup";
                        }

                        Move firstMove;
                        IP ipos = IP(unit.pos);

                        double score = 0;
                        double coef = 1.0;
                        bool fired = false;
                        int health = unit.health;
                        //std::vector<Simulator> saveSim;

                        for (int tick = 0; tick < TICKN; ++tick)
                        {
                            MyUnit *simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                                break;

                            double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                            simUnit->action = MyAction();
                            for (MyUnit &su : sim.units)
                            {
                                su.action = MyAction();

                                MyAction &action = su.action;
                                action.shoot = true;
                                if (su.weapon)
                                {
                                    action.aim = su.weapon.lastAngle;

                                    if (tick >= 5 && su.isEnemy())
                                    {
                                        const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                        if (een)
                                        {
                                            action.aim = P(een->pos - su.pos).norm();
                                        }
                                    }
                                }

                                if (su.side == Side::ME && su.id != unit.id)
                                {
                                    applyMove(myMoves[su.id], action);
                                }
                                else if (tick <= 5 && su.isEnemy())
                                {
                                    Move &em = enemyMoves[su.id];
                                    applyMove(em, su.action);
                                }
                            }

                            if (dir == 0)
                            {
                                simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                            }
                            else if (dir == 1)
                            {
                                simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                            }



                            if (jumpS == 0)
                            {
                                simUnit->action.jump = true;
                            }
                            else if (jumpS == 1)
                            {
                                simUnit->action.jumpDown = true;
                            }

                            if (jumpS == 3 && tick == 0)
                            {
                                simUnit->action.jump = false;
                            }

                            if (jumpS == 2 || (jumpS == 3 && tick > 0))
                            {
                                if (dir == 0)
                                {
                                    IP ip = IP(simUnit->pos - P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                                else if (dir == 1)
                                {
                                    IP ip = IP(simUnit->pos + P(0.5, 0));

                                    if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                                    {
                                        simUnit->action.jump = true;
                                    }
                                }
                            }

                            if (tick == 0)
                            {
                                firstMove.vel = simUnit->action.velocity;
                                if (simUnit->action.jump)
                                    firstMove.jump = 1;
                                else if (simUnit->action.jumpDown)
                                    firstMove.jump = -1;
                            }

                            computeFire<10, false>(sim, *this, sdScore, {});

                            /*{
                                for (MyBullet &b : sim.bullets)
                                {
                                    BBox bb = b.getBBox();

                                    uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                    renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                    renderLine(bb.getCorner(1), bb.getCorner(3), c);

                                }
                            }*/


                            if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                            {
                                fireAim = simUnit->action.aim;
                                fired = true;
                            }

                            //saveSim.push_back(sim);
                            sim.tick();

                            {
                                for (MyUnit &u : sim.units)
                                {
                                    if (u.isEnemy())
                                    {
                                        renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                        renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                    }
                                }
                            }

                            simUnit = sim.getUnit(unit.id);
                            if (simUnit == nullptr)
                            {
                                score -= 1000;
                                break;
                            }

                            {
                                if (simUnit->health != health)
                                {
                                    uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                    renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                    WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                    health = simUnit->health;
                                }
                                else
                                {
                                    uint32_t  c = 0x000000ccu;
                                    renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                    renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                                }
                            }

                            {
                                P p2 = simUnit->pos;
                                renderLine(p1, p2, 0x000000ffu);
                                p1 = p2;
                            }

                            if (tick == bulletFlyTime && nearestEnemy)
                            {
                                MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                                if (newEnemy)
                                    enemyPos = newEnemy->pos;
                            }

                            if (tick == ticksToFire && nearestEnemy)
                            {
                                myFirePos = simUnit->pos;
                            }

                            /*IP newIpos = IP(unit.pos);

                            if (newIpos != ipos)
                            {
                                if (navMesh.get(newIpos).canWalk)
                                {
                                    ipos = newIpos;
                                }
                            }*/

                            double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth, tick, RLColPoints);
                            if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                            {
                                WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                            }

                            score += lscore * coef;

                            /*if (tick < 3)
                                coef *= 0.95;
                            else
                                coef *= 0.9;*/
                            coef *= 0.95;
                        }

                        WRITE_LOG(moveName << " " << score);

                        if (score > bestScore)
                        {
                            bestMove = firstMove;
                            bestFire = fireAim;
                            bestScore = score;
                            friendlyFire = sim.friendlyFireByUnitId[unit.id];
                            bestMyFirePos = myFirePos;

                            WRITE_LOG("SC " << moveName << " " << score);
                        }


                    }
                }
            }

            // ============

            myMoves[unit.id] = bestMove;
            unit.action = MyAction();
            applyMove(bestMove, unit.action);
            WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

            // ============

            if (nearestEnemy->canJump && onLadderOrOnGround(*sim.level, *nearestEnemy))
                enemyPos = nearestEnemy->pos;

            {
                renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                }
                else
                {
                    if (unit.weapon.fireTimerMicrotics > 400)
                    {
                        unit.action.aim = (enemyPos - unit.pos).norm();
                    }
                    else if (bestFire != P(0, 0))
                    {
                        unit.action.aim = bestFire;

                        //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                        normalizeAim(unit, enemyPos);
                    }
                    else
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                    }
                }

                WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle << " SC " << sim.score[0] << " " << sim.score[1]);
            }


            int minesPlanted = 0;
            for (MyMine &m : sim.mines)
            {
                if (m.pos.maxDist(unit.pos) < 0.4)
                    ++minesPlanted;
            }

            if (unit.mines > 0 /*&& unit.health <= 20 */ && unit.weapon && unit.weapon.fireTimerMicrotics <= 500 /*&& sim.score[0] > sim.score[1]*/)
            {
                double dy = unit.pos.y - std::floor(unit.pos.y);
                if (dy < 0.4 && wallOrPlatform(sim.level->getTile(IP(P(unit.pos.x, unit.pos.y - 1)))))
                {
                    int score = -1000;

                    for (MyUnit &u : sim.units)
                    {
                        if (u.id != unit.id)
                        {
                            double dd = u.pos.maxDist(unit.pos);

                            WRITE_LOG("MSCDD " << dd << " " << u.isMine());

                            if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 1.0))
                            {
                                score -= 1000;
                            }
                            else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - (0.4 + std::max(unit.weapon.fireTimerMicrotics/100.0*0.17, dy))))
                            {
                                if (u.health <= 50 * ((unit.mines + minesPlanted) + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0)))
                                {
                                    score += 1000;
                                    score += u.health;
                                }
                            }
                        }
                    }

                    if (score > 0 && minesPlanted > 0)
                    {
                        int curScore = 0;
                        double damage = 50 * ((minesPlanted) + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0));

                        for (MyUnit &u : sim.units)
                        {
                            {
                                double dd = u.pos.maxDist(unit.pos);

                                if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 1.0) && u.health <= damage)
                                {
                                    curScore -= 1000;
                                }
                                else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - (0.4 + std::max(unit.weapon.fireTimerMicrotics/100.0*0.17, dy))))
                                {
                                    if (u.health <= damage)
                                    {
                                        curScore += 1000;
                                        curScore += u.health;
                                    }
                                }
                            }
                        }

                        if (curScore >= score)
                        {
                            WRITE_LOG("DONT PLANT " << curScore << " " << score);
                            score = 0;
                        }
                    }

                    WRITE_LOG("MSC " << score);
                    //LOG("MSC " << score);

                    if (score > 0 && sim.score[0] + score > sim.score[1] + 10)
                    {
                        LOG("===PLM");
                        unit.action.plantMine = true;
                        unit.action.jump = false;
                        unit.action.jumpDown = false;
                        unit.action.velocity = 0;
                        unit.action.shoot = false;
                    }
                }
            }

            if (minesPlanted > 0 && unit.weapon.fireTimerMicrotics <= 300)
            {
                WRITE_LOG("MPL " << minesPlanted);
                LOG("MPL " << minesPlanted);

                int score = -1000;
                for (MyUnit &u : sim.units)
                {
                    if (u.id != unit.id)
                    {
                        double dd = u.pos.maxDist(unit.pos);

                        if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 0.2))
                        {
                            score -= 1000;
                        }
                        else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - 0.2))
                        {
                            if (u.health <= 50 * (minesPlanted + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0)))
                            {
                                score += 1000;
                                score += u.health;
                            }
                        }
                    }
                }

                WRITE_LOG("MPSC " << score);
                LOG("MPSC " << score);

                if (sim.score[0] + score > sim.score[1] + 10)
                {
                    unit.action.jump = false;
                    unit.action.jumpDown = false;
                    unit.action.velocity = 0;

                    if (unit.weapon.fireTimerMicrotics <= 100)
                    {
                        for (MyMine &m : sim.mines)
                        {
                            if (m.pos.maxDist(unit.pos) < 0.4)
                            {
                                P aim = (m.getCenter() - unit.getCenter()).norm();
                                unit.action.aim = aim;
                                unit.action.shoot = true;
                                WRITE_LOG("SHOOT");
                                LOG("SHOOT");
                                break;
                            }
                        }
                    }
                }
            }

        }
    }
}
}


namespace v39 {

void DistanceMap::compute(const Simulator &sim, IP ip) {
    w = sim.level->w;
    h = sim.level->h;
    distMap.clear();
    distMap.resize(w * h, -1);

    ip.limitWH(w, h);

    std::deque<IP> pts;
    pts.push_back(ip);

    distMap[ip.ind(w)] = 0;

    while (!pts.empty())
    {
        IP p = pts.front();
        pts.pop_front();

        double v = distMap[p.ind(w)] + 1;

        for (int y = p.y - 1; y <= p.y + 1; ++y)
        {
            if (sim.level->isValidY(y))
            {
                for (int x = p.x - 1; x <= p.x + 1; ++x)
                {
                    if (sim.level->isValidX(x))
                    {
                        int ind = IP(x, y).ind(w);

                        if (distMap[ind] < 0)
                        {
                            Tile t = sim.level->getTile(x, y);
                            if (t != WALL)
                            {
                                distMap[ind] = v;
                                pts.push_back(IP(x, y));
                            }
                        }
                    }
                }
            }
        }
    }
}


void NavMeshV2::compute(const MyLevel &level)
{
    w = level.w;
    h = level.h;

    nodes.resize(level.w * level.h);

    auto canMove = [&level](int x, int y) {
        return !wall(level.getTile(x, y)) && !wall(level.getTile(x, y + 1));
    };

    for (int y = 1; y < level.h - 1; ++y)
    {
        for (int x = 1; x < level.w - 1; ++x)
        {
            //if (x == 31 && y == 27)
            //    LOG("AA");

            if (canMove(x, y))
            {
                std::array<Node, MOM_N> &cellNodes = nodes[IP(x, y).ind(level.w)];
                bool onLadder = level.getTile(x, y) == LADDER;
                bool onLadderUpper = level.getTile(x, y + 1) == LADDER;

                bool onGround;

                bool cm_l = canMove(x - 1, y);
                bool cm_r = canMove(x + 1, y);
                bool cm_u = canMove(x, y + 1);
                bool cm_d = canMove(x, y - 1);
                bool cm_lu = cm_l && cm_u && canMove(x - 1, y + 1);
                bool cm_ru = cm_r && cm_u && canMove(x + 1, y + 1);
                bool cm_ld = cm_l && cm_d && canMove(x - 1, y - 1);
                bool cm_rd = cm_r && cm_d && canMove(x + 1, y - 1);

                bool isJumppadBelow = jumppad(level.getTile(x, y - 1));
                bool isJumppadUpperHalf = jumppad(level.getTile(x, y + 1));
                bool isJumppadCurrent = jumppad(level.getTile(x, y)) || isJumppadUpperHalf;
                bool isJumppadBody = isJumppadBelow || isJumppadCurrent;

                bool isWallOrLadderAbove = wall(level.getTile(x, y + 2)) || level.getTile(x, y + 2) == LADDER;

                if (!isJumppadBody || isWallOrLadderAbove)
                {
                    // move left
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_l)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1;
                        nd0.links.push_back(l);
                    }

                    // left down
                    if (!isJumppadBody && cm_ld)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -1 - level.w;
                        nd0.links.push_back(l);
                    }

                    // move right
                    onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                               || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                               || onLadder;

                    if ((onGround || onLadderUpper) && cm_r)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1;
                        nd0.links.push_back(l);
                    }

                    // right down
                    if (!isJumppadBody && cm_rd)
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 1 - level.w;
                        nd0.links.push_back(l);
                    }


                    // moveDown

                    if (!isJumppadBody && !wallOrJumppad(level.getTile(x, y - 1)))
                    {
                        Node &nd0 = cellNodes[0];
                        Link l;
                        l.cost = 6;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = -level.w;
                        nd0.links.push_back(l);
                    }

                }

                // move up
                if (cm_u)
                {
                    if (!isJumppadBody)
                    {
                        for (int i = 1; i <= 5; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 6;
                            l.targetMomentum = i - 1;
                            l.targetCanCancel = true;
                            l.deltaPos = level.w;
                            node.links.push_back(l);
                        }
                    }

                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 3;
                        l.targetMomentum = onLadderUpper ? 0 : (isJumppadCurrent ? 10 : i - 6);
                        l.targetCanCancel = l.targetMomentum == 0;
                        l.deltaPos = level.w;
                        node.links.push_back(l);
                    }

                    // move left up
                    if (cm_lu)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = -1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x - 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = -1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }

                    // move right up
                    if (cm_ru)
                    {
                        if (!isJumppadBody)
                        {
                            for (int i = 1; i <= 5; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = i - 1;
                                l.targetCanCancel = true;
                                l.deltaPos = 1 + level.w;
                                node.links.push_back(l);
                            }
                        }

                        if (canMove(x + 1, y + 2) && !onLadder && !onLadderUpper)
                        {
                            for (int i = 7; i < 16; ++i)
                            {
                                Node& node = cellNodes[i];

                                Link l;
                                l.cost = 6;
                                l.targetMomentum = isJumppadUpperHalf ? 10 : (isJumppadCurrent ? 9 : i - 7);
                                l.targetCanCancel = l.targetMomentum == 0;
                                l.deltaPos = +1 + 2 * level.w;
                                node.links.push_back(l);
                            }
                        }
                    }
                }

                // stop jumping
                for (int i = 1; i <= 5; ++i)
                {
                    Node& node = cellNodes[i];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 0;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // start jumping
                onGround = wallOrPlatformOrLadder(level.getTile(x, y - 1))
                           || (cm_l && wallOrPlatform(level.getTile(x - 1, y - 1)))
                           || (cm_r && wallOrPlatform(level.getTile(x + 1, y - 1)))
                           || onLadder;

                if (onGround)
                {
                    Node& node = cellNodes[0];

                    Link l;
                    l.cost = 1;
                    l.targetMomentum = 5;
                    l.targetCanCancel = true;
                    l.deltaPos = 0;
                    node.links.push_back(l);
                }

                // hit ceiling

                bool ceiling = isWallOrLadderAbove
                               || wall(level.getTile(x - 1, y + 2))
                               || wall(level.getTile(x + 1, y + 2));

                if (ceiling && !isJumppadCurrent)
                {
                    for (int i = 6; i < 16; ++i)
                    {
                        Node& node = cellNodes[i];

                        Link l;
                        l.cost = 1;
                        l.targetMomentum = 0;
                        l.targetCanCancel = true;
                        l.deltaPos = 0;
                        node.links.push_back(l);
                    }
                }

                if (!onLadder && !onLadderUpper)
                {
                    // jump on jumppad
                    bool isJumppad = false;
                    for (int dy = -1; dy <= 2; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (level.getTileSafe(IP(x + dx, y + dy)) == JUMP_PAD)
                            {
                                isJumppad = true;
                                break;
                            }
                        }
                    }

                    if (isJumppad)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            Node& node = cellNodes[i];

                            Link l;
                            l.cost = 1;
                            l.targetMomentum = 10;
                            l.targetCanCancel = false;
                            l.deltaPos = 0;
                            node.links.push_back(l);
                        }
                    }
                }
            }
        }
    }
}

void DistanceMapV2::compute(const NavMeshV2 &navMesh, const PP &pp)
{
    w = navMesh.w;
    h = navMesh.h;

    distMap.clear();
    distMap.resize(w * h * MOM_N, 65000);

#ifdef ENABLE_LOGGING
    prevNode.clear();
    prevNode.resize(w * h * MOM_N, -1);
#endif

    typedef std::pair<uint16_t , PP> NLink;
    auto cmp = [](const NLink &l1, const NLink &l2){return l1.first > l2.first;};

    std::priority_queue<NLink, std::vector<NLink>, decltype(cmp)> pts(cmp);

    pts.push(std::make_pair(0, pp));
    distMap[pp.ind()] = 0;

    while (!pts.empty())
    {
        PP p = pts.top().second;
        pts.pop();

        int pInd = p.ind();
        uint16_t curDist = distMap[pInd];
        const Node &node = navMesh.nodes[p.pos][p.innerInd()];

        //LOG("FROM " << p.toString(w) << " d " << curDist);

        for (const auto &link : node.links)
        {
            PP v;
            v.pos = p.pos + link.deltaPos;
            v.upMomentum = link.targetMomentum;
            v.canCancel = link.targetCanCancel;

            uint16_t newD = curDist + link.cost;
            int vInd = v.ind();
            if (distMap[vInd] > newD)
            {
                distMap[vInd] = newD;
#ifdef ENABLE_LOGGING
                prevNode[vInd] = pInd;
#endif
                pts.push(std::make_pair(newD, v));

                //LOG("TO " << v.toString(w) << " d " << newD);
            }
        }
    }
}


void applyMove(const Move &m, MyAction &action)
{
    action.velocity = m.vel;
    if (m.jump == 1)
        action.jump = true;
    else if (m.jump == -1)
        action.jumpDown = true;
}

DistanceMapV2 &getDistMap(const MyUnit &u, MyStrat &strat)
{
    PP p = strat.navMesh.makePP(u);
    int posInd = p.ind();

    auto it = strat.distMap.find(posInd);
    if (it != strat.distMap.end())
    {
        strat.lruCache.erase(it->second.lruCacheIt);
        strat.lruCache.push_front(posInd);
        it->second.lruCacheIt = strat.lruCache.begin();
        return it->second;
    }

    if (strat.lruCache.size() > MAX_DMAPS)
    {
        int last = strat.lruCache.back();
        strat.lruCache.pop_back();
        strat.distMap.erase(last);

        //LOG("REMOVE CACHE");
    }

    DistanceMapV2 &res = strat.distMap[posInd];
    res.compute(strat.navMesh, p);

    strat.lruCache.push_front(posInd);
    res.lruCacheIt = strat.lruCache.begin();
    //LOG("CASHE SIZE " << strat.lruCache.size());

    return res;
}

DistanceMap &getSimpleDistMap(const IP &p, MyStrat &strat)
{
    int posInd = p.ind(strat.sim.level->w);

    auto it = strat.simpleDistMap.find(posInd);
    if (it != strat.simpleDistMap.end())
        return it->second;

    DistanceMap &res = strat.simpleDistMap[posInd];
    res.compute(strat.sim, p);
    return res;
}

struct DistComputer {
    DistanceMapV2 &dmap;
    DistanceMap &sdmap;
    MyStrat &strat;

    DistComputer(const MyUnit &unit, MyStrat &strat) : dmap(getDistMap(unit, strat)), sdmap(getSimpleDistMap(unit.pos, strat)), strat(strat) {
    }

    double getDist(const P &p, double simpleDistMapCoef) const {

        double d1 = dmap.getDist(strat.navMesh.makePP(p));
        double d2 = sdmap.getDist(p);
        double dist = d1 * (1.0 - simpleDistMapCoef) + d2;

        /*static double sd1 = 0;
        static double sd2 = 0;

        if (d1 < 200 && d2 < 200)
        {
            static int c = 0;
            sd1 += d1;
            sd2 += d2;
            ++c;
            if (c % 100000 == 0)
                LOG(sd1 / sd2);
        }
        else
        {
            LOG(d1 << " " << d2);
        }*/
        return dist;
    }

    double getDistM0(const MyUnit &u, double simpleDistMapCoef)
    {
        double dist = dmap.getDistM0(strat.navMesh.makePP(u)) * (1.0 - simpleDistMapCoef) + sdmap.getDist(u.pos);
        return dist;
    }
};

double getScore(MyStrat &strat, const Simulator &sim, bool isLoosing, double sdScore, std::map<int, int> &curHealth, int curTick, const std::vector<std::pair<P, double/*T*/>> &RLColPoints)
{
    double score = (sim.score[0] - sim.score[1])/10.0 + (sim.selfFire[1] - sim.selfFire[0])/4.0;

    double s0 = score;
    double lbScore = 0;
    //std::set<int> clBoxes;
    for (const MyLootBox &l : sim.lootBoxes)
    {
        if (l.type == MyLootBoxType ::HEALTH_PACK)
        {
            double minDist = 1e9;
            Side side = Side::NONE;
            int id = -1;
            double deltaHealth = 0;
            for (const MyUnit &unit : sim.units)
            {
                DistComputer dcomp(unit, strat);

                double dist = dcomp.getDist(l.pos, sdScore);
                if (unit.isMine())
                    dist *= 1.1;

                if (dist < minDist)
                {
                    minDist = dist;
                    side = unit.side;
                    id = unit.id;
                    deltaHealth = 100 - curHealth[unit.id];
                }
            }

            //clBoxes.insert(id);
            if (side == Side::ME)
                lbScore += (deltaHealth * 0.1);
            else
                lbScore -= (deltaHealth * 0.1);
        }
    }

    if (lbScore > 30)
        lbScore = 30;

    score += lbScore;
    //WRITE_LOG("++++++++++++ " << lbScore);

    for (const MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (isLoosing && unit.weapon.lastFireTick + 500 < sim.curTick)
                score += unit.health * 0.5;
            else
                score += unit.health;

            if (unit.weapon)
                score -= unit.weapon.spread;
            else
                score -= 1;

            if (!unit.action.shoot && unit.weapon && unit.weapon.fireTimerMicrotics <= 100)
            {
                score -= 1;
            }

            IP ipos = IP(unit.pos);

            DistComputer dcomp(unit, strat);

            double magazineRel = 1.0;
            if (unit.weapon && unit.weapon.weaponType != MyWeaponType ::ROCKET_LAUNCHER)
            {
                magazineRel = (double) unit.weapon.magazine / (double) unit.weapon.params->magazineSize;
            }

            bool reload = false;
            if (unit.weapon)
            {
                score += 100;

                reload = unit.weapon.fireTimerMicrotics > unit.weapon.params->fireRateMicrotiks;
            }
            else
            {
                double wScore = 0;
                double wCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::WEAPON)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        wScore += 50.0 / (dist + 1.0);
                        wCount++;
                    }
                }

                if (wCount > 0)
                    score += wScore / wCount;
            }

            double hpScore = 0;
            double mineScore = 0;

            {
                double hScore = 0;
                double hCount = 0;
                double mScore = 0;
                double mCount = 0;

                for (const MyLootBox &l : sim.lootBoxes)
                {
                    if (l.type == MyLootBoxType ::HEALTH_PACK)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        hScore += 50.0 / (dist + 1.0);
                        hCount++;
                    }
                    else if (l.type == MyLootBoxType ::MINE)
                    {
                        double dist = dcomp.getDist(l.pos, sdScore);
                        mScore += 5.0 / (dist + 1.0);
                        mCount++;
                    }
                }

                if (hCount > 0)
                    hpScore = hScore / hCount;

                if (mCount > 0)
                    mineScore = mScore / mCount;
            }

            double enScore = 0;
            if (unit.weapon)
            {
                for (const MyUnit &en : sim.units)
                {
                    if (en.side == Side::EN)
                    {
                        bool enReload = false;
                        if (en.weapon)
                            enReload = en.weapon.fireTimerMicrotics > en.weapon.params->fireRateMicrotiks;

                        double dist = dcomp.getDistM0(en, sdScore);

                        if (reload && !enReload && en.weapon && en.weapon.magazine > 3 && dist < 10)
                        {
                            enScore -= (10 - dist)*2;
                            // WRITE_LOG("===")
                        }


                        if (unit.health > 80 || unit.health >= en.health || hpScore == 0 /*|| !clBoxes.count(unit.id)*/)
                        {
                            double keepDist = 0;


                            if ((reload /*|| magazineRel < 0.5*/))
                                keepDist = 10;

                            if (en.weapon)
                            {
                                const double D = 5;
                                if (dist < D)
                                {
                                    bool enCanKill = !enReload && en.weapon.magazine * en.weapon.params->bulletDamage >= unit.health;
                                    bool meCanKill = unit.weapon.magazine * unit.weapon.params->bulletDamage >= en.health;

                                    if (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics))
                                        enScore -= (D - dist)*5;

                                    /*WRITE_LOG("KK " << enCanKill << " " << meCanKill << " " << (enCanKill && (!meCanKill || unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics)));
                                    WRITE_LOG("KKK1 " << en.weapon.magazine << " " << en.weapon.params->bulletDamage << " " << unit.health);
                                    WRITE_LOG("KKK2 " << unit.weapon.magazine << " " << unit.weapon.params->bulletDamage << " " << en.health);*/

                                    if (unit.weapon.fireTimerMicrotics >= en.weapon.fireTimerMicrotics + 300)
                                        enScore -= (D - dist)*2;
                                    else if (unit.weapon.fireTimerMicrotics <= en.weapon.fireTimerMicrotics - 300)
                                        enScore += (D - dist)*2;
                                }
                            }

                            //if (keepDist == 0 && en.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                            //    keepDist = 5;
                            //else if (magazineRel * unit.weapon.params->fireRateMicrotiks < 500)
                            //    keepDist = 3;

                            //WRITE_LOG("KD " << keepDist << " " << dist << " " << (50.0 / (std::abs(dist - keepDist) + 5.0)) << " ep " << en.pos << " eh " << en.health);
                            if (keepDist > 0)
                                enScore += 50.0 / (std::abs(dist - keepDist) + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                            else
                                enScore += 50.0 / (dist + 5.0) * (unit.health + 20.0) / (en.health + 20.0);
                        }
                        else
                        {
                            enScore -= 50.0 / (dist + 5.0);
                        }
                    }
                }

                score += enScore;
            }

            for (const MyUnit &en : sim.units)
            {
                if (en.side == Side::EN)
                {
                    if (en.weapon && en.pos.dist(unit.pos) < 5 && dot((unit.pos - en.pos).norm(), en.weapon.lastAngle.norm()) >= 0.5)
                    {
                        if (en.weapon.fireTimerMicrotics <= 100 && (!unit.canJump || !unit.canCancel))
                        {
                            score -= 10.0;
                        }
                    }
                }
            }

            double rlScore = 0;

            if (!RLColPoints.empty())
            {
                BBox bb = unit.getBBox();
                bb.expand(getWeaponParams(MyWeaponType::ROCKET_LAUNCHER)->explRadius + 0.1);

                for (const auto &p : RLColPoints)
                {
                    if (p.second < 15 && bb.contains(p.first) && curTick <= (p.second + 1))
                    {
                        double ticksToExpl = p.second - curTick;
                        rlScore -= 2.0 * std::max(0.0, (10.0 - p.first.dist(unit.getCenter()))) / (ticksToExpl + 1.0);
                        //WRITE_LOG("RL " << ticksToExpl << " t " << p.second << " s " << rlScore << " d " << p.first.dist(unit.getCenter()));
                        //LOG(rlScore);
                    }

                }
            }



            if (enScore < 0 && unit.health < 100)
            {
                score += hpScore;
            }

            score += mineScore;

            if (curHealth[unit.id] < 100)
                score += hpScore * (100 - curHealth[unit.id]) / 200.0;

            score += rlScore;

            //WRITE_LOG("ENS " << enScore << " HPS " << hpScore << " H " << unit.health << " s0 " << s0 << " lb " << lbScore << " rl " << rlScore);
        }
        else
        {
            score -= unit.health * 0.5;

            if (unit.weapon)
                score += unit.weapon.spread * 0.5;
            else
                score += 1 * 0.5;
        }
    }

    return score;
}

const MyUnit *getNearestEnemy(Simulator &sim, MyStrat &strat, const MyUnit &unit, double sdScore)
{
    IP ipos = IP(unit.pos);

    DistComputer dcomp(unit, strat);

    const MyUnit *nearestEnemy = nullptr;
    double dmapDist = 1e9;

    for (const MyUnit &other : sim.units)
    {
        if (other.side != unit.side)
        {
            double dist = dcomp.getDistM0(other, sdScore);

            if (unit.weapon)
            {
                /*double dt = dot(unit.weapon.lastAngle, (other.pos - unit.pos).norm());
                dist += (1.0 - dt) * 1.0;

                dist += other.health * 0.1;*/
            }
            if (dist < dmapDist)
            {
                dmapDist = dist;
                nearestEnemy = &other;
            }
        }
    }

    return nearestEnemy;
}



void normalizeAim(MyUnit &unit, const P &lowestAngle, const P &largestAngle)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        bool b1 = isAngleLessThan(lowestAngle, unit.weapon.lastAngle.rotate(-unit.weapon.spread));
        bool b2 = isAngleLessThan(unit.weapon.lastAngle.rotate(unit.weapon.spread), largestAngle);
        if (!b1 && !b2)
        {
            unit.action.aim = unit.weapon.lastAngle;
            return;
        }

        if (b1 && b2)
            return;

        if (b1)
        {
            double ang = -unrotate(unit.weapon.lastAngle, lowestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P(-(ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
        else if (b2)
        {
            double ang = unrotate(unit.weapon.lastAngle, largestAngle).getAngleFast();
            //assert(ang > unit.weapon.spread);
            P newAngle = P((ang - unit.weapon.spread) * 0.5);
            unit.action.aim = unit.weapon.lastAngle.rotate(newAngle);
        }
    }
}

void normalizeAim(MyUnit &unit, const P &enemyPos)
{
    if (unit.weapon.lastAngle != P(0, 0))
    {
        P center = unit.getCenter();
        BBox enBox = BBox::fromBottomCenterAndSize(enemyPos, unit.size);
        P lowestAngle = (enBox.getCorner(0) - center).norm();
        P largestAngle = lowestAngle;
        for (int i = 1; i < 4; ++i)
        {
            P angle = (enBox.getCorner(i) - center).norm();
            if (isAngleLessThan(angle, lowestAngle))
                lowestAngle = angle;

            if (isAngleLessThan(largestAngle, angle))
                largestAngle = angle;
        }

        normalizeAim(unit, lowestAngle, largestAngle);
    }
}

bool onLadderOrOnGround(const MyLevel &level, const MyUnit &unit)
{
    bool enOnLadder = level.getTileSafe(IP(unit.pos)) == LADDER || level.getTileSafe(IP(unit.pos.x, unit.pos.y + 1)) == LADDER;
    if (enOnLadder)
        return true;
    if ((unit.pos.y - std::floor(unit.pos.y)) < 0.001)
    {
        bool onGround = wallOrPlatform(level.getTileSafe(IP(unit.pos.x, unit.pos.y - 1)))
                || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x - 1, unit.pos.y)) != WALL)
                || (wallOrPlatform(level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y - 1))) && level.getTileSafe(IP(unit.pos.x + 1, unit.pos.y)) != WALL)
        ;

        return onGround;
    }
    return false;

}

template<int RAYS_N, bool log>
void computeFire(Simulator &sim, MyStrat &strat, double sdScore, std::optional<P> enemyPosOpt, int unitId = -1)
{
    for (MyUnit &unit : sim.units)
    {
        if (unitId != -1 && unit.id != unitId)
            continue;

        if (unit.side == Side::ME && unit.weapon)
        {
            const MyUnit *nearestEnemy = getNearestEnemy(sim, strat, unit, sdScore);

            if (!nearestEnemy)
                return;

            P enemyPos = enemyPosOpt ? *enemyPosOpt : nearestEnemy->pos;
            P aim = (enemyPos - unit.pos).norm();
            unit.action.aim = aim;

            //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);

            normalizeAim(unit, enemyPos);

            //unit.action.aim = (enemyPos - bestMyFirePos).norm();
            //unit.action.aim = (nearestEnemy->pos - unit.pos).norm();
            unit.action.shoot = false;

            bool isNearby = unit.pos.dist(nearestEnemy->pos) < 1.5 || (std::abs(unit.pos.x - nearestEnemy->pos.x) < 0.5 && std::abs(unit.pos.y - nearestEnemy->pos.y) < 2.0);

            if (!nearestEnemy->canJump || !nearestEnemy->canCancel || onLadderOrOnGround(*sim.level, *nearestEnemy) || isNearby)
            {
                unit.action.shoot = true;

                double spread = unit.weapon.spread;
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    //const int RAYS_N = 10;

                    P ddir = P(spread * 2.0 / (RAYS_N - 1));
                    P ray = unit.action.aim.rotate(-spread);

                    P center = unit.getCenter();

                    MyWeaponParams *wparams = unit.weapon.params;
                    BBox missile = BBox::fromCenterAndHalfSize(center, wparams->bulletSize * 0.5);

                    int myUnitsHits = 0;
                    double enUnitsHits = 0;

                    for (int i = 0; i < RAYS_N; ++i)
                    {
                        P tp = sim.level->bboxWallCollision2(missile, ray);

                        MyUnit *directHitUnit = nullptr;
                        double directHitDist = 1e9;
                        for (MyUnit &mu : sim.units) // direct hit
                        {
                            if (mu.id != unit.id)
                            {
                                auto [isCollided, colP] = mu.getBBox().rayIntersection(center, ray);

                                if (isCollided)
                                {
                                    double d1 = tp.dist2(center);
                                    double d2 = colP.dist2(center);

                                    if (d2 < d1 && d2 < directHitDist)
                                    {
                                        directHitDist = d2;
                                        directHitUnit = &mu;
                                    }
                                }
                            }
                        }

                        if (directHitUnit)
                        {
                            if (directHitUnit->isMine())
                                myUnitsHits++;
                            else
                            {
                                BBox b2 = directHitUnit->getBBox();
                                double dst = std::max(0.0, std::sqrt(directHitDist) - 1.0);
                                double expD = dst / 10.0;
                                if (expD > 0.4)
                                    expD = 0.4;
                                b2.expand(-expD);

                                auto [isCollided, colP] = b2.rayIntersection(center, ray);
                                if (isCollided)
                                {
                                    if (dst / unit.weapon.params->bulletSpeed > 0.1)
                                    {
                                        enUnitsHits += 0.09;
                                    }
                                    else
                                    {
                                        enUnitsHits++;
                                    }
                                    //LOG("EXP " << expD);
                                }
                                else
                                    enUnitsHits += 0.09;
                            }
                        }

                        // Explosion damage
                        if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                        {
                            double explRad = wparams->explRadius;

                            for (MyUnit &mu : sim.units)
                            {
                                double dist = tp.maxDist(mu.getCenter());

                                if (mu.isMine() && dist < (explRad + 1))
                                {
                                    myUnitsHits++;

                                    if (log)
                                        renderLine(mu.getCenter(), tp, 0xff0000ffu);
                                }
                                else if (mu.isEnemy() && dist < explRad * 0.7)
                                {
                                    enUnitsHits++;
                                }
                            }
                        }

                        ray = ray.rotate(ddir);
                    }

                    if (enUnitsHits < 1.0 || (myUnitsHits > 0 && enUnitsHits < RAYS_N * 0.7))
                        unit.action.shoot = false;

                    if (log)
                        WRITE_LOG("EH " << enUnitsHits << " MH " << myUnitsHits << " F " << unit.action.shoot);
                }
            }
        }

        if (unit.isMine())
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
                unit.action.swapWeapon = true;

            //if (unit.weapon.weaponType == MyWeaponType ::ASSAULT_RIFLE /*&& nearestEnemy->weapon && nearestEnemy->weapon.weaponType != MyWeaponType::ROCKET_LAUNCHER*/)
            //    unit.action.swapWeapon = true;

            /*if (!unit.action.shoot && unit.weapon && unit.weapon.lastFireTick + 300 < sim.curTick)
            {
                unit.action.reload = true;
            }*/
        }
    }
}

void renderPathTo(MyStrat &s, const MyUnit &from, const MyUnit &to)
{
#ifdef ENABLE_LOGGING
    DistanceMapV2 dmap = getDistMap(from, s);

    PP p = s.navMesh.makePP(to);
    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = dmap.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    renderPath(*s.sim.level, nodes);
#endif
}

void MyStrat::compute(const Simulator &_sim)
{
    double SDMAP_COEF = (sim.getRandom() + 1.0) * 0.5;
    //LOG(SDMAP_COEF);

    std::map<int, Move> enemyMoves;

    if (_sim.curTick > 0)
    {
        for (const MyUnit &u : _sim.units)
        {
            if (u.isEnemy())
            {
                const MyUnit *oldUnit = this->sim.getUnit(u.id);
                if (oldUnit)
                {
                    Move &m = enemyMoves[u.id];
                    m.vel = (u.pos.x - oldUnit->pos.x) * _sim.level->properties.ticksPerSecond;

                    if (u.pos.y > oldUnit->pos.y)
                    {
                        m.jump = 1;
                    }
                    else if (u.pos.y > oldUnit->pos.y && u.canJump)
                    {
                        m.jump = -1;
                    }
                }
            }
        }
    }

    this->sim = _sim;

    bool isLoosing = sim.score[0] <= sim.score[1];

    /*for (MyUnit &u : sim.units)
    {
        if (u.side == Side::ME)
        {
            u.action = MyAction();
        }
    }

    return;*/

    if (!navMesh.w)
    {
        navMesh.compute(*sim.level);
    }

    std::map<int, int> curHealth;
    for (MyUnit &unit : sim.units)
    {
        curHealth[unit.id] = unit.health;
    }

    std::vector<std::pair<P, double/*T*/>> RLColPoints;
    for (MyBullet &b : sim.bullets)
    {
        if (b.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
        {
            BBox bbox = b.getBBox();
            P tp = sim.level->bboxWallCollision2(bbox, b.vel);
            double t = (tp - b.pos).len() / b.params->bulletSpeed * 60;
            RLColPoints.emplace_back(tp, t);
        }
    }

    std::set<int> mineShoot;

    for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            WRITE_LOG("UNIT " << unit.pos << " " << unit.id);

            double bestScore = -100000;
            double bestScore0 = -100000;
            Move bestMove;
            Move bestMove0;
            P bestFire = P(0, 0);
            double friendlyFire = 0;
            P bestMyFirePos = unit.pos;

            const MyUnit *nearestEnemy = getNearestEnemy(sim, *this, unit, 0.01);

            if (nearestEnemy)
                renderPathTo(*this, unit, *nearestEnemy);

            P enemyPos = P(0, 0);
            const int TICKN = 25;
            int bulletFlyTime = 0;
            double enemyDist = 1000.0;
            int ticksToFire = 0;
            if (nearestEnemy && unit.weapon)
            {
                enemyPos = nearestEnemy->pos;
                enemyDist = enemyPos.dist(unit.pos);
                bulletFlyTime = std::max(0.0, enemyDist - 1.5) / unit.weapon.params->bulletSpeed*60;

                ticksToFire = unit.weapon.fireTimerMicrotics / 100;
            }

            if (bulletFlyTime >= TICKN)
                bulletFlyTime = TICKN - 1;


            {

                int jumpN = 3;
                if (unit.canJump)
                    jumpN = 4;

                auto computeMove = [&](const std::string &moveName, std::function<void (MyUnit *simUnit, int tick)> applyMoveFunc){

                    Simulator sim = _sim;
                    sim.enFireAtCenter = true;
                    sim.score[0] = 0;
                    sim.score[1] = 0;
                    sim.selfFire[0] = 0;
                    sim.selfFire[1] = 0;
                    sim.friendlyFireByUnitId.clear();

                    P myFirePos = unit.pos;
                    P fireAim = P(0, 0);

                    P p1 = unit.pos;



                    Move firstMove;
                    IP ipos = IP(unit.pos);

                    double score = 0;
                    double coef = 1.0;
                    bool fired = false;
                    int health = unit.health;
                    //std::vector<Simulator> saveSim;

                    for (int tick = 0; tick < TICKN; ++tick)
                    {
                        MyUnit *simUnit = sim.getUnit(unit.id);
                        if (simUnit == nullptr)
                            break;

                        double sdScore = tick < 4 ? 0.01 : SDMAP_COEF;

                        simUnit->action = MyAction();
                        for (MyUnit &su : sim.units)
                        {
                            su.action = MyAction();

                            MyAction &action = su.action;
                            action.shoot = true;
                            if (su.weapon)
                            {
                                action.aim = su.weapon.lastAngle;

                                if (tick >= 5 && su.isEnemy())
                                {
                                    const MyUnit *een = getNearestEnemy(sim, *this, su, sdScore);
                                    if (een)
                                    {
                                        action.aim = P(een->pos - su.pos).norm();
                                    }
                                }
                            }

                            if (su.side == Side::ME && su.id != unit.id)
                            {
                                applyMove(myMoves[su.id], action);
                            }
                            else if (tick <= 5 && su.isEnemy())
                            {
                                Move &em = enemyMoves[su.id];
                                applyMove(em, su.action);
                            }

                            /*if (su.isEnemy() && su.weapon && su.weapon.fireTimerMicrotics <= 100)
                            {
                                // Check friend
                                for (const MyUnit &oth : sim.units)
                                {
                                    if (oth.id != su.id && oth.side == su.side)
                                    {
                                        P ray = (oth.pos - su.pos);
                                        double dist = ray.len();

                                        if (dist < 2)
                                        {
                                            ray /= dist;

                                            double angle = unrotate(su.action.aim, ray).getAngleFast();
                                            if (std::abs(angle) < su.weapon.spread + 0.5 / dist)
                                            {
                                                su.action.shoot = false;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (su.action.shoot && su.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                                {
                                    P col = sim.level->rayWallCollision(su.getCenter(), su.action.aim);
                                    double dd = col.dist(su.getCenter());
                                    if (dd < 3)
                                    {
                                        bool anyClose = false;
                                        for (const MyUnit &oth : sim.units)
                                        {
                                            if (oth.isMine())
                                            {
                                                if (dd > oth.pos.dist(su.pos) || oth.pos.dist(col) < 4)
                                                {
                                                    anyClose = true;
                                                }
                                            }
                                        }

                                        if (!anyClose)
                                            su.action.shoot = false;
                                    }
                                }
                            }*/
                        }

                        applyMoveFunc(simUnit, tick);

                        if (tick == 0)
                        {
                            firstMove.vel = simUnit->action.velocity;
                            if (simUnit->action.jump)
                                firstMove.jump = 1;
                            else if (simUnit->action.jumpDown)
                                firstMove.jump = -1;
                        }

                        computeFire<10, false>(sim, *this, sdScore, {});

                        /*{
                            for (MyBullet &b : sim.bullets)
                            {
                                BBox bb = b.getBBox();

                                uint32_t c = b.side == Side::ME ? 0xff0000ffu : 0x0000ffffu;
                                renderLine(bb.getCorner(0), bb.getCorner(2), c);
                                renderLine(bb.getCorner(1), bb.getCorner(3), c);

                            }
                        }*/


                        if (simUnit->weapon && !fired && simUnit->weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick && simUnit->action.shoot)
                        {
                            fireAim = simUnit->action.aim;
                            fired = true;
                        }

                        //saveSim.push_back(sim);
                        sim.tick();

                        {
                            for (MyUnit &u : sim.units)
                            {
                                if (u.isEnemy())
                                {
                                    renderLine(u.pos - P(0.1, 0), u.pos + P(0.1, 0), 0x000000ffu);
                                    renderLine(u.pos - P(0, 0.1), u.pos + P(0, 0.1), 0x000000ffu);
                                }
                            }
                        }

                        simUnit = sim.getUnit(unit.id);
                        if (simUnit == nullptr)
                        {
                            score -= 1000;
                            break;
                        }

                        {
                            if (simUnit->health != health)
                            {
                                uint32_t  c = simUnit->health < health ? 0xff0000ffu : 0x00ff00ffu;
                                renderLine(simUnit->pos - P(0.1, 0), simUnit->pos + P(0.1, 0), c);
                                renderLine(simUnit->pos - P(0, 0.1), simUnit->pos + P(0, 0.1), c);
                                WRITE_LOG("H t=" << tick << " " << health << " -> " << simUnit->health);
                                health = simUnit->health;
                            }
                            else
                            {
                                uint32_t  c = 0x000000ccu;
                                renderLine(simUnit->pos - P(0.05, 0), simUnit->pos + P(0.05, 0), c);
                                renderLine(simUnit->pos - P(0, 0.05), simUnit->pos + P(0, 0.05), c);
                            }
                        }

                        {
                            P p2 = simUnit->pos;
                            renderLine(p1, p2, 0x000000ffu);
                            p1 = p2;
                        }

                        if (tick == bulletFlyTime && nearestEnemy)
                        {
                            MyUnit *newEnemy = sim.getUnit(nearestEnemy->id);
                            if (newEnemy)
                                enemyPos = newEnemy->pos;
                        }

                        if (tick == ticksToFire && nearestEnemy)
                        {
                            myFirePos = simUnit->pos;
                        }

                        /*IP newIpos = IP(unit.pos);

                        if (newIpos != ipos)
                        {
                            if (navMesh.get(newIpos).canWalk)
                            {
                                ipos = newIpos;
                            }
                        }*/

                        double lscore = getScore(*this, sim, isLoosing, sdScore, curHealth, tick, RLColPoints);
                        if (tick == 0 || tick == 10 || tick == (TICKN - 1))
                        {
                            WRITE_LOG("LS " << tick << "| " << moveName << " " << lscore << " H " << simUnit->health);
                        }

                        score += lscore * coef;

                        if (tick == 5)
                        {
                            if (score > bestScore0)
                            {
                                bestMove0 = firstMove;
                                bestScore0 = score;
                            }
                        }

                        /*if (tick < 3)
                            coef *= 0.95;
                        else
                            coef *= 0.9;*/
                        coef *= 0.95;
                    }

                    WRITE_LOG(moveName << " " << score);

                    if (score > bestScore)
                    {
                        bestMove = firstMove;
                        bestFire = fireAim;
                        bestScore = score;
                        friendlyFire = sim.friendlyFireByUnitId[unit.id];
                        bestMyFirePos = myFirePos;

                        WRITE_LOG("SC " << moveName << " " << score);
                    }
                };

                auto applyMoveFuncByDir = [&](MyUnit *simUnit, int tick, int dir, int jumpS){
                    if (dir == 0)
                    {
                        simUnit->action.velocity = -sim.level->properties.unitMaxHorizontalSpeed;
                    }
                    else if (dir == 1)
                    {
                        simUnit->action.velocity = sim.level->properties.unitMaxHorizontalSpeed;
                    }

                    if (jumpS == 0)
                    {
                        simUnit->action.jump = true;
                    }
                    else if (jumpS == 1)
                    {
                        simUnit->action.jumpDown = true;
                    }

                    if (jumpS == 3 && tick == 0)
                    {
                        simUnit->action.jump = false;
                    }

                    if (jumpS == 2 || (jumpS == 3 && tick > 0))
                    {
                        if (dir == 0)
                        {
                            IP ip = IP(simUnit->pos - P(0.5, 0));

                            if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                            {
                                simUnit->action.jump = true;
                            }
                        }
                        else if (dir == 1)
                        {
                            IP ip = IP(simUnit->pos + P(0.5, 0));

                            if (sim.level->getTileSafe(ip) == WALL || sim.level->getTileSafe(ip.incY()) == WALL)
                            {
                                simUnit->action.jump = true;
                            }
                        }
                    }
                };

                int bestDir = -1;
                int bestJumpS = -1;

                for (int dir = 0; dir < 3; ++dir)
                {
                    for (int jumpS = 0; jumpS < jumpN; ++jumpS)
                    {
                        std::string moveName;
                        if (dir == 0)
                        {
                            moveName = "left ";
                        }
                        else if (dir == 1)
                        {
                            moveName = "right ";
                        }

                        if (jumpS == 0)
                        {
                            moveName += "up";
                        }
                        else if (jumpS == 1)
                        {
                            moveName += "down";
                        }
                        else if (jumpS == 3)
                        {
                            moveName += "downup";
                        }

                        auto applyMoveFunc = [&](MyUnit *simUnit, int tick){
                            applyMoveFuncByDir(simUnit, tick, dir, jumpS);
                        };

                        double oldBestScore = bestScore;
                        computeMove(moveName, applyMoveFunc);

                        if (bestScore > oldBestScore)
                        {
                            bestDir = dir;
                            bestJumpS = jumpS;

                            //LOG("BS " << oldBestScore << " " << bestScore << " " << dir);
                        }
                    };
                }

                if (bestMove0.vel != bestMove.vel)
                {
                    auto applyMoveFunc = [&](MyUnit *simUnit, int tick){
                        applyMoveFuncByDir(simUnit, tick, bestDir, bestJumpS);
                        if (tick <= 2)
                        {
                            //LOG("V " << simUnit->action.velocity << " " << bestMove0.vel << " " << bestDir << " " << bestJumpS << " " << bestMove0.vel << " " << bestMove.vel);
                            simUnit->action.velocity = bestMove0.vel;
                        }
                    };

                    double oldBestScore = bestScore;
                    computeMove("corrected", applyMoveFunc);
                    if (bestScore > oldBestScore)
                    {
                        //LOG("UPD SCORE " << oldBestScore << " " << bestScore);
                    }
                }
            }

            // ============

            myMoves[unit.id] = bestMove;
            unit.action = MyAction();
            applyMove(bestMove, unit.action);
            WRITE_LOG("MOVE " << unit.action.velocity << " " << unit.action.jump << " " << unit.action.jumpDown);

            // ============

            if (nearestEnemy->canJump && onLadderOrOnGround(*sim.level, *nearestEnemy))
                enemyPos = nearestEnemy->pos;

            {
                renderLine(enemyPos - P(0.5, 0.0), enemyPos + P(0.5, 0.0), 0x00ff00ffu);
                renderLine(enemyPos - P(0.0, 0.5), enemyPos + P(0.0, 0.5), 0x00ff00ffu);
            }

            if (unit.weapon)
            {
                if (unit.weapon.fireTimerMicrotics <= sim.level->properties.updatesPerTick)
                {
                    computeFire<10, true>(sim, *this, 0, enemyPos, unit.id);
                }
                else
                {
                    if (unit.weapon.fireTimerMicrotics > 400)
                    {
                        unit.action.aim = (enemyPos - unit.pos).norm();
                    }
                    else if (bestFire != P(0, 0))
                    {
                        unit.action.aim = bestFire;

                        //renderLine(unit.getCenter(), unit.getCenter() + aim * 100, 0xff00ffffu);
                        normalizeAim(unit, enemyPos);
                    }
                    else
                    {
                        unit.action.aim = unit.weapon.lastAngle;
                    }
                }

                WRITE_LOG("AIM " << unit.action.aim << " BF " << bestFire << " LA " << unit.weapon.lastAngle << " SC " << sim.score[0] << " " << sim.score[1]);
            }


            int minesPlanted = 0;
            for (MyMine &m : sim.mines)
            {
                if (m.pos.maxDist(unit.pos) < 0.4)
                    ++minesPlanted;
            }

            if (unit.mines > 0 /*&& unit.health <= 20 */ && unit.weapon && unit.weapon.fireTimerMicrotics <= 500 /*&& sim.score[0] > sim.score[1]*/)
            {
                double dy = unit.pos.y - std::floor(unit.pos.y);
                if (dy < 0.4 && wallOrPlatform(sim.level->getTile(IP(P(unit.pos.x, unit.pos.y - 1)))))
                {
                    int score = -1000;

                    for (MyUnit &u : sim.units)
                    {
                        if (u.id != unit.id)
                        {
                            double dd = u.pos.maxDist(unit.pos);

                            WRITE_LOG("MSCDD " << dd << " " << u.isMine());

                            if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 1.0))
                            {
                                score -= 1000;
                            }
                            else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - (0.4 + std::max(unit.weapon.fireTimerMicrotics/100.0*0.17, dy))))
                            {
                                if (u.health <= 50 * ((unit.mines + minesPlanted) + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0)))
                                {
                                    score += 1000;
                                    score += u.health;
                                }
                            }
                        }
                    }

                    if (score > 0 && minesPlanted > 0)
                    {
                        int curScore = 0;
                        double damage = 50 * ((minesPlanted) + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0));

                        for (MyUnit &u : sim.units)
                        {
                            {
                                double dd = u.pos.maxDist(unit.pos);

                                if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 1.0) && u.health <= damage)
                                {
                                    curScore -= 1000;
                                }
                                else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - (0.4 + std::max(unit.weapon.fireTimerMicrotics/100.0*0.17, dy))))
                                {
                                    if (u.health <= damage)
                                    {
                                        curScore += 1000;
                                        curScore += u.health;
                                    }
                                }
                            }
                        }

                        if (curScore >= score)
                        {
                            WRITE_LOG("DONT PLANT " << curScore << " " << score);
                            score = 0;
                        }
                    }

                    WRITE_LOG("MSC " << score);
                    //LOG("MSC " << score);

                    if (score > 0 && sim.score[0] + score > sim.score[1] + 10)
                    {
                        LOG("===PLM");
                        unit.action.plantMine = true;
                        unit.action.jump = false;
                        unit.action.jumpDown = false;
                        unit.action.shoot = false;

                        if (unit.weapon.fireTimerMicrotics <= 200)
                            unit.action.velocity = 0;
                    }
                }
            }

            if (minesPlanted > 0 && unit.weapon.fireTimerMicrotics <= 300)
            {
                WRITE_LOG("MPL " << minesPlanted);
                LOG("MPL " << minesPlanted);

                int score = -1000;
                for (MyUnit &u : sim.units)
                {
                    if (u.id != unit.id)
                    {
                        double dd = u.pos.maxDist(unit.pos);

                        if (u.isMine() && dd < (sim.level->properties.mineExplosionParams.radius + sim.level->properties.mineSize.x + 0.2))
                        {
                            score -= 1000;
                        }
                        else if (u.isEnemy() && dd < (sim.level->properties.mineExplosionParams.radius - 0.2))
                        {
                            if (u.health <= 50 * (minesPlanted + (unit.weapon.weaponType == MyWeaponType::ROCKET_LAUNCHER ? 1 : 0)))
                            {
                                score += 1000;
                                score += u.health;
                            }
                        }
                    }
                }

                WRITE_LOG("MPSC " << score);
                LOG("MPSC " << score);

                if (sim.score[0] + score > sim.score[1] + 10)
                {
                    unit.action.jump = false;
                    unit.action.jumpDown = false;
                    unit.action.velocity = 0;

                    if (unit.weapon.fireTimerMicrotics <= 100)
                    {
                        for (MyMine &m : sim.mines)
                        {
                            if (m.pos.maxDist(unit.pos) < 0.4)
                            {
                                P aim = (m.getCenter() - unit.getCenter()).norm();
                                unit.action.aim = aim;
                                unit.action.shoot = true;
                                mineShoot.insert(unit.id);
                                WRITE_LOG("SHOOT");
                                LOG("SHOOT " << sim.curTick);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Safety check
    /*for (MyUnit &unit : sim.units)
    {
        if (unit.side == Side::ME)
        {
            if (unit.weapon.weaponType == MyWeaponType ::ROCKET_LAUNCHER && !mineShoot.count(unit.id) && unit.weapon.fireTimerMicrotics <= 100 && unit.action.shoot)
            {
                auto computeSim = [&](int dir, bool fire, Simulator sim){
                    sim.myRLFireDir = dir;

                    for (int tick = 0; tick < 7; ++tick)
                    {
                        for (MyUnit &u : sim.units)
                        {
                            if (u.id == unit.id)
                            {
                                u.action.shoot = fire;
                            }
                            else if (u.isMine())
                            {
                                applyMove(myMoves[u.id], u.action);
                            }
                            else
                            {
                                Move &em = enemyMoves[u.id];
                                applyMove(em, u.action);
                                u.action.shoot = true;

                                const MyUnit *een = getNearestEnemy(sim, *this, u, 0.5);
                                if (een)
                                {
                                    u.action.aim = P(een->pos - u.pos).norm();
                                }
                            }
                        }
                        sim.tick();
                    }

                    double res = sim.score[0] - sim.score[1];

                    for (MyUnit &u : sim.units)
                    {
                        if (u.isMine())
                            res += u.health;
                        else
                            res -= u.health;
                    }

                    return res;
                };

                double scNoShoot = computeSim(-1, false, sim);
                double scShoot1 = computeSim(0, true, sim);

                if (scNoShoot > scShoot1)
                {
                    LOG("DONT SHOOT1 " << sim.curTick << " " << scNoShoot << " " << scShoot1);
                    unit.action.shoot = false;
                }

                if (unit.action.shoot)
                {
                    double scShoot2 = computeSim(1, true, sim);

                    if (scNoShoot > scShoot2)
                    {
                        LOG("DONT SHOOT2 " << sim.curTick << " " << scNoShoot << " " << scShoot2);
                        unit.action.shoot = false;
                    }
                }
            }
        }
    }*/
}
}
