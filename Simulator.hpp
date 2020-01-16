#ifndef RAIC2019_SIMULATOR_HPP
#define RAIC2019_SIMULATOR_HPP

#include <vector>
#include <cassert>
#include "model/Game.hpp"
#include "myutils.hpp"
#include <random>
#include <map>

struct MyLevel {
    std::vector<Tile> tiles;
    std::vector<P> spawns;
    Properties properties;
    uint32_t w, h;
    double microtickT;
    int unitJumpTimeMicrotics;
    int jumpPadJumpTimeMicrotics;
    double unitW05;
    double unitH05;

    inline Tile getTile(int x, int y) const {
        assert(x >= 0 && x < (int) w);
        assert(y >= 0 && y < (int) h);

        return tiles[y * w + x];
    }

    inline Tile getTile(IP p) const {
        return getTile(p.x, p.y);
    }

    inline Tile getTileSafe(IP p) const {
        if (isValid(p))
            return getTile(p.x, p.y);
        return TILE_NONE;
    }

    int toMicroticks(double t) const {
        return t / microtickT + 0.5;
    }

    bool isValidX(int x) const {
        return x >= 0 && x < (int) w;
    }

    bool isValidY(int y) const {
        return y >= 0 && y < (int) h;
    }

    bool isValid(IP p) const {
        return p.x >= 0 && p.x < (int) w && p.y >= 0 && p.y < (int) h;
    }

    P rayWallCollision(const P &p, const P &ray) const;
    P bboxWallCollision(const BBox &p, const P &ray) const; // В этой версии баг!
    P bboxWallCollision2(const BBox &p, const P &ray) const;
};

enum class Side
{
    NONE = -1, ME, EN
};

struct MyAction
{
    P aim = P(1, 0);
    P rawAim = P(0, 0);
    double velocity = 0;
    bool jump = false;
    bool jumpDown = false;
    bool shoot = false;
    bool reload = false;
    bool swapWeapon = false;
    bool plantMine = false;
};

enum class MyWeaponType
{
    NONE = -1,
    PISTOL = 0,
    ASSAULT_RIFLE = 1,
    ROCKET_LAUNCHER = 2
};

inline std::string getWeaponName(MyWeaponType t)
{
    switch (t)
    {
        case MyWeaponType::NONE:
            return "NONE";
        case MyWeaponType::PISTOL:
            return "PISTOL";
        case MyWeaponType::ASSAULT_RIFLE:
            return "ASSAULT_RIFLE";
        case MyWeaponType::ROCKET_LAUNCHER:
            return "ROCKET_LAUNCHER";
    }

    return "";
}

struct MyWeaponParams {
    int magazineSize;
    int fireRateMicrotiks;
    int reloadTimeMicrotiks;
    double minSpread;
    double maxSpread;
    double recoil;
    double aimSpeed;

    double bulletSpeed;
    double bulletSize;
    int bulletDamage;

    double explRadius;
    int explDamage;
};

extern MyWeaponParams WEAPON_PARAMS[3];
inline MyWeaponParams *getWeaponParams(MyWeaponType weaponType)
{
    return &WEAPON_PARAMS[(int) weaponType];
}

struct MyWeapon
{
    MyWeaponType weaponType = MyWeaponType::NONE;
    MyWeaponParams *params;

    int magazine;
    bool wasShooting;
    double spread;
    int fireTimerMicrotics;
    P lastAngle;
    int lastFireTick = -1;

    explicit operator bool() const
    {
        return weaponType != MyWeaponType ::NONE;
    }
};

struct MyUnit {
    P pos;
    P _oldPos, _vel;
    P size;

    int maxTimeMicrotics;

    int playerId;
    int id;
    int health;
    int mines;
    Side side;

    MyWeapon weapon;

    bool canCancel;
    bool canJump;
    bool walkedRight;
    bool stand;
    bool onGround;
    bool onLadder;

    MyAction action;

    BBox getBBox() const {
        BBox res;
        res.minP = pos - P(size.x * 0.5, 0);
        res.maxP = pos + P(size.x * 0.5, size.y);
        return res;
    }

    P getCenter() const {
        return pos + P(0, size.y * 0.5);
    }

    bool isMine() const {
        return side == Side::ME;
    }

    bool isEnemy() const {
        return side == Side::EN;
    }
};

enum class MyLootBoxType {
    NONE = -1, HEALTH_PACK, WEAPON, MINE
};



struct MyLootBox {
    P pos, size;
    MyLootBoxType type;
    int health;
    MyWeaponType weaponType;

    BBox getBBox() const {
        BBox res;
        res.minP = pos - P(size.x * 0.5, 0);
        res.maxP = pos + P(size.x * 0.5, size.y);
        return res;
    }
};

struct MyBullet {
    int unitId;
    Side side;
    MyWeaponType weaponType;
    P pos, vel;
    double size05;
    MyWeaponParams *params;

    BBox getBBox() const {
        BBox res;
        res.minP = pos - P(size05, size05);
        res.maxP = pos + P(size05, size05);
        return res;
    }

    BBox getExplosionBBox() const {
        BBox res;
        res.minP = pos - P(params->explRadius, params->explRadius);
        res.maxP = pos + P(params->explRadius, params->explRadius);
        return res;
    }
};

struct MyMine {
    P pos, size;
    double triggerRadius;
    int timerMicroticks;
    Side side;
    MineState state;

    BBox getBBox() const {
        BBox res;
        res.minP = pos - P(size.x * 0.5, 0);
        res.maxP = pos + P(size.x * 0.5, size.y);
        return res;
    }

    P getCenter() const {
        return pos + P(0, size.y * 0.5);
    }
};

inline std::string getMineStateName(MineState state)
{
    switch (state)
    {
        case PREPARING:
            return "Preparing";
        case IDLE:
            return "Idle";
        case TRIGGERED:
            return "Triggered";
        case EXPLODED:
            return "Exploded";
    }

    return "";
}

struct Simulator {
    std::minstd_rand rnd;
    std::uniform_real_distribution<double> dis = std::uniform_real_distribution<double>(-1.0, 1.0);
    bool enFireAtCenter = false;
    int myRLFireDir = -1;
    bool log = false;

    double getRandom()
    {
        return dis(rnd);
    }

    const MyLevel *level;
    int curTick = -1;

    std::vector<MyUnit> units;
    std::vector<MyBullet> bullets;
    std::vector<MyMine> mines;
    std::vector<MyLootBox> lootBoxes;

    MyUnit *getUnit(int id) {
        for (MyUnit &m : units)
        {
            if (m.id == id)
                return &m;
        }

        return nullptr;
    }

    int score[2] = {};
    double selfFire[2] = {};
    std::map<int, int> friendlyFireByUnitId;

    void tickNoOpt();
    void tick();
    void microtick(int subtickInd, int microtickCount);
};


inline bool wallOrPlatformOrLadder(Tile tile)
{
    return tile == WALL || tile == PLATFORM || tile == LADDER;
}

inline bool wallOrPlatform(Tile tile)
{
    return tile == WALL || tile == PLATFORM;
}

inline bool wall(Tile tile)
{
    return tile == WALL;
}

inline bool wallOrLadder(Tile tile)
{
    return tile == WALL || tile == LADDER;
}

inline bool wallOrPlatformOrLadderOrJumpPad(Tile tile)
{
    return tile == WALL || tile == PLATFORM || tile == LADDER || tile == JUMP_PAD;
}

inline bool wallOrJumppad(Tile tile)
{
    return tile == WALL || tile == JUMP_PAD;
}

inline bool jumppad(Tile tile)
{
    return tile == JUMP_PAD;
}

inline bool emptyOrPlatformOrLadder(Tile tile)
{
    return tile == EMPTY || tile == PLATFORM || tile == LADDER;
}

#endif
