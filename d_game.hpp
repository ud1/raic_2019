#ifndef RAIC2018_D_GAME_HPP
#define RAIC2018_D_GAME_HPP

#include "Simulator.hpp"
#include "d_tcpclient.hpp"
#include "d_format.hpp"

MyLevel createLevel(const std::string &levelFile, bool singleMicro);
void saveLevel(const Simulator &sim);
Simulator createSim(const MyLevel *level);
Simulator mirrorSim(const Simulator &sim);
void generateLootboxes(Simulator &sim);

uint32_t rgbF(float r, float g, float b);
void sendNewTick(TcpClient &client, uint32_t tick);
void sendMap(TcpClient &tcpClient, const MyLevel &level);
void sendObjects(TcpClient &tcpClient, const Simulator &sim, int side);
void sendActions(TcpClient &tcpClient, const Simulator &sim, int side);


#endif //RAIC2018_D_GAME_HPP
