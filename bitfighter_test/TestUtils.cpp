//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "TestUtils.h"
#include "../zap/gameType.h"
#include "../zap/GameManager.h"
#include "../zap/ServerGame.h"
#include "../zap/ClientGame.h"
#include "../zap/FontManager.h"
#include "../zap/UIManager.h"
#include "../zap/SystemFunctions.h"

#include "../zap/stringUtils.h"
#include "gtest/gtest.h"

#include <string>

using namespace std;

namespace Zap
{

// Create a new ClientGame with one dummy team -- be sure to delete this somewhere!
ClientGame *newClientGame()
{
   GameSettingsPtr settings = GameSettingsPtr(new GameSettings());
   return newClientGame(settings);
}


ClientGame *newClientGame(const GameSettingsPtr &settings)
{
   Address addr;
   
   // Need to initialize FontManager to use ClientGame... use false to avoid hassle of locating font files.
   // False will tell the FontManager to only use internally defined fonts; any TTF fonts will be replaced with Roman.
   FontManager::initialize(settings.get(), false);   
   ClientGame *game = new ClientGame(addr, settings, new UIManager());    // ClientGame destructor will clean up UIManager

   game->addTeam(new Team());     // Teams will be deleted by ClientGame destructor

   return game;
}


// Create a new ServerGame with one dummy team -- be sure to delete this somewhere!
ServerGame *newServerGame()
{
   Address addr;
   GameSettingsPtr settings = GameSettingsPtr(new GameSettings());
   LevelSourcePtr levelSource = LevelSourcePtr(new StringLevelSource(""));

   ServerGame *game = new ServerGame(addr, settings, levelSource, false, false);
   game->addTeam(new Team());    // Team will be cleaned up when game is deleted

   return game;
}


GamePair::GamePair(GameSettingsPtr settings)
{
   initialize(settings, "", 0);
}


// Create a pair of games suitable for testing client/server interaction.  Provide some levelcode to get things started.
GamePair::GamePair(const string &levelCode, S32 clientCount)
{
   GameSettingsPtr settings = GameSettingsPtr(new GameSettings());

   initialize(settings, levelCode, clientCount);
}


void GamePair::initialize(GameSettingsPtr settings, const string &levelCode, S32 clientCount)
{
   LevelSourcePtr levelSource = LevelSourcePtr(new StringLevelSource(levelCode));
   initHosting(settings, levelSource, true, false);      // Creates a game and adds it to GameManager

   server = GameManager::getServerGame();                // Get the game created in initHosting

   GameType *gt = new GameType();    // Cleaned up by database
   gt->addToGame(server, server->getGameObjDatabase());

   server->startHosting();          // This will load levels and wipe out any teams

   for(S32 i = 0; i < clientCount; i++)
      addClient("TestPlayer" + itos(i));

   LuaScriptRunner::setScriptingDir(settings->getFolderManager()->luaDir);
   LuaScriptRunner::startLua();
}


GamePair::~GamePair()
{
   LuaScriptRunner::clearScriptCache();
   LuaScriptRunner::shutdown();

   // Disconnect all ClientGames before deleting
   const Vector<ClientGame *> *clientGames = GameManager::getClientGames();

   for(S32 i = 0; i < clientGames->size(); i++)
      clientGames->get(i)->closeConnectionToGameServer();

   // Clean up GameManager
   GameManager::deleteServerGame();    
   GameManager::deleteClientGames();
}


// Idle a pair of games for a specified number of cycles, static method
void GamePair::idle(U32 timeDelta, U32 cycles)
{
   for(U32 i = 0; i < cycles; i++)
      GameManager::idle(timeDelta);
}


void GamePair::addClient(const string &name, S32 team)
{
   ClientGame *client = newClientGame(server->getSettingsPtr());
   GameManager::addClientGame(client);

   client->userEnteredLoginCredentials(name, "password", false);    // Simulates entry from NameEntryUserInterface

   client->joinLocalGame(server->getNetInterface());


   // This is a bit hacky, but we need to turn off TNL's bandwidth controls so our tests can run faster.  FASTER!!@!
   client->getConnectionToServer()->useZeroLatencyForTesting();

   clients.push_back(client);

   ClientInfo *clientInfo = server->getClientInfo(server->getClientInfos()->size() - 1);

   if(!clientInfo->isRobot())
      clientInfo->getConnection()->useZeroLatencyForTesting();

   if(team != NO_TEAM)
   {
      TNLAssert(team < server->getTeamCount(), "Bad team!");
      clientInfo->setTeamIndex(team);
   }
}


void GamePair::addBotClient(const string &name, S32 team)
{
   server->addBot(Vector<const char *>());
   // Get most recently added clientInfo
   ClientInfo *clientInfo = server->getClientInfo(server->getClientInfos()->size() - 1);
   ASSERT_TRUE(clientInfo->isRobot()) << "This is supposed to be a robot!";
   clientInfo->setTeamIndex(team);
}


};
