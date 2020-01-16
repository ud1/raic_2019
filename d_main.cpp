#include <iostream>

#include "Simulator.hpp"
#include "d_tcpclient.hpp"
#include <set>
#include "d_game.hpp"
#include <stdexcept>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace std::chrono_literals;

#include "MyStrategy.hpp"

bool repeater = false;
bool render = false || repeater;
int SEED = 1145775;
int gameStart = 0;
int gameCount = 500;
int teamSize = 2;
bool disableSim = repeater && false;

int LEVEL_IND = 1;

const char * const LEVELS[] = {
        "../levels/levels",
        "../levels/levels2",
};

TcpClient client;

enum class Mode {
    NORMAL, SAVE_TO_FILE, READ_FROM_FILE
};

Mode mode = Mode::NORMAL;



void writeLog(const std::string &str)
{
    if (render)
    {
        Obj obj;
        obj.type = "log";
        obj.props["text"] = str;
        client.sendObj(obj);
    }
}

void renderLine(const P &p1, const P &p2, uint32_t color)
{
    if (render)
    {
        static long i = 0;
        Obj obj;
        obj.type = "line_" + std::to_string(i++);

        obj.subObjs["line"]["type"] = "line";
        obj.subObjs["line"]["c"] = color;
        obj.subObjs["line"]["p1"] = p1;
        obj.subObjs["line"]["p2"] = p2;

        client.sendObj(obj);
    }
}


void sendNewTick(TcpClient &client, uint32_t tick);

#include "Debug.hpp"
#include "MyStrategy.hpp"
#include "TcpStream.hpp"
#include "model/PlayerMessageGame.hpp"
#include "model/ServerMessageGame.hpp"
#include "gameUtils.hpp"
#include "d_oldversions.hpp"

struct TcpInputStreamWrapper : public InputStream {
    TcpInputStreamWrapper(const std::shared_ptr<InputStream> &stream) : stream(stream) {
        file.open("data" + std::to_string(time(0)) + ".bin", std::ios_base::binary);
    }

    std::shared_ptr<InputStream> stream;
    std::ofstream file;

    virtual void readBytes(char *buffer, size_t byteCount)
    {
        stream->readBytes(buffer, byteCount);
        file.write(buffer, byteCount);
        file.flush();
    }
};

struct NullOutputStream : public OutputStream {
    virtual void writeBytes(const char *buffer, size_t byteCount) {}
    virtual void flush() {}
};

struct FileInputStream : public InputStream {
    std::ifstream file;

    FileInputStream() {
        file.open("data.bin", std::ios_base::binary);
    }
    virtual void readBytes(char *buffer, size_t byteCount)
    {
        file.read(buffer, byteCount);
    }
};


class Runner {
public:
    Runner(const std::string &host, int port, const std::string &token) {

        if (mode == Mode::NORMAL)
        {
            std::shared_ptr<TcpStream> tcpStream(new TcpStream(host, port));
            inputStream = getInputStream(tcpStream);
            outputStream = getOutputStream(tcpStream);
        }
        else if (mode == Mode::SAVE_TO_FILE)
        {
            std::shared_ptr<TcpStream> tcpStream(new TcpStream(host, port));
            inputStream = std::shared_ptr<InputStream>(new TcpInputStreamWrapper(getInputStream(tcpStream)));
            outputStream = getOutputStream(tcpStream);
        }
        else if (mode == Mode::READ_FROM_FILE)
        {
            inputStream = std::shared_ptr<InputStream>(new FileInputStream());
            outputStream = std::shared_ptr<OutputStream>(new NullOutputStream());
        }
        outputStream->write(token);
        outputStream->flush();
    }
    void run() {
        MyStrategy myStrategy;
        Debug debug(outputStream);

        MyLevel level;

        bool initialized = false;


        while (true) {
            auto message = ServerMessageGame::readFrom(*inputStream);
            const auto& playerView = message.playerView;
            if (!playerView) {
                break;
            }

            level = convertLevel(playerView->game);
            Simulator sim = convertSimulator(&level, playerView->game, playerView->myId);

            if (!initialized)
            {
                initialized = true;
                if (render)
                    sendMap(client, level);

                if (mode == Mode::SAVE_TO_FILE)
                {
                    saveLevel(sim);
                }
            }
            if (render)
                sendObjects(client, sim, 1);

            std::unordered_map<int, UnitAction> actions;
            for (const Unit &unit : playerView->game.units) {
                if (unit.playerId == playerView->myId) {
                    if (!disableSim)
                    {
                        actions.emplace(std::make_pair(
                                unit.id,
                                myStrategy.getAction(unit, playerView->game, debug)));
                    }
                    else
                    {
                        UnitAction result;
                        result.velocity = 0;
                        result.jump = false;
                        result.jumpDown = false;
                        result.aim.x = 0;
                        result.aim.y = 0;
                        result.shoot = false;
                        result.swapWeapon = false;
                        result.plantMine = false;
                        result.reload = false;

                        actions.emplace(std::make_pair(unit.id, result));
                    }
                }
            }
            PlayerMessageGame::ActionMessage(actions).writeTo(*outputStream);
            outputStream->flush();

            if (render)
                sendActions(client, myStrategy.strat.sim, 1);
        }
    }

private:
    std::shared_ptr<InputStream> inputStream;
    std::shared_ptr<OutputStream> outputStream;
};


int mainRun() {
    std::string host = "127.0.0.1";
    int port = 31001;
    std::string token = "0000000000000000";

    if (render)
        client.run();

    Runner(host, port, token).run();
    return 0;
}

void testLadder()
{
    MyLevel level = createLevel("../levels/levelLadder.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 3150; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = level.properties.unitMaxHorizontalSpeed;
        //a.velocity = 0;
        a.jumpDown = false;
        a.jump = false;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();
    }

    P p = sim.units[0].pos;
    if (p.dist(P(7.75, 1.350000001)) > 1e-9)
        throw std::runtime_error("Ladder test error");
}

void testLadder2()
{
    MyLevel level = createLevel("../levels/levelLadder2.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 9999; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = level.properties.unitMaxHorizontalSpeed;
        //a.velocity = 0;
        a.jumpDown = false;
        a.jump = true;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();
    }

    P p = sim.units[0].pos;
    if (p.dist(P(19.1649999999988303, 4.33333333433318835)) > 1e-9)
        throw std::runtime_error("Ladder2 test error");

}

void testLadder3()
{
    MyLevel level = createLevel("../levels/levelLadder3.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 9999; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = 0;
        a.jumpDown = false;
        a.jump = true;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();
    }

    P p = sim.units[0].pos;
    if (p.dist(P(2.5, 6.66166666766708282)) > 1e-9)
        throw std::runtime_error("Ladder3 test error");

    // P(2.5, 6.66166666766708282)
}

void testPlatform()
{
    MyLevel level = createLevel("../levels/levelPlatform.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 9999; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = 0;
        a.jumpDown = false;
        a.jump = true;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();
    }

    P p = sim.units[0].pos;
    if (p.dist(P(2.5, 13.6533333343339045)) > 1e-9)
        throw std::runtime_error("Platform test error");

    // P(2.5, 13.6533333343339045)
}

void testPlatform2()
{
    MyLevel level = createLevel("../levels/levelPlatform.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 9999; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = 0;
        a.jumpDown = sim.curTick == 8500;
        a.jump = sim.curTick < 8100;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();

        if (tick == 8200)
        {
            P p = sim.units[0].pos;
            if (p.dist(P(2.5, 10.3200000010000501)) > 1e-9)
                throw std::runtime_error("Platform test error");
        }

        if (tick == 9200)
        {
            P p = sim.units[0].pos;
            if (p.dist(P(2.5, 8.83166666766648412)) > 1e-9)
                throw std::runtime_error("Platform test error");
        }
    }

    P p = sim.units[0].pos;
    if (p.dist(P(2.5, 8.00000000100000008)) > 1e-9)
        throw std::runtime_error("Platform2 test error");

    // P(2.5, 8.00000000100000008)
}

void testJump()
{
    MyLevel level = createLevel("../levels/levelJump.txt", true);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 10000; ++tick)
    {
        sim.curTick = tick;

        MyAction &a = sim.units[0].action;
        a.velocity = level.properties.unitMaxHorizontalSpeed;
        //a.velocity = 0;
        a.jumpDown = false;
        a.jump = true;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;

        sim.tick();
    }

    P p = sim.units[0].pos;
    if (p.dist(P(16.41, 3.060000001)) > 1e-9)
        throw std::runtime_error("Jump test error");
}

struct Results {
    double t = 0;
    double sumSpread = 0;
    double spreadCount = 0;
    int totalWin = 0;
    int inRoundWin = 0;
    int roundsWin = 0;
};

void mainRunner()
{
    if (render)
        client.run();

    std::vector<MyLevel> levels;
    {
        std::vector<std::string> fileNames;
        for (auto it = fs::directory_iterator(LEVELS[LEVEL_IND]); it != fs::directory_iterator(); ++it)
        {
            std::string fileName = it->path().string();
            fileNames.push_back(fileName);
        }

        std::sort(fileNames.begin(), fileNames.end());

        for (const std::string &fileName : fileNames)
        {
            levels.push_back(createLevel(fileName, false));
        }
    }

    Results res1, res2;
    double tsim = 0;

    int totalTick = 0;

    for (int i = gameStart; i < gameStart + gameCount; ++i)
    {
        if (i % 5 == 0)
        {
            res1.inRoundWin = res2.inRoundWin = 0;
        }

        for (int side = 0; side < 2; ++side)
        {

            MyLevel &level = levels[i % levels.size()];
            level.properties.teamSize = teamSize;
            if (render)
                sendMap(client, level);

            Simulator sim = createSim(&level);
            sim.rnd.seed(i + SEED);
            generateLootboxes(sim);

            /*
             * qsg / v2 199| 49 335	366	120373	t1: 71.853 t2: 23649.1	| 0 40 | SIM 272.432
             * v1 / v2 199| 150 181 195 252819 t1: 64128.3 t2: 42914.1 | 9 22 | SIM 468.78
             * v1 / v2 199| 140 187	423	251842	t1: 60865.6 t2: 41054.1	| 4 23 | SIM 3079.17
             *
             * bugfix
             * qsg / v2 199| 53 331	246	120248	t1: 81.229 t2: 29941.4	| 0 40 | SIM 1768.84
             *          499| 100 879	176	288620	t1: 188.543 t2: 69996.1	| 0 100 | SIM 4200.38
             * v1 / v2 138 183	471	150369	t1: 50773.3 t2: 43589.2	| 7 20 | SIM 2347.04
             *
             * qsg / v4 500| 107 885	350	287533	t1: 204.539 t2: 72299.5	| 0 100 | SIM 4414.11
             * v3 / v4 500| 405 470	285	327185	t1: 88674.1 t2: 90618	| 23 46 | SIM 5033.38
             *
             * qsg / v5 499| 73 925	240	294211	t1: 195.401 t2: 94145.5	| 0 100 | SIM 4140.75
             * v4 / v5 499| 413 485	436	360265	t1: 92991.6 t2: 152361	| 33 46 | SIM 5295.68
             *
             * qsg / v6 499| 68 926	240	297542	t1: 210.592 t2: 111262	| 0 100 | SIM 4410.4
             * v4 / v6 499| 395 491	456	377008	t1: 99062 t2: 183136	| 29 57 | SIM 5773.69
             *
             * qsg / v6 499| 59 931	142	285934	t1: 188.078 t2: 97287.2	| 0 100 | SIM 4331.57
             * qsg / v8 499| 47 945	142	280250	t1: 198.984 t2: 111164	| 0 100 | SIM 4286.11
             * v3 / v6 499| 372 517	284	415624	t1: 104304 t2: 202273	| 26 60 | SIM 6087.66
             * v4 / v8 499| 372 517	284	415624	t1: 105152 t2: 204733	| 26 60 | SIM 6125.82
             * v4 / v6 499| 424 464	221	385133	t1: 92227.4 t2: 170889	| 43 43 | SIM 5629.23
             * v4 / v8 499| 430 465	284	394529	t1: 100076 t2: 192514	| 41 47 | SIM 5837.03
             * v6 / v8 499| 413 494	681	586295	t1: 254318 t2: 279944	| 23 50 | SIM 8336.33
             *
             * qsg / v9 499| 25 975	[39]	142	274950	t1: 198.687 t2: 110607	| 0 100 | SIM 4327.9
             * v8 / v9 245| 139 272	[1.95683]	3599	713131	t1: 262564 t2: 381876	| 8 38 | SIM 10386.9
             *
             * qsg / v10 499| 17 979	[57.5882]	142	277209	t1: 193.397 t2: 94156	| 0 100 | SIM 4272.75
             * v9 / v10 RL! 201| 143 214	[1.4965]	250	299348	t1: 61841.7 t2: 69034.1	| 7 27 | SIM 3650.78
             * v9 / v10 499| 321 404	[1.25857]	227	2106089	t1: 1.03373e+06 t2: 971773	| 30 53 | SIM 31178.1
             *
             * qsg / v11 499| 7 990	[141.429]	142	292362	t1: 200.033 t2: 100236	| 0 100 | SIM 4335.4
             * v9 / v11 377| 225 330	[1.46667]	466	1600266	t1: 720310 t2: 685140	| 17 46 | SIM 21561.2
             * v10 / v11 207| 137 149	[1.08759]	3599	986783	t1: 384684 t2: 434464	| 10 13 | SIM 14561.6
             *
             * i != 2
             * qsg / v11 503| 33 967	[29.303]	310	348853	t1: 276.679 t2: 111165	| 0 100 | SIM 5690.42
             * qsg / v12 503| 24 975	[40.625]	348	326969	t1: 338.75 t2: 389689	| 0 100 | SIM 5444.32
             *
             * i == 2
             * qsg / v12 503| 4 989	[247.25]	165	245177	t1: 229.975 t2: 236090	| 0 100 | SIM 3452.83
             *
             * qsg / v12 288| 9 558	[62]	272	157252	t1: 156.373 t2: 186277	| 0 57 | SIM 2588.93
             *
             * v11 / v12 503| 295 678	[2.29831]	285	1132790	t1: 474968 t2: 667736	| 4 85 | SIM 17697
             *
             * v12 / v13 203 TS1| 166 231	[1.39157]	1170	417674	t1: 239600 t2: 327941	| 10 26 | SIM 7063.85
             *
             * qsg / v14 523 TS1| 13 979	[75.3077]	312	307119	t1: 264.788 t2: 288975	| 0 100 | SIM 4798.36
             * v11 / v14 222 TS1| 94 294	[3.12766]	277	460318	t1: 210559 t2: 653775	| 3 36 | SIM 7115.83
             * v13 / v14 92 TS2| 39 98	[2.51282]	1116	146078	t1: 424754 t2: 789617	| 0 11 | SIM 4364.75
             * v13 / v14 254 TS1| 188 261	[1.3883]	3599	419135	t1: 365730 t2: 623822	| 9 31 | SIM 7201.25
             *
             * v13 / v15 188 TS1| 150 214	[1.42667]	162	349132	t1: 304126 t2: 493348	| 10 22 | SIM 6128.53
             * v13 / v15 177 TS1| 142 205   [1.44366]   670 357107  t1: 356600 t2: 571756   | 7 22 | SIM 6461.33
             *
             * qsg / v11 500 TS1| 25 969	[38.76]	189	301104	t1: 194.129 t2: 84242	| 0 100 | SIM 4360.72
             *
             * qsg / v16 500 TS1| 17 979	[57.5882]	227	289615	t1: 211.31 t2: 242700	| 0 100 | SIM 4388.88
             * v11 / v16 144 TS1| 67 215	[3.20896]	468	299643	t1: 121741 t2: 380125	| 0 28 | SIM 4560.03
             * v15 / v16 194 TS1| 159 206	[1.2956]	1039	362027	t1: 502216 t2: 506507	| 7 23 | SIM 6152.27
             *
             * v16 / v17 145 TS1| 122 149	[1.22131]	328	277062	t1: 398110 t2: 388312	| 5 12 | SIM 4705.16
             *
             * qsg / v18 500 TS1| 10 988	[98.8]	308	304034	t1: 326.29 t2: 319150	| 0 100 | SIM 4876.8
             * v11 / v18 370 TS1| 144 576	[4]	723	607942	t1: 240792 t2: 876744	| 0 73 | SIM 8779.23
             * v17 / v18 220 TS1| 185 251	[1.35676]	1428	264074	t1: 356080 t2: 491145	| 10 25 | SIM 4712.07
             *
             * qsg / v20 499 TS1| 11 988	[89.8182]	705	316189	t1: 272.441 t2: 245990	| 0 100 | SIM 4975.38
             * v18 / v20 244 TS1| 224 250	[1.11607]	1368	542837	t1: 604812 t2: 610627	| 18 21 | SIM 8596.07
             * v18 / v20 113 TS2| 103 122	[1.18447]	843	169570	t1: 667594 t2: 697560	| 4 11 | SIM 4881.48
             *
             * v18 / pre_v20 499 TS2| 449 546        [1.21604]       1039    762236  t1: 3.04446e+06 t2: 3.10326e+06 | 27 50 | SIM 22755.2
             *
             * v20 / v21 318 TS1| 253 347        [1.37154]       717     1045684 t1: 1.32511e+06 t2: 1.32247e+06 | 11 36 | SIM 16195.8
             *
             * v21 / v22 L2 67 TS2| 107 165	[1.54206]	593	162871	t1: 655614 t2: 775788	| 2 8 | SIM 4816.03
             * v21 / v22 L1 126 TS2| 241 267        [1.10788]       3001    175656  t1: 711803 t2: 747112   | 9 10 | SIM 5194.67
             *
             * v21 / v22 L2 124 TS2| 212 286        [1.34906]       356     322499  t1: 1.38979e+06 t2: 1.68571e+06 | 6 12 | SIM 9996.73
             *
             *
             * pre_v22 / pre2_v22	98  TS2 L2| 208 188	[0.903846]	669	244659	t1: 1.20949e+06 t2: 1.21275e+06	| 10 4 | SIM 7347.14 | SPR 0.321496 0.257693
             * pre_v22 / pre2_v22      185  TS1 L2| 311 431    [1.38585]       972     719283  t1: 1.24825e+06 t2: 1.29072e+06 | 10 21 | SIM 12845.8 | SPR 0.264992 0.218586
             *
             * pre_v22 / pre2_v22	216  TS1 L1| 354 512	[1.44633]	3599	468998	t1: 602238 t2: 1.21725e+06	| 8 30 | SIM 7461.59 | SPR 0.403774 0.412574
             * pre_v22 / pre2_v22      104  TS2 L2| 216 202    [0.935185]      1285    226708  t1: 1.12173e+06 t2: 2.74487e+06 | 7 5 | SIM 6931.59 | SPR 0.400314 0.414285
             *
             * v21 / pre_v22   148  TS2 L2| 266 326    [1.22556]       521     360412  t1: 1.48741e+06 t2: 1.67739e+06 | 8 14 | SIM 10446.4 | SPR 0.399274 0.396958
             *
             *
             * v25 / v26	145  TS1 L2| 257 319	[1.24125]	216	541985	t1: 1.01127e+06 t2: 1.05271e+06	| 11 13 | SIM 11169.4 | SPR 0.390812 0.36432
             * v25 / v26       143  TS1 L2| 241 327    [1.35685]       948     533486  t1: 1.02743e+06 t2: 1.04908e+06 | 5 16 | SIM 11272.1 | SPR 0.388641 0.361009
             * v25 / v26       146  TS1 L2| 241 339    [1.40664]       1446    519711  t1: 1.03494e+06 t2: 1.05318e+06 | 5 20 | SIM 10990.9 | SPR 0.390057 0.363693
             * v25 / v26       158  TS1 L2| 288 340    [1.18056]       2162    537956  t1: 1.03006e+06 t2: 1.07246e+06 | 10 15 | SIM 11562 | SPR 0.389039 0.363488
             *
             * v28 / v32	109  TS1 L2| 188 252	[1.34043]	1618	436050	t1: 927435 t2: 1.27691e+06	| 5 11 | SIM 9587.19 | SPR 0.367798 0.380703
             * v28 / v32       108  TS1 L2| 192 242    [1.26042]       3599    415898  t1: 910437 t2: 1.29419e+06      | 6 11 | SIM 9268.22 | SPR 0.361986 0.376162
             * v28 / v32       112  TS1 L2| 224 226    [1.00893]       3599    439212  t1: 918325 t2: 1.31207e+06      | 9 9 | SIM 9828.87 | SPR 0.368146 0.3835
             * v28 / v32       105  TS1 L2| 206 218    [1.05825]       574     403027  t1: 933529 t2: 1.30092e+06      | 10 8 | SIM 8867.08 | SPR 0.359225 0.375229
             *
             * v28 / v36	499  TS1 L2| 874 1126(252)	[1.28833]	1963	1819556	t1: 3.36638e+06 t2: 3.25754e+06	| 28 55 | SIM 33505 | SPR 0.363908 0.374513
             * v28 / v36       499  TS1 L2| 921 1079(158)      [1.17155]       355     1744573 t1: 3.18765e+06 t2: 3.10492e+06 | 34 55 | SIM 32132.7 | SPR 0.36673 0.377685
             * v28 / v36       499  TS1 L2| 919 1081(162)      [1.17628]       262     1809455 t1: 3.34865e+06 t2: 3.24652e+06 | 32 52 | SIM 33060 | SPR 0.363986 0.374834
             * v28 / v36       499  TS1 L2| 896 1104(208)      [1.23214]       3599    1954051 t1: 3.63916e+06 t2: 3.49451e+06 | 30 51 | SIM 35598 | SPR 0.362046 0.37221
             *
             *
             * v36 / v38	57  TS2 L2| 97 135(38)	[1.39175]	972	145385	t1: 642802 t2: 661726	| 3 7 | SIM 4238 | SPR 0.391604 0.392015
             * v36 / v38       54  TS2 L2| 114 106(-8) [0.929825]      1228    123226  t1: 673722 t2: 649173   | 6 4 | SIM 4065.4 | SPR 0.391588 0.388084
             * v36 / v38       68  TS2 L2| 128 148(20) [1.15625]       751     134057  t1: 662502 t2: 654673   | 3 6 | SIM 4218.88 | SPR 0.391316 0.391165
             * v36 / v38       53  TS2 L2| 94 122(28)  [1.29787]       2202    126641  t1: 636175 t2: 661048   | 0 7 | SIM 3922.59 | SPR 0.388265 0.392846
             *
             * v36 / v38       133  TS1 L1| 226 308(82)        [1.36283]       539     297474  t1: 479174 t2: 479124   | 4 16 | SIM 5374.69 | SPR 0.377186 0.378
             * v36 / v38       147  TS1 L1| 250 342(92)        [1.368] 1850    296867  t1: 476265 t2: 477643   | 5 16 | SIM 5467.17 | SPR 0.385244 0.384093
             * v36 / v38       125  TS1 L1| 209 295(86)        [1.41148]       1308    284645  t1: 477693 t2: 482620   | 4 15 | SIM 5266.14 | SPR 0.376704 0.378
             * v36 / v38	177  TS1 L1| 338 372(34)	[1.10059]	2647	396808	t1: 659844 t2: 658165	| 13 15 | SIM 7245.1 | SPR 0.377865 0.377718
             *
             * v38 / v39	243  TS1 L2| 453 521(68)	[1.15011]	3599	864235	t1: 1.40482e+06 t2: 1.52515e+06	| 15 26 | SIM 13892.7 | SPR 0.367749 0.370665
             * v38 / v39       244  TS1 L2| 465 513(48)        [1.10323]       3599    837045  t1: 1.39445e+06 t2: 1.5287e+06  | 18 21 | SIM 13950.9 | SPR 0.369802 0.371716
             * v38 / v39       234  TS1 L2| 439 501(62)        [1.14123]       219     839810  t1: 1.38781e+06 t2: 1.53427e+06 | 14 26 | SIM 14100.7 | SPR 0.368211 0.371052
             *
             * v28 / v39	74  TS1 L2| 102 196(94)	[1.92157]	614	211615	t1: 360031 t2: 389769	| 1 11 | SIM 3680.05 | SPR 0.362094 0.372811
             * v28 / v39       61  TS1 L2| 91 155(64)  [1.7033]        1043    212869  t1: 351111 t2: 380192   | 1 8 | SIM 3621.31 | SPR 0.362648 0.373779
             * v28 / v39       75  TS1 L2| 109 195(86) [1.78899]       3599    210562  t1: 346243 t2: 381594   | 2 12 | SIM 3705.9 | SPR 0.361185 0.372542
             *
             * v39 / v40	71  TS2 L2| 130 156(26)	[1.2]	995	139629	t1: 675459 t2: 1.00903e+06	| 4 6 | SIM 4231.23 | SPR 0.390743 0.387634
             * v39 / v40       64  TS2 L2| 106 154(48) [1.45283]       1040    137147  t1: 664447 t2: 971223   | 3 6 | SIM 4065.95 | SPR 0.387741 0.386183
             * v39 / v40       67  TS2 L2| 118 152(34) [1.28814]       961     136985  t1: 649102 t2: 996180   | 3 7 | SIM 4150.7 | SPR 0.389807 0.388357
             *
             *
             *

             */

            //quickStartGuy::MyStrat strat1;
            //v1::MyStrat strat1;
            //v2::MyStrat strat1;
            //v3::MyStrat strat1;
            //v4::MyStrat strat1;
            //v5::MyStrat strat1;
            //v6::MyStrat strat1;
            //v8::MyStrat strat1;
            //v9::MyStrat strat1;
            //v10::MyStrat strat1;
            //v11::MyStrat strat1;
            //v12::MyStrat strat1;
            //v13::MyStrat strat1;
            //v16::MyStrat strat1;
            //v17::MyStrat strat1;
            //v18::MyStrat strat1;
            //v20::MyStrat strat1;
            //v21::MyStrat strat1;
            //v22::MyStrat strat1;
            //v25::MyStrat strat1;
            //v26::MyStrat strat1;
            //v27::MyStrat strat1;
            //v28::MyStrat strat1;
            //v36::MyStrat strat1;
            //v38::MyStrat strat1;
            v39::MyStrat strat1;
            MyStrat strat2;

            std::string name1 = strat1.getVersion();
            std::string name2 = strat2.getVersion();

            MyStratBase *leftStrat;
            MyStratBase *rightStrat;
            Results *leftRes;
            Results *rightRes;
            if (side == 0)
            {
                leftStrat = &strat1;
                leftRes = &res1;
                rightStrat = &strat2;
                rightRes = &res2;
            }
            else
            {
                leftStrat = &strat2;
                leftRes = &res2;
                rightStrat = &strat1;
                rightRes = &res1;
            }

            for (int tick = 0; tick < sim.level->properties.maxTickCount; ++tick)
            {
                sim.curTick = tick;

                if (render)
                    sendObjects(client, sim, side);

                for (MyUnit &u : sim.units)
                {
                    u.action = MyAction();
                }

                {
                    TimeMeasure t(leftRes->t);
                    leftStrat->compute(sim);
                }

                {
                    Simulator msim = mirrorSim(sim);
                    TimeMeasure t(rightRes->t);
                    rightStrat->compute(msim);
                }


                for (MyUnit &u : sim.units)
                    u.action = MyAction();

                for (MyUnit &u : leftStrat->sim.units)
                {
                    if (u.side == Side::ME)
                    {
                        for (MyUnit &u2 : sim.units)
                        {
                            if (u2.id == u.id)
                            {
                                u2.action = u.action;

                                if (u2.action.shoot && u2.weapon && u2.weapon.fireTimerMicrotics <= 100)
                                {
                                    leftRes->sumSpread += u2.weapon.spread;
                                    leftRes->spreadCount += 1.0;
                                }
                            }
                        }
                    }
                }

                for (MyUnit &u : rightStrat->sim.units)
                {
                    if (u.side == Side::ME)
                    {
                        for (MyUnit &u2 : sim.units)
                        {
                            if (u2.id == u.id)
                            {
                                u2.action = u.action;

                                if (u2.action.shoot && u2.weapon && u2.weapon.fireTimerMicrotics <= 100)
                                {
                                    rightRes->sumSpread += u2.weapon.spread;
                                    rightRes->spreadCount += 1.0;
                                }
                            }
                        }
                    }
                }

                /*for (MyUnit &u : strat3.sim.units)
                {
                    if (u.side == Side::ME)
                    {
                        for (MyUnit &u2 : sim.units)
                        {
                            if (u2.id == u.id)
                            {
                                if (u2.action.jump != u.action.jump)
                                {
                                    WRITE_LOG("=== JUMP " << u.action.jump);
                                }
                                if (u2.action.velocity != u.action.velocity)
                                {
                                    WRITE_LOG("=== VEL " << u.action.velocity << " " << u2.action.velocity);
                                }
                                if (u2.action.shoot != u.action.shoot)
                                {
                                    WRITE_LOG("=== SHOOT " << u.action.shoot);
                                }

                                //u2.action = u.action;
                            }
                        }
                    }
                }*/

                if (render)
                    sendActions(client, sim, side);

                {
                    TimeMeasure t(tsim);
                    sim.log = true;
                    sim.tickNoOpt();
                    sim.log = false;
                }

                std::set<Side> teams;

                for (const MyUnit &u : sim.units)
                    teams.insert(u.side);

                if (teams.size() < 2)
                {
                    if (render)
                        sendObjects(client, sim, side);

                    break;
                }
            }

            if (side == 1)
            {
                LOG("SCORE [" << sim.score[0] << "] " << sim.score[1] << " SF [" << sim.selfFire[0] << "] " << sim.selfFire[1]);
            }
            else
            {
                LOG("SCORE " << sim.score[0] << " [" << sim.score[1] << "] SF " << sim.selfFire[0] << " [" << sim.selfFire[1] << "]");
            }

            if (sim.score[0] > sim.score[1])
            {
                leftRes->totalWin += 2;
                leftRes->inRoundWin += 2;
            }
            else if (sim.score[1] > sim.score[0])
            {
                rightRes->totalWin += 2;
                rightRes->inRoundWin += 2;
            }
            else
            {
                leftRes->totalWin += 1;
                leftRes->inRoundWin += 1;
                rightRes->totalWin += 1;
                rightRes->inRoundWin += 1;
            }

            if (side == 1 && i % 5 == 4)
            {
                if (i > 0)
                {
                    if (leftRes->inRoundWin > rightRes->inRoundWin)
                        ++leftRes->roundsWin;
                    else if (leftRes->inRoundWin < rightRes->inRoundWin)
                        ++rightRes->roundsWin;
                }
            }

            totalTick += sim.curTick;
            LOG(name1 << " / " << name2 << "\t" << i << " " << " TS" << teamSize << " L" << (LEVEL_IND + 1) << "| " << res1.totalWin << " " << res2.totalWin << "(" << (res2.totalWin - res1.totalWin) << ")" << "\t[" << ((double) res2.totalWin / (double) res1.totalWin) << "]\t" << sim.curTick << "\t" << totalTick << "\tt1: " << res1.t << " t2: " << res2.t
                << "\t| " << res1.roundsWin << " " << res2.roundsWin << " | SIM " << tsim
                << " | SPR " << (res1.sumSpread / res1.spreadCount) << " " << (res2.sumSpread / res2.spreadCount));
        }
    }

}

void testSim()
{
    if (render)
        client.run();

    MyLevel level = createLevel("../levels/level.txt", true);

    if (render)
        sendMap(client, level);

    Simulator sim = createSim(&level);
    for (int tick = 0; tick < 10000; ++tick)
    {
        sim.curTick = tick;

        if (render)
            sendObjects(client, sim, 1);

        MyAction &a = sim.units[0].action;
        a = MyAction();

        /*if (tick == 0) {
            // do nothing
        } else {
            a.jump = true;
            if (tick == 67 || tick == 68) {
                std::cout << sim.units[0].pos.y << std::endl;
            }
        }*/

        if (tick < 100) {
            // do nothing
        }
        /*else if (tick < 200) {
            a.velocity = 10;
        }*/
        else {
            a.jump = true;
            /*if (tick == 52 || tick == 53) {
                std::cout << sim.units[0].pos.y << std::endl;
            }*/
        }

        /*a.velocity = level.properties.unitMaxHorizontalSpeed;
        //a.velocity = 0;
        a.jumpDown = false;
        a.jump = false;
        a.shoot = false;
        a.swapWeapon = false;
        a.plantMine = false;*/

        sim.tick();
    }
}















void renderPath(const MyLevel &level, const std::vector<int> &nodes)
{
    if (!render)
        return;

    auto toP = [&level](int n){
        int pos = n / 16;
        int x = pos % level.w;
        int y = pos / level.w;

        return P(x, y);
    };

    Obj obj;
    obj.type = "!path";
    obj.subObjs["line"]["type"] = "line";
    obj.subObjs["line"]["c"] = 0x00ff00ffu;

    for (int i = 0; i < nodes.size(); ++i)
    {
        P p1 = toP(nodes[i]);
        obj.subObjs["line"]["p" + std::to_string(i + 1)] = p1 + P(0.5, 0.5);
        obj.props["p" + std::to_string(i + 1)] = (double) (nodes[i] % 16);
    }

    client.sendObj(obj);
}

void testNav()
{
    if (render)
        client.run();

    MyLevel level = createLevel("../levels/levels3/level2.txt", true);

    if (render)
        sendMap(client, level);

    sendNewTick(client, 1);


    //Simulator sim = createSim(&level);

    NavMeshV2 navMesh;
    navMesh.compute(level);

    DistanceMapV2 distanceMapV2;
    PP p;
    p.pos = IP(36, 1).ind(level.w);
    p.upMomentum = 0;
    p.canCancel = true;
    distanceMapV2.compute(navMesh, p);

    p.pos = IP(2, 1).ind(level.w);
    LOG("D " << distanceMapV2.getDist(p));

    std::vector<int> nodes;
    nodes.push_back(p.ind());

    int ind = p.ind();
    while(true)
    {
        ind = distanceMapV2.prevNode[ind];
        if (ind == -1)
            break;

        nodes.push_back(ind);
    }

    LOG("NODES " << nodes.size());

    renderPath(level, nodes);
}




#ifdef CUSTOM_RUNNER
int main(int argc, char **argv) {
    std::cout << "Current path is " << fs::current_path() << '\n';

    //testNav();
    //return 0;
    //testSim();

    if (argc > 1)
        SEED = atoi(argv[1]);

    testLadder();
    testLadder2();
    testLadder3();
    testPlatform();
    testPlatform2();
    testJump();

    if (!repeater)
        mainRunner();
    else
        mainRun();
}

#endif
