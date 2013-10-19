#include "TestUtils.h"
#include "../zap/ClientGame.h"
#include "../zap/ServerGame.h"
#include "../zap/gameType.h"
#include "../zap/luaLevelGenerator.h"
#include "gtest/gtest.h"

namespace Zap
{

using namespace std;
using namespace TNL;

TEST(RobotTest, addBot)
{
	GamePair gamePair;
	Vector<StringTableEntry> args;

	EXPECT_EQ(0, gamePair.server->getRobotCount());
	EXPECT_EQ(0, gamePair.client->getRobotCount());

	gamePair.server->getGameType()->addBot(args);

	for(U32 i = 0; i < 10; i++)
		gamePair.idle(10);
	
	EXPECT_EQ(1, gamePair.server->getRobotCount());
	EXPECT_EQ(1, gamePair.client->getRobotCount());
}


TEST(RobotTest, luaRobotNew)
{
	GamePair gamePair;

	EXPECT_EQ(0, gamePair.server->getRobotCount());
	EXPECT_EQ(0, gamePair.client->getRobotCount());

	LuaLevelGenerator levelgen(gamePair.server);
	levelgen.runScript(false);

	EXPECT_TRUE(levelgen.runString("bf:addItem(Robot.new())"));

	for(U32 i = 0; i < 10; i++)
		gamePair.idle(10);
	
	EXPECT_EQ(1, gamePair.server->getRobotCount());
	EXPECT_EQ(1, gamePair.client->getRobotCount());

	EXPECT_TRUE(levelgen.runString("bots = bf:findAllObjects(ObjType.Robot); bots[1]:removeFromGame()"));

	for(U32 i = 0; i < 10; i++)
		gamePair.idle(10);
	
	EXPECT_EQ(0, gamePair.server->getRobotCount());
	EXPECT_EQ(0, gamePair.client->getRobotCount());
}


};