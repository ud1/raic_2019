#ifndef RAIC2019_MYSTRAT_HPP
#define RAIC2019_MYSTRAT_HPP

#include "Simulator.hpp"
#include <map>
#include <list>

struct MyStratBase {
    Simulator sim;
    virtual void compute(const Simulator &_sim) = 0;
    virtual std::string getVersion() = 0;
};

struct DistanceMap {
    std::vector<double> distMap;
    int w, h;

    void compute(const Simulator &sim, IP ip);

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

struct Move {
    double vel = 0;
    int jump = 0;
};

struct Link
{
    short deltaPos;
    short targetMomentum;
    uint16_t cost;
    bool targetCanCancel;
};

struct Node {
    std::vector<Link> links;
};

const int MOM_N = 16;
// canCancel, up momentum [0 - 5]
// !canCancel, up momentum [1 - 10]

struct PP {
    int pos;
    short upMomentum;
    bool canCancel;

    int ind() const {
        assert((canCancel && upMomentum >= 0 && upMomentum <= 5) || (!canCancel && upMomentum >= 1 && upMomentum <= 10));
        return pos * 16 + innerInd();
    }

    int innerInd() const
    {
        return upMomentum + (canCancel ? 0 : 5);
    }

    std::string toString(int w) const {
        int x = pos % w;
        int y = pos / w;
        return "(" + std::to_string(x) + " " + std::to_string(y) + " m " + std::to_string(upMomentum) + " cc " + std::to_string(canCancel) + ")";
    };
};

struct NavMeshV2 {
    std::vector<std::array<Node, MOM_N>> nodes;
    int w = 0, h = 0;
    void compute(const MyLevel &level);

    PP makePP(const MyUnit &u) const {
        PP result;
        IP pos = IP(u.pos);
        pos.limitWH(w, h);

        result.pos = IP(u.pos).ind(w);
        result.canCancel = u.canCancel;
        int upY = (int) (u.pos.y + u.maxTimeMicrotics / (u.canCancel ? 600.0 : 300.0));
        result.upMomentum = upY - pos.y;
        if (result.canCancel && result.upMomentum > 5)
            result.upMomentum = 5;
        if (!result.canCancel && result.upMomentum > 10)
            result.upMomentum = 10;
        if (!result.canCancel && result.upMomentum == 0)
            result.canCancel = true;
        return result;
    }

    PP makePP(const P &p) const {
        PP result;
        result.canCancel = true;
        result.upMomentum = 0;
        result.pos = IP(p).ind(w);
        return result;
    }
};

const int DMAP_SIZE = 40*30*16*2;
const int MAX_DMAPS = 100 * 1024*1024 / DMAP_SIZE;
struct DistanceMapV2 {
    std::vector<uint16_t > distMap;

#ifdef ENABLE_LOGGING
    std::vector<int> prevNode;
#endif

    int w, h;
    std::list<int>::iterator lruCacheIt;

    void compute(const NavMeshV2 &navMesh, const PP &pp);

    double getDist(PP p) const
    {
        double res = distMap[p.ind()] * 0.16;
        return res;
    }

    double getDistM0(PP p) const
    {
        double res = getDist(p);
        if (res > 1e6)
        {
            p.upMomentum = 0;
            p.canCancel = true;
            res = getDist(p);
        }
        return res;
    }
};

struct MyUnitState
{
    P firePos;
    int enemyId;
};

struct MyStrat : MyStratBase {
    NavMeshV2 navMesh;
    std::map<int, DistanceMapV2> distMap;
    std::map<int, DistanceMap> simpleDistMap;
    std::list<int> lruCache;

    std::map<int/* unit id*/, Move> myMoves;
    std::map<int/* unit id*/, MyUnitState> myUnitState;

    virtual void compute(const Simulator &_sim);

    virtual std::string getVersion() {
        return "v40";
    }
};





#endif

