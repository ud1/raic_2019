#ifndef OLD_VERSIONS_HPP
#define OLD_VERSIONS_HPP

#include "MyStrat.hpp"

namespace emptyStrat {
    struct MyStrat : MyStratBase {
        void compute(const Simulator &_sim) override;

        std::string getVersion() override {
            return "empty";
        }
    };
}

namespace quickStartGuy {
    struct MyStrat : MyStratBase {
        void compute(const Simulator &_sim) override;

        std::string getVersion() override {
            return "qsg";
        }
    };
}

namespace v1 {
    struct MyStrat : MyStratBase {
        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v1";
        }
    };
}

namespace v2 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);

        std::string getVersion() override {
            return "v2";
        }
    };
}

namespace v3 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);

        std::string getVersion() override {
            return "v3";
        }
    };
}

namespace v4 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v4";
        }
    };
}

namespace v5 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v5";
        }
    };
}

namespace v6 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v6";
        }
    };
}

namespace v8 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v7";
        }
    };
}

namespace v9 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v9";
        }
    };
}

namespace v10 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v10";
        }
    };
}

namespace v11 {
    struct DistanceMap {
        std::vector<double> distMap;
        int w, h;

        void compute(const Simulator &sim, P start);

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v11";
        }
    };
}

namespace v12 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v12";
        }
    };

}

namespace v13 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v13";
        }
    };

}

namespace v16 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v16";
        }
    };

}

namespace v17 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v17";
        }
    };

}

namespace v18 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v18";
        }
    };

}

namespace v20 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v20";
        }
    };

}

namespace v21 {

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

    struct MyStrat : MyStratBase {
        std::map<int, DistanceMap> distMap;
        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);
        std::string getVersion() override {
            return "v21";
        }
    };

}

namespace v22 {

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

    struct MyStrat : MyStratBase {
        NavMeshV2 navMesh;
        std::map<int, DistanceMapV2> distMap;
        std::map<int, DistanceMap> simpleDistMap;
        std::list<int> lruCache;

        std::map<int/* unit id*/, Move> myMoves;

        virtual void compute(const Simulator &_sim);

        virtual std::string getVersion() {
            return "v22";
        }
    };

}

namespace v25 {

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
            return "v25";
        }
    };

}

namespace v26 {

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
            return "v26";
        }
    };

}

namespace v27 {

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
            return "v27";
        }
    };

}

// v28 == v30
namespace v28 {
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
            return "v28";
        }
    };

}

namespace v36 {
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
            return "v36";
        }
    };
}

namespace v38 {

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
            return "v38";
        }
    };

}

namespace v39 {

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
            return "v39";
        }
    };

}

#endif
