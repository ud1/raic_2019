#include <random>

#include "d_game.hpp"
#include <fstream>
#include <set>
#include "gameUtils.hpp"

MyLevel createLevel(const std::string &levelFile, bool singleMicro)
{
    MyLevel res;

    res.properties.maxTickCount = 3600;
    res.properties.teamSize = 1;
    res.properties.ticksPerSecond = singleMicro ? 6000 : 60;
    res.properties.updatesPerTick = singleMicro ? 1 : 100;
    res.properties.lootBoxSize.x = 0.5;
    res.properties.lootBoxSize.y = 0.5;
    res.properties.unitSize.x = 0.9;
    res.properties.unitSize.y = 1.8;
    res.properties.unitMaxHorizontalSpeed = 10;
    res.properties.unitFallSpeed = 10;
    res.properties.unitJumpTime = 0.55;
    res.properties.unitJumpSpeed = 10;
    res.properties.jumpPadJumpTime = 0.525;
    res.properties.jumpPadJumpSpeed = 20;
    res.properties.unitMaxHealth = 100;
    res.properties.healthPackHealth = 50;
    {
        WeaponParams &wp = res.properties.weaponParams[PISTOL];
        wp.magazineSize = 8;
        wp.fireRate = 0.4;
        wp.reloadTime = 1;
        wp.minSpread = 0.05;
        wp.maxSpread = 0.5;
        wp.recoil = 0.5;
        wp.aimSpeed = 1;
        wp.bullet.speed = 50;
        wp.bullet.size = 0.2;
        wp.bullet.damage = 20;
    }
    {
        WeaponParams &wp = res.properties.weaponParams[ASSAULT_RIFLE];
        wp.magazineSize = 20;
        wp.fireRate = 0.1;
        wp.reloadTime = 1;
        wp.minSpread = 0.1;
        wp.maxSpread = 0.5;
        wp.recoil = 0.2;
        wp.aimSpeed = 1.9;
        wp.bullet.speed = 50;
        wp.bullet.size = 0.2;
        wp.bullet.damage = 5;
    }
    {
        WeaponParams &wp = res.properties.weaponParams[ROCKET_LAUNCHER];
        wp.magazineSize = 1;
        wp.fireRate = 1;
        wp.reloadTime = 1;
        wp.minSpread = 0.1;
        wp.maxSpread = 0.5;
        wp.recoil = 1;
        wp.aimSpeed = 1;
        wp.bullet.speed = 20;
        wp.bullet.size = 0.4;
        wp.bullet.damage = 30;
        wp.explosion = std::make_shared<ExplosionParams>();
        wp.explosion->radius = 3;
        wp.explosion->damage = 50;
    }

    res.properties.mineSize.x = 0.5;
    res.properties.mineSize.y = 0.5;
    res.properties.mineExplosionParams.radius = 3;
    res.properties.mineExplosionParams.damage = 50;
    res.properties.minePrepareTime = 1;
    res.properties.mineTriggerTime = 0.5;
    res.properties.mineTriggerRadius = 1;
    res.properties.killScore = 1000;

    initializeLevelFromProps(res);

    std::ifstream file(levelFile);
    std::string str;
    std::vector<std::string> lines;
    while (std::getline(file, str)) {
        if (str.length() > 0)
            lines.push_back(str);
    }

    res.w = lines[0].length();
    res.h = lines.size();

    res.tiles.resize(res.w * res.h);

    for (int y = 0; y < res.h; ++y)
    {
        std::string &l = lines[res.h - y - 1];
        if (l.length() != res.w)
            throw std::runtime_error("Invalid row length");

        for (int x = 0; x < res.w; ++x)
        {
            char ch = l[x];

            int ind = y * res.w + x;

            Tile tile;
            if (ch == '#')
            {
                tile = WALL;
            }
            else if (ch == '.')
            {
                tile = EMPTY;
            }
            else if (ch == 'H')
            {
                tile = LADDER;
            }
            else if (ch == 'T')
            {
                tile = JUMP_PAD;
            }
            else if (ch == '^')
            {
                tile = PLATFORM;
            }
            else if (ch == 'P')
            {
                tile = EMPTY;
                res.spawns.push_back(P(x, y));
            }
            else
            {
                throw std::runtime_error(std::string("Invalid symbol ") + ch + " line " + std::to_string(y));
            }

            res.tiles[ind] = tile;
        }
    }

    return res;
}

void saveLevel(const Simulator &sim) {
    int t = time(0);

    const MyLevel &l = *sim.level;

    std::set<int> unitPos;
    for (const MyUnit &u : sim.units)
    {
        IP ip = IP(u.pos);
        unitPos.insert(ip.ind(l.w));
    }
    std::string fileName = "level_" + std::to_string(t);

    LOG("SAVE TO "<< fileName);

    std::ofstream file(fileName);

    for (int y = l.h; y --> 0;)
    {
        for (int x = 0; x < l.w; ++x)
        {
            Tile t = l.getTile(x, y);
            int ind = IP(x, y).ind(l.w);

            if (unitPos.count(ind))
            {
                file.write("P", 1);
            }
            else if (t == EMPTY)
            {
                file.write(".", 1);
            }
            else if (t == WALL)
            {
                file.write("#", 1);
            }
            else if (t == LADDER)
            {
                file.write("H", 1);
            }
            else if (t == JUMP_PAD)
            {
                file.write("T", 1);
            }
            else if (t == PLATFORM)
            {
                file.write("^", 1);
            }
        }

        file.write("\n", 1);
    }

    file.close();
}

Simulator createSim(const MyLevel *level)
{
    Simulator sim;
    sim.level = level;
    sim.score[0] = 0;
    sim.score[1] = 0;

    for (int i = 0; i < 2 * level->properties.teamSize; ++i)
    {
        MyUnit unit;
        unit.pos = level->spawns[i % 2] + P(0.5, 0);
        if (i >= 2)
        {
            if (unit.pos.x < level->w / 2.0)
                unit.pos.x += 1;
            else
                unit.pos.x -= 1;
        }
        unit.size = P(level->properties.unitSize.x, level->properties.unitSize.y);
        unit.maxTimeMicrotics = 0;
        unit.playerId = i;
        unit.id = i;
        unit.health = level->properties.unitMaxHealth;
        unit.mines = 0;
        unit.side = (i % 2) == 0 ? Side::ME : Side::EN;
        unit.weapon.weaponType = MyWeaponType ::NONE;
        unit.canCancel = false;
        unit.canJump = false;
        unit.walkedRight = true;
        unit.stand = true;
        unit.onGround = false;
        unit.onLadder = false;

        sim.units.push_back(unit);
    }
    return sim;
}

Side mirrorSide(Side side) {
    if (side == Side::ME)
        return Side::EN;
    else if (side == Side::EN)
        return Side::ME;

    return Side::NONE;
}

Simulator mirrorSim(const Simulator &sim)
{
    Simulator res = sim;

    for (MyUnit &u : res.units)
    {
        u.side = mirrorSide(u.side);
    }

    for (MyBullet &b : res.bullets)
    {
        b.side = mirrorSide(b.side);
    }

    for (MyMine &m : res.mines)
    {
        m.side = mirrorSide(m.side);
    }

    std::swap(res.score[0], res.score[1]);

    return res;
}

bool isSolid(Tile tile) {
    return tile == WALL || tile == PLATFORM;
}

void generateLootboxes(Simulator &sim)
{
    std::vector<P> places;

    int wm1 = sim.level->w - 1;
    for (int y = 0; y < sim.level->h - 1; ++y)
    {
        for (int x = 0; x < sim.level->w / 2; ++x)
        {
            if (isSolid(sim.level->getTile(x, y)) && isSolid(sim.level->getTile(wm1 - x, y)))
            {
                if (sim.level->getTile(x, y + 1) == EMPTY && sim.level->getTile(wm1 - x, y + 1) == EMPTY)
                {
                    places.emplace_back(x + 0.5, y + 1);
                }
            }
        }
    }

    std::shuffle(places.begin(), places.end(), sim.rnd);

    if (places.size() < 3)
        throw std::runtime_error("Invalid map");

    for (int i = 0; i < 3; ++i)
    //int i = 2;
    //int i = 1;
    //if (i != 2)
    {
        MyLootBox l;
        l.type = MyLootBoxType::WEAPON;
        l.weaponType = (MyWeaponType) i;
        //l.weaponType = MyWeaponType::ROCKET_LAUNCHER;
        l.size = P(sim.level->properties.lootBoxSize.x, sim.level->properties.lootBoxSize.y);
        l.pos = places[i];

        sim.lootBoxes.push_back(l);

        l.pos.x = sim.level->w - l.pos.x;
        sim.lootBoxes.push_back(l);
    }

    std::uniform_real_distribution<double> dis = std::uniform_real_distribution<double>(0.0, 1.0);
    for (int i = 3; i < places.size(); ++i)
    {
        double r = dis(sim.rnd);

        MyLootBox l;
        l.type = MyLootBoxType::NONE;

        if (r < 0.025)
        {
            l.type = MyLootBoxType::HEALTH_PACK;
            l.health = sim.level->properties.healthPackHealth;
        }
        else if (r < 0.05 + 0.025)
        {
            l.type = MyLootBoxType::MINE;
        }

        if (l.type != MyLootBoxType::NONE)
        {
            l.size = P(sim.level->properties.lootBoxSize.x, sim.level->properties.lootBoxSize.y);
            l.pos = places[i];

            sim.lootBoxes.push_back(l);

            l.pos.x = sim.level->w - l.pos.x;
            sim.lootBoxes.push_back(l);
        }
    }
}

uint32_t rgbF(float r, float g, float b)
{
    unsigned ur = r * 255;
    unsigned ug = g * 255;
    unsigned ub = b * 255;
    return (ur << 24) | (ug << 16) | (ub << 8) | 0xFF;
}


void sendNewTick(TcpClient &client, uint32_t tick)
{
    Obj obj;
    obj.type = "tick";
    obj.props["num"] = tick;
    client.sendObj(obj);
}

uint32_t POS_COLOR = 0x4444ffffu;
uint32_t POS_COLOR_NITRO = 0x0000ffffu;
uint32_t NEG_COLOR = 0xff4444ffu;
uint32_t NEG_COLOR_NITRO = 0xff0000ffu;

void sendMap(TcpClient &tcpClient, const MyLevel &level) {
    {
        Obj obj;
        obj.type = "fieldSize";
        obj.props["w"] = level.w;
        obj.props["h"] = level.h;

        tcpClient.sendObj(obj);
    }

    {
        Obj obj;
        obj.type = "static";

        SObj walls;
        SObj platforms;
        SObj ladders;
        SObj jpRects;
        walls["type"] = "rects";
        walls["c"] = 0x662222ffu;
        walls["dw"] = 0.0;

        platforms["type"] = "rects";
        platforms["c"] = 0xaaaa00ffu;
        platforms["dw"] = 0.0;

        ladders["type"] = "rects";
        ladders["c"] = 0x008888ffu;
        ladders["dw"] = 0.3;

        jpRects["type"] = "rects";
        jpRects["c"] = 0x00dd00ffu;
        jpRects["dw"] = 0.01;

        int wi = 0;
        int pi = 0;
        int li = 0;
        int ji = 0;
        for (int y = 0; y < level.h; ++y)
        {
            for (int x = 0; x < level.w; ++x)
            {
                Tile tile = level.getTile(x, y);
                if (tile == WALL)
                {
                    std::ostringstream oss;
                    oss << "p" << wi++;
                    walls[oss.str()] = P(x + 0.5, y + 0.5);
                }
                else if (tile == PLATFORM)
                {
                    std::ostringstream oss;
                    oss << "p" << pi++;
                    platforms[oss.str()] = P(x + 0.5, y + 0.5);
                }
                else if (tile == LADDER)
                {
                    std::ostringstream oss;
                    oss << "p" << li++;
                    ladders[oss.str()] = P(x + 0.5, y + 0.5);
                }
                else if (tile == JUMP_PAD)
                {
                    SObj jumpPad;
                    jumpPad["type"] = "poly";
                    jumpPad["c"] = 0x00ff00ffu;
                    jumpPad["p1"] = P(x, y + 1);
                    jumpPad["p2"] = P(x + 1, y + 1);
                    jumpPad["p3"] = P(x + 0.5, y);

                    std::ostringstream oss;
                    oss << "p" << ji++;
                    jpRects[oss.str()] = P(x + 0.5, y + 0.5);

                    obj.subObjs[std::string("jp") + std::to_string(ji)] = jumpPad;
                }
            }
        }
        obj.subObjs["walls"] = walls;
        obj.subObjs["platforms"] = platforms;
        obj.subObjs["ladders"] = ladders;
        obj.subObjs["jpRects"] = jpRects;




        tcpClient.sendObj(obj);
    }
}

void sendObjects(TcpClient &tcpClient, const Simulator &sim, int side)
{
    sendNewTick(tcpClient, 1);

    Side mySide = side == 0 ? Side::EN : Side::ME;

    {
        Obj obj;
        obj.type = "!score";
        obj.props["L"] = (double) sim.score[0];
        obj.props["R"] = (double) sim.score[1];
        tcpClient.sendObj(obj);
    }

    for (const MyMine &m : sim.mines)
    {
        Obj obj;
        obj.type = "mine_" + std::to_string(m.pos.x) + "_" + std::to_string(m.pos.y);
        obj.props["state"] = getMineStateName(m.state);
        obj.props["center"] = m.getCenter();
        obj.props["microticks"] = (double) m.timerMicroticks;

        obj.subObjs["body"]["c"] = 0x000000ffu;
        double range = 0;
        if (m.state == PREPARING)
        {
            obj.subObjs["body"]["c"] = 0xaaaa00ffu;
            obj.subObjs["range"]["c"] = 0xaaaa0022u;
            range = m.triggerRadius;
        }
        else if (m.state == TRIGGERED)
        {
            obj.subObjs["body"]["c"] = 0xff0000ffu;
            obj.subObjs["range"]["c"] = 0xff000088u;
            range = sim.level->properties.mineExplosionParams.radius;
        }
        else if (m.state == IDLE)
        {
            obj.subObjs["range"]["c"] = 0xff440044u;
            range = m.triggerRadius;
        }
        obj.subObjs["body"]["type"] = "line";
        obj.subObjs["body"]["p1"] = m.pos + P(-m.size.x*0.5, 0);
        obj.subObjs["body"]["p2"] = m.pos + P(+m.size.x*0.5, 0);
        obj.subObjs["body"]["p3"] = m.pos + P(+m.size.x*0.5, +m.size.y);
        obj.subObjs["body"]["p4"] = m.pos + P(-m.size.x*0.5, +m.size.y);
        obj.subObjs["body"]["p5"] = m.pos + P(-m.size.x*0.5, 0);

        obj.subObjs["range"]["type"] = "poly";
        BBox mb = m.getBBox();
        mb.expand(range);
        for (int i = 0; i < 4; ++i)
        {
            obj.subObjs["range"]["p" + std::to_string(i + 1)] = mb.getCorner(i);
        }
        tcpClient.sendObj(obj);
    }

    int i = 0;
    for (const MyBullet &b : sim.bullets)
    {
        Obj obj;
        obj.type = "bullet_" + std::to_string(i);
        obj.props["pos"] = b.pos;
        obj.props["vel"] = b.vel;

        obj.subObjs["body"]["type"] = "poly";
        obj.subObjs["body"]["c"] = 0xff00ffffu;

        double sz = b.params->bulletSize * 0.5;
        obj.subObjs["body"]["p1"] = b.pos + P(-sz, -sz);
        obj.subObjs["body"]["p2"] = b.pos + P(sz, -sz);
        obj.subObjs["body"]["p3"] = b.pos + P(sz, sz);
        obj.subObjs["body"]["p4"] = b.pos + P(-sz, sz);

        BBox bulBbox = b.getBBox();
        P tp = sim.level->bboxWallCollision2(bulBbox, b.vel);
        obj.subObjs["traj"]["type"] = "line";
        obj.subObjs["traj"]["c"] = 0x00000066u;
        obj.subObjs["traj"]["p1"] = b.pos;
        obj.subObjs["traj"]["p2"] = tp;

        double l = tp.dist(b.pos);
        P velPerTick = b.vel / sim.level->properties.ticksPerSecond;
        double ticks = l / velPerTick.len();
        for (int i = 1; i <= ticks; ++i)
        {
            P mp = b.pos + velPerTick * i;
            SObj &sobj = obj.subObjs["mp" + std::to_string(i)];
            sobj["type"] = "line";
            sobj["c"] = 0x00000066u;
            sobj["p1"] = mp + b.vel.norm().conj() * 0.05;
            sobj["p2"] = mp + b.vel.norm().conjR() * 0.05;
        }

        obj.subObjs["endP"]["type"] = "line";
        obj.subObjs["endP"]["c"] = 0x00000066u;
        obj.subObjs["endP"]["p1"] = tp + b.vel.norm().conj() * 0.1;
        obj.subObjs["endP"]["p2"] = tp + b.vel.norm().conjR() * 0.1;

        tcpClient.sendObj(obj);

        ++i;
    }

    for (const MyLootBox &l : sim.lootBoxes)
    {
        std::string pos = std::to_string(l.pos.x) + "_" + std::to_string(l.pos.y);
        Obj obj;
        if (l.type == MyLootBoxType::HEALTH_PACK)
        {
            obj.type = "health_pack_" + pos;
            obj.subObjs["body"]["c"] = 0xff0000ffu;
        }
        else if (l.type == MyLootBoxType::WEAPON)
        {
            if (l.weaponType == MyWeaponType ::PISTOL)
                obj.type = "pistol_" + pos;
            else if (l.weaponType == MyWeaponType ::ASSAULT_RIFLE)
                obj.type = "assault_rifle" + pos;
            else if (l.weaponType == MyWeaponType ::ROCKET_LAUNCHER)
                obj.type = "rocket_launcher" + pos;
            obj.subObjs["body"]["c"] = 0x00ff00ffu;
        }
        else if (l.type == MyLootBoxType::MINE)
        {
            obj.type = "mineLoot_" + pos;
            obj.subObjs["body"]["c"] = 0x0000ffffu;
        }

        obj.subObjs["body"]["type"] = "poly";
        obj.subObjs["body"]["p1"] = l.pos + P(-l.size.x*0.5, 0);
        obj.subObjs["body"]["p2"] = l.pos + P(+l.size.x*0.5, 0);
        obj.subObjs["body"]["p3"] = l.pos + P(+l.size.x*0.5, +l.size.y);
        obj.subObjs["body"]["p4"] = l.pos + P(-l.size.x*0.5, +l.size.y);
        tcpClient.sendObj(obj);
    }

    for (const MyUnit &u : sim.units)
    {
        Obj obj;
        obj.type = std::string("Unit") + (u.side == mySide ? "My" : "En");
        obj.subObjs["body"]["type"] = "line";
        obj.subObjs["body"]["c"] = u.side == mySide ? 0xff0000ffu : 0x0000ffffu;
        obj.subObjs["body"]["p1"] = u.pos + P(-u.size.x*0.5, 0);
        obj.subObjs["body"]["p2"] = u.pos + P(u.size.x*0.5, 0);
        obj.subObjs["body"]["p3"] = u.pos + P(u.size.x*0.5, u.size.y);
        obj.subObjs["body"]["p4"] = u.pos + P(-u.size.x*0.5, u.size.y);
        obj.subObjs["body"]["p5"] = u.pos + P(-u.size.x*0.5, 0);

        obj.subObjs["health"]["type"] = "poly";
        obj.subObjs["health"]["c"] = 0x00ff00aau;
        obj.subObjs["health"]["p1"] = u.pos;
        obj.subObjs["health"]["p2"] = u.pos + P(0.2, 0.0);
        obj.subObjs["health"]["p3"] = u.pos + P(0.2, u.size.y * u.health / sim.level->properties.unitMaxHealth);
        obj.subObjs["health"]["p4"] = u.pos + P(0.0, u.size.y * u.health / sim.level->properties.unitMaxHealth);

        if (u.weapon)
        {
            double t = (double) u.weapon.fireTimerMicrotics / u.weapon.params->fireRateMicrotiks;
            obj.subObjs["reload"]["type"] = "poly";

            if (t > 1)
                obj.subObjs["reload"]["c"] = 0xff000088u;
            else
                obj.subObjs["reload"]["c"] = 0xff0000aau;
            obj.subObjs["reload"]["p1"] = u.pos + P(0.2, 0.0);
            obj.subObjs["reload"]["p2"] = u.pos + P(0.4, 0.0);
            obj.subObjs["reload"]["p3"] = u.pos + P(0.4, u.size.y * t);
            obj.subObjs["reload"]["p4"] = u.pos + P(0.2, u.size.y * t);

            obj.subObjs["magazine"]["type"] = "poly";
            obj.subObjs["magazine"]["c"] = 0x0000ffaau;
            obj.subObjs["magazine"]["p1"] = u.pos;
            obj.subObjs["magazine"]["p2"] = u.pos + P(-0.2, 0.0);
            obj.subObjs["magazine"]["p3"] = u.pos + P(-0.2, u.size.y * u.weapon.magazine / u.weapon.params->magazineSize);
            obj.subObjs["magazine"]["p4"] = u.pos + P(-0.0, u.size.y * u.weapon.magazine / u.weapon.params->magazineSize);

            obj.subObjs["mines"]["type"] = "poly";
            obj.subObjs["mines"]["c"] = 0x004444aau;
            obj.subObjs["mines"]["p1"] = u.pos + P(-0.2, 0.0);
            obj.subObjs["mines"]["p2"] = u.pos + P(-0.4, 0.0);
            obj.subObjs["mines"]["p3"] = u.pos + P(-0.4, u.size.y * u.mines / 3.0);
            obj.subObjs["mines"]["p4"] = u.pos + P(-0.2, u.size.y * u.mines / 3.0);
        }

        obj.props["pos"] = u.pos;
        obj.props["center"] = u.pos + P(0, u.size.y * 0.5);
        obj.props["maxTime"] = (uint32_t) u.maxTimeMicrotics;
        obj.props["!health"] = (double) u.health;
        obj.props["!mines"] = (double) u.mines;

        obj.props["canCancel"] = u.canCancel ? "true" : "false";
        obj.props["canJump"] = u.canJump ? "true" : "false";
        obj.props["walkedRight"] = u.walkedRight ? "true" : "false";
        obj.props["stand"] = u.stand ? "true" : "false";
        obj.props["onGround"] = u.onGround ? "true" : "false";
        obj.props["onLadder"] = u.onLadder ? "true" : "false";

        if (u.weapon)
        {
            obj.props["w_type"] = getWeaponName(u.weapon.weaponType);
            obj.props["w_magazine"] = (uint32_t) u.weapon.magazine;
            obj.props["w_wasShooting"] = u.weapon.wasShooting ? "true" : "false";
            obj.props["w_spread"] = u.weapon.spread;
            obj.props["w_fireTimer"] = (double) u.weapon.fireTimerMicrotics;
            obj.props["w_lastAngle"] = u.weapon.lastAngle;
            obj.props["w_lastFireTick"] = (double) u.weapon.lastFireTick;
        }

        tcpClient.sendObj(obj);
    }

    for (const MyUnit &u : sim.units)
    {
        if (u.weapon && u.weapon.lastAngle != P(0, 0))
        {
            Obj obj;
            obj.type = std::string("Weapon") + std::to_string(u.id);
            obj.subObjs["aim"]["type"] = "line";
            obj.subObjs["aim"]["c"] = u.side == mySide ? 0xff0000ffu : 0x0000ffffu;
            obj.subObjs["aim"]["p1"] = u.getCenter();
            obj.subObjs["aim"]["p2"] = u.getCenter() + u.weapon.lastAngle * 100;

            obj.subObjs["spread"]["type"] = "poly";
            obj.subObjs["spread"]["c"] = u.side == mySide ? 0x88000022u : 0x00008822u;
            obj.subObjs["spread"]["p1"] = u.getCenter();
            obj.subObjs["spread"]["p2"] = u.getCenter() + u.weapon.lastAngle.rotate(-u.weapon.spread) * 100;
            obj.subObjs["spread"]["p3"] = u.getCenter() + u.weapon.lastAngle.rotate(u.weapon.spread) * 100;
            obj.subObjs["spread"]["p4"] = u.getCenter();

            tcpClient.sendObj(obj);
        }
    }
}

void sendActions(TcpClient &tcpClient, const Simulator &sim, int side)
{
    for (const MyUnit &u : sim.units)
    {
        if (u.weapon)
        {
            Obj obj;
            obj.type = "new aim";
            obj.subObjs["aim"]["type"] = "line";
            obj.subObjs["aim"]["c"] = 0xffff0088u;
            obj.subObjs["aim"]["p1"] = u.getCenter();
            obj.subObjs["aim"]["p2"] = u.getCenter() + u.action.aim * 100;
            tcpClient.sendObj(obj);
        }
    }
}