//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008-2009 Chris Eykamp
// Original work copyright (C) 2004 GarageGames.com, Inc.
// Other code copyright as noted
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful (and fun!),
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#include "gameConnection.h"

#include "game.h"
#include "UIGame.h"
#include "UIMenus.h"
#include "UIInstructions.h"
#include "UIChat.h"
#include "UIMessage.h"
#include "UIDiagnostics.h"
#include "gameType.h"
#include "lpc10.h"

#include "../tnl/tnlEndian.h"

#include "ship.h"
#include "gameObjectRender.h"
#include "input.h"
#include "config.h"
#include "loadoutSelect.h"

#include "md5wrapper.h"    // For submission of passwords

#include "../glut/glutInclude.h"
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

namespace Zap
{

GameUserInterface gGameUserInterface;

// TODO: Make these static like privateF5MessageDisplayedInGameColor!
Color gGlobalChatColor(0.9, 0.9, 0.9);
Color gTeamChatColor(0, 1, 0);
Color gCmdChatColor(1, 0, 0);

Color GameUserInterface::privateF5MessageDisplayedInGameColor(0, 0, 1);

// Used to supply names for loutout indicators --> must correspond to enum ShipModule
const char *gModuleShortName[] = {
   "Shield",
   "Turbo",
   "Sensor",
   "Repair",
   "Engineer",
   "Cloak",
};



// Constructor
GameUserInterface::GameUserInterface()
{
   setMenuID(GameUI);
   setPlayMode();
   mInScoreboardMode = false;
#if 0 //defined(TNL_OS_XBOX)
   mFPSVisible = true;
#else
   mFPSVisible = false;
#endif
   mFPSAvg = 0;
   mFrameIndex = 0;
   for(U32 i = 0; i < FPSAvgCount; i++)
      mIdleTimeDelta[i] = 50;

   memset(mChatBuffer, 0, sizeof(mChatBuffer));

   // Initialize message buffers
   for(S32 i = 0; i < MessageDisplayCount; i++)
      mDisplayMessage[i][0] = 0;

   for(S32 i = 0; i < MessageStoreCount; i++)
      mStoreMessage[i][0] = 0;

   mGotControlUpdate = false;
   mRecalcFPSTimer = 0;

   mFiring = false;
   for (U32 i = 0; i < ShipModuleCount; i++)
      mModActivated[i] = false;

   mDisplayMessageTimer.setPeriod(DisplayMessageTimeout);    // Set the period of our message timeout timer

   populateChatCmdList();
}


extern bool gDisableShipKeyboardInput;
extern Point gMousePos;

void GameUserInterface::onActivate()
{
   gDisableShipKeyboardInput = false;                     // Make sure our ship controls are active
   mMissionOverlayActive = false;                         // Turn off the mission overlay (if it was on)
   glutSetCursor(GLUT_CURSOR_NONE);                       // Turn off cursor
   onMouseMoved((S32) gMousePos.x, (S32) gMousePos.y);    // Make sure ship pointed is towards mouse

   // Clear out any lingering chat messages
   for(S32 i = 0; i < MessageStoreCount; i++)   
      mDisplayMessage[i][0] = 0; 

   for(S32 i = 0; i < MessageDisplayCount; i++)   
      mStoreMessage[i][0] = 0;  

   mMessageDisplayMode = ShortTimeout;                    // Start with normal chat msg display
   setPlayMode();                                         // Make sure we're not in chat or loadout-select mode

   for(S32 i = 0; i < ShipModuleCount; i++)
      mModActivated[i] = false;
}


void GameUserInterface::onReactivate()
{
   gDisableShipKeyboardInput = false;
   glutSetCursor(GLUT_CURSOR_NONE);    // Turn off cursor
   setPlayMode();

   for(S32 i = 0; i < ShipModuleCount; i++)
      mModActivated[i] = false;

   // Call onMouseMoved to get ship pointed at current cursor location
   onMouseMoved((S32) gMousePos.x, (S32) gMousePos.y);
}


// A new chat message is here!  We don't actually display anything here, despite the name...  
// just add it to the list, will be displayed in render()
void GameUserInterface::displayMessage(Color theColor, const char *format, ...)
{
   // Create a slot for our new message
   if(mDisplayMessage[0][0])
      for(S32 i = MessageDisplayCount - 1; i > 0; i--)
      {
         strcpy(mDisplayMessage[i], mDisplayMessage[i-1]);
         mDisplayMessageColor[i] = mDisplayMessageColor[i-1];
      }

   for(S32 i = MessageStoreCount - 1; i > 0; i--)
   {
      strcpy(mStoreMessage[i], mStoreMessage[i-1]);
      mStoreMessageColor[i] = mStoreMessageColor[i-1];
   }
   
   va_list args;
   va_start(args, format);

   dVsprintf(mDisplayMessage[0], sizeof(mDisplayMessage[0]), format, args);
   mDisplayMessageColor[0] = theColor;

   dVsprintf(mStoreMessage[0], sizeof(mStoreMessage[0]), format, args);
   mStoreMessageColor[0] = theColor;

   mDisplayMessageTimer.reset();
}


void GameUserInterface::idle(U32 timeDelta)
{
   if(mDisplayMessageTimer.update(timeDelta))
   {
      for(S32 i = MessageDisplayCount - 1; i > 0; i--)
      {
         strcpy(mDisplayMessage[i], mDisplayMessage[i-1]);
         mDisplayMessageColor[i] = mDisplayMessageColor[i-1];
      }

      mDisplayMessage[0][0] = 0;    // Null, that is
      mDisplayMessageTimer.reset();
   }

   // Time to recalc FPS?
   if (mFPSVisible)        // Only bother if we're displaying the value...
   {
      if(timeDelta > mRecalcFPSTimer)
      {
         U32 sum = 0;
         for(U32 i = 0; i < FPSAvgCount; i++)
            sum += mIdleTimeDelta[i];
         mFPSAvg = (1000 * FPSAvgCount) / F32(sum);
         mRecalcFPSTimer = 1000;
      }
      else
         mRecalcFPSTimer -= timeDelta;
   }

   if(mCurrentMode == ChatMode)
      updateCursorBlink(timeDelta);    // Blink the cursor if in ChatMode
   else if(mCurrentMode == QuickChatMode)
      mQuickChat.idle(timeDelta);
   else if(mCurrentMode == LoadoutMode)
      mLoadout.idle(timeDelta);

   mVoiceRecorder.idle(timeDelta);
   mIdleTimeDelta[mFrameIndex % FPSAvgCount] = timeDelta;
   mFrameIndex++;

   mWrongModeMsgDisplay.update(timeDelta);

   mProgressBarFadeTimer.update(timeDelta);

   // Should we move this timer over to UIGame??
   if(gMainMenuUserInterface.levelLoadDisplayFadeTimer.update(timeDelta))
      gMainMenuUserInterface.clearLevelLoadDisplay();
}


#ifdef TNL_OS_WIN32
extern void checkMousePos(S32 maxdx, S32 maxdy);
#endif


// Draw main game screen (client only)
void GameUserInterface::render()
{
   glColor3f(0.0, 0.0, 0.0);
   if(!gClientGame->isConnectedToServer())
   {
      glColor3f(1, 1, 1);
      drawCenteredString(260, 30, "Connecting to server...");

      glColor3f(0, 1, 0);
      if(gClientGame->getConnectionToServer())
         drawCenteredString(310, 16, gConnectStatesTable[gClientGame->getConnectionToServer()->getConnectionState()]);

      glColor3f(1, 1, 1);
      drawCenteredString(346, 20, "Press <ESC> to abort");
   }

   gClientGame->render();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();          // OpenGL command to load an identity matrix (see OpenGL docs)

   renderReticle();           // Draw crosshairs if using mouse
   renderMessageDisplay();    // Render incoming chat msgs
   renderCurrentChat();       // Render any chat msg user is composing
   renderLoadoutIndicators(); // Draw indicators for the various loadout items

   gMainMenuUserInterface.renderProgressListItems();  // This is the list of levels loaded while hosting

   renderProgressBar();       // This is the status bar that shows progress of loading this level

   mVoiceRecorder.render();   // This is the indicator that someone is sending a voice msg

   // Display running average FPS
   if(mFPSVisible)
   {
      glColor3f(1, 1, 1);
      drawStringf(canvasWidth - horizMargin - 170, vertMargin, 30, "%4.1f fps", mFPSAvg);
   }

   // Render QuickChat / Loadout menus
   if(mCurrentMode == QuickChatMode)
   {     // (braces required)
      if(!mQuickChat.render())      // Render QuickChat msgs if there are any, otherwise switch back into PlayMode
         setPlayMode();
   }
   else if(mCurrentMode == LoadoutMode)
      mLoadout.render();

   GameType *theGameType = gClientGame->getGameType();

   if(theGameType)
      theGameType->renderInterfaceOverlay(mInScoreboardMode);

#if 0
   // Some code for outputting the position of the ship for finding good spawns
   GameConnection *con = gClientGame->getConnectionToServer();

   if(con)
   {
      GameObject *co = con->getControlObject();
      if(co)
      {
         Point pos = co->getActualPos() * F32(1 / 300.0f);
         drawStringf(10, 550, 30, "%0.2g, %0.2g", pos.x, pos.y);
      }
   }

   if(mGotControlUpdate)
      drawString(710, 10, 30, "CU");
#endif
}


// Draws level-load progress bar across the bottom of the screen
void GameUserInterface::renderProgressBar()
{
   GameType *gt = gClientGame->getGameType();
   if((mShowProgressBar || mProgressBarFadeTimer.getCurrent() > 0) && gt && gt->mObjectsExpected > 0)
   {
      glEnable(GL_BLEND);
      glColor4f(0, 1, 0, mShowProgressBar ? 1 : mProgressBarFadeTimer.getFraction());

      // Outline
      const S32 left = 200;
      const S32 width = canvasWidth - 2 * left;
      const S32 height = 10;

      // For some reason, there are occasions where the status bar doesn't progress all the way over during the load process.
      // The problem is that, for some reason, some objects do not add themselves to the loaded object counter, and this creates
      // a disconcerting effect, as if the level did not fully load.  Rather than waste any more time on this problem, we'll just
      // fill in the status bar while it's fading, to make it look like the level fully loaded.  Since the only thing that this
      // whole mechanism is used for is to display something to the user, this should work fine.
      F32 barWidth = mShowProgressBar ? ((F32) width * (F32) gClientGame->mObjectsLoaded / (F32) gt->mObjectsExpected) : width;

      glBegin(GL_LINE_LOOP);
         glVertex2f(left, canvasHeight - vertMargin);
         glVertex2f(left + width, canvasHeight - vertMargin);
         glVertex2f(left + width, canvasHeight - vertMargin - height);
         glVertex2f(left, canvasHeight - vertMargin - height);
      glEnd();

      glBegin(GL_POLYGON);
         glVertex2f(left, canvasHeight - vertMargin);
         glVertex2f(left + barWidth, canvasHeight - vertMargin);
         glVertex2f(left + barWidth, canvasHeight - vertMargin - height);
         glVertex2f(left, canvasHeight - vertMargin - height);
      glEnd();

      glDisable(GL_BLEND);
   }
}


extern CmdLineSettings gCmdLineSettings;
extern IniSettings gIniSettings;

// Draw the reticle (i.e. the mouse cursor) if we are using keyboard/mouse
void GameUserInterface::renderReticle()
{
   if(gIniSettings.inputMode == Keyboard)
   {
#if 0 // TNL_OS_WIN32
      Point realMousePoint = mMousePoint;
      if(!gIniSettings.controlsRelative)
      {
         F32 len = mMousePoint.len();
         checkMousePos(windowWidth * 100 / canvasWidth,
                     windowHeight * 100 / canvasHeight);

         if(len > 100)
            realMousePoint *= 100 / len;
      }
#endif
      Point offsetMouse = mMousePoint + Point(canvasWidth / 2, canvasHeight / 2);

      glEnable(GL_BLEND);
      glColor4f(0,1,0, 0.7);
      glBegin(GL_LINES);

      glVertex2f(offsetMouse.x - 15, offsetMouse.y);
      glVertex2f(offsetMouse.x + 15, offsetMouse.y);
      glVertex2f(offsetMouse.x, offsetMouse.y - 15);
      glVertex2f(offsetMouse.x, offsetMouse.y + 15);

      if(offsetMouse.x > 30)
      {
         glColor4f(0,1,0, 0);
         glVertex2f(0, offsetMouse.y);
         glColor4f(0,1,0, 0.7);
         glVertex2f(offsetMouse.x - 30, offsetMouse.y);
      }
      if(offsetMouse.x < canvasWidth - 30)
      {
         glColor4f(0,1,0, 0.7);
         glVertex2f(offsetMouse.x + 30, offsetMouse.y);
         glColor4f(0,1,0, 0);
         glVertex2f(canvasWidth, offsetMouse.y);
      }
      if(offsetMouse.y > 30)
      {
         glColor4f(0,1,0, 0);
         glVertex2f(offsetMouse.x, 0);
         glColor4f(0,1,0, 0.7);
         glVertex2f(offsetMouse.x, offsetMouse.y - 30);
      }
      if(offsetMouse.y < canvasHeight - 30)
      {
         glColor4f(0,1,0, 0.7);
         glVertex2f(offsetMouse.x, offsetMouse.y + 30);
         glColor4f(0,1,0, 0);
         glVertex2f(offsetMouse.x, canvasHeight);
      }

      glEnd();
      glDisable(GL_BLEND);
   }

   if(mWrongModeMsgDisplay.getCurrent())
   {
      glColor3f(1,.5,.5);
      drawCenteredString(225, 20, "You are in joystick mode.");
      drawCenteredString(250, 20, "You can change to Keyboard input with the Options menu.");

   }
}

extern LoadoutItem gLoadoutModules[];

#define fontSize 15
#define gapSize 3       // Gap between text and box

// Draw weapon indicators at top of the screen
void GameUserInterface::renderLoadoutIndicators()
{
   if (!gIniSettings.showWeaponIndicators)      // If we're not drawing them, we've got nothing to do
      return;

   U32 xPos = UserInterface::horizMargin;

   if(!gClientGame || !gClientGame->getConnectionToServer())
      return;
   Ship *localShip = dynamic_cast<Ship *>(gClientGame->getConnectionToServer()->getControlObject());

   if (!localShip)
      return;

   glEnable(GL_BLEND);

   // First, the weapons
   for(S32 i = 0; i < ShipWeaponCount; i++)
   {
      U32 width = getStringWidth(fontSize, gWeapons[localShip->getWeapon(i)].name.getString());

      if(i == localShip->mActiveWeaponIndx)
         glColor4f(1, 1, 0, 1);
      else
         glColor4f(1, 1, 0, 0.5);

      glBegin(GL_POLYGON);
         glVertex2f(xPos, UserInterface::vertMargin);
         glVertex2f(xPos + width + 2 * gapSize, UserInterface::vertMargin);
         glVertex2f(xPos + width + 2 * gapSize, UserInterface::vertMargin + fontSize + 2 * gapSize);
         glVertex2f(xPos, UserInterface::vertMargin + fontSize + 2 * gapSize);
      glEnd();

      if(i == localShip->mActiveWeaponIndx)
         glColor3f(0,0,0);
      else
         glColor3f(.2,.2,.2);

      // Add the weapon name
      drawString(xPos + gapSize, UserInterface::vertMargin + gapSize, fontSize, gWeapons[localShip->getWeapon(i)].name.getString());

      xPos += UserInterface::vertMargin + width - 2 * gapSize;
   }

   xPos += 20;    // Small horizontal gap

   // Next, loadout modules
   for(S32 i = 0; i < ShipModuleCount; i++)
   {
      U32 width = getStringWidth(fontSize, gModuleShortName[localShip->getModule(i)]);

      if(localShip->isModuleActive(localShip->getModule(i)))
         glColor4f(0, 1, 0, 1);
      else
         glColor4f(0, 1, 0, 0.5);

      glBegin(GL_POLYGON);
         glVertex2f(xPos, UserInterface::vertMargin);
         glVertex2f(xPos + width + 2 * gapSize, UserInterface::vertMargin);
         glVertex2f(xPos + width + 2 * gapSize, UserInterface::vertMargin + fontSize + 2 * gapSize);
         glVertex2f(xPos, UserInterface::vertMargin + fontSize + 2 * gapSize);
      glEnd();

      if(i == localShip->mActiveWeaponIndx)
         glColor3f(0,0,0);
      else
         glColor3f(.2,.2,.2);

      // Add the weapon name
      drawString(xPos + gapSize, UserInterface::vertMargin + gapSize, fontSize, gModuleShortName[localShip->getModule(i)]);

      xPos += UserInterface::vertMargin + width - 2 * gapSize;
   }

   glDisable(GL_BLEND);
}

S32 gLoadoutIndicatorHeight = fontSize + gapSize * 2;

#undef fontSize
#undef gapSize

#define FONTSIZE 14
#define fontGap 4

// Render any incoming chat msgs
void GameUserInterface::renderMessageDisplay()
{
   glColor3f(1,1,1);

   S32 y = gIniSettings.showWeaponIndicators ? UserInterface::chatMargin : UserInterface::vertMargin;
   S32 msgCount;

   if(mMessageDisplayMode == LongFixed)
      msgCount = MessageStoreCount;    // Long form
   else
      msgCount = MessageDisplayCount;  // Short form


   if(mMessageDisplayMode == ShortTimeout)
      for(S32 i = msgCount - 1; i >= 0; i--)
      {
         if(mDisplayMessage[i][0])
         {
            glColor(mDisplayMessageColor[i]);
            drawString(UserInterface::horizMargin, y, FONTSIZE, mDisplayMessage[i]);
            y += FONTSIZE + fontGap;
         }
      }
   else
      for(S32 i = msgCount - 1; i >= 0; i--)
      {
         if(mStoreMessage[i][0])
         {
            glColor(mStoreMessageColor[i]);
            drawString(UserInterface::horizMargin, y, FONTSIZE, mStoreMessage[i]);
            y += FONTSIZE + fontGap;
         }
      }   

   // Render faint gray background box... probably better without this...
   //S32 ypos1 = (gIniSettings.showWeaponIndicators ? UserInterface::chatMargin : UserInterface::vertMargin);
   //S32 ypos2 = ypos1 + (FONTSIZE + fontGap) * MessageStoreCount;
   //S32 width = canvasWidth - UserInterface::horizMargin;

   //glEnable(GL_BLEND);
   //   glColor4f(1,1,1,.2);
   //   glBegin(GL_POLYGON);
   //      glVertex2f(UserInterface::horizMargin, UserInterface::vertMargin + ypos1 - 3);
   //      glVertex2f(width, UserInterface::vertMargin + ypos1 - 3);
   //      glVertex2f(width, UserInterface::vertMargin + ypos2);
   //      glVertex2f(UserInterface::horizMargin, UserInterface::vertMargin + ypos2);
   //   glEnd();
   //   glColor4f(1,1,1,.4);
   //   glBegin(GL_LINE_LOOP);
   //      glVertex2f(UserInterface::horizMargin, UserInterface::vertMargin + ypos1 - 3);
   //      glVertex2f(width, UserInterface::vertMargin + ypos1 - 3);
   //      glVertex2f(width, UserInterface::vertMargin + ypos2);
   //      glVertex2f(UserInterface::horizMargin, UserInterface::vertMargin + ypos2);
   //   glEnd();
   //glDisable(GL_BLEND);
}


// Render chat msg that user is composing
void GameUserInterface::renderCurrentChat()
{
   if(mCurrentMode != ChatMode)
      return;

   const S32 chatComposeYPos = (gIniSettings.showWeaponIndicators ? UserInterface::chatMargin : UserInterface::vertMargin) + 
                               (mMessageDisplayMode == LongFixed ? MessageStoreCount : MessageDisplayCount) * (FONTSIZE + fontGap);
   const char *promptStr;

   Color baseColor;

   if(mChatBuffer[0] == '/' || mCurrentChatType == CmdChat)      // Whatever the underlying chat mode, seems we're entering a command here
   {
      baseColor = gCmdChatColor;
      promptStr = mCurrentChatType ? "(Command): /" : "(Command): ";
   }
   else if(mCurrentChatType == TeamChat)    // Team chat (goes to all players on team)
   {
      baseColor = gTeamChatColor;
      promptStr = "(Team): ";
   }
   else                                // Global in-game chat (goes to all players in game)
   {
      baseColor = gGlobalChatColor;
      promptStr = "(Global): ";
   }

   // Protect against crashes while game is initializing
   if(! (gClientGame && gClientGame->getConnectionToServer() && gClientGame->getConnectionToServer()->getControlObject()))
      return;

   Ship *ship = dynamic_cast<Ship *>(gClientGame->getConnectionToServer()->getControlObject());
   S32 promptSize = getStringWidthf(FONTSIZE, "%s", promptStr);
   S32 nameSize = getStringWidthf(FONTSIZE, "%s: ", ship->getName().getString());
   S32 nameWidth = max(nameSize, promptSize);
   // Above block repeated below...

   S32 xpos = UserInterface::horizMargin;
   S32 width = canvasWidth - 2 * UserInterface::horizMargin - (nameWidth - promptSize) + 6;


   // Render text entry box like thingy
   glEnable(GL_BLEND);
      glColor4f(baseColor.r, baseColor.g, baseColor.b, .25);
      glBegin(GL_POLYGON);
         glVertex2f(xpos, UserInterface::vertMargin + chatComposeYPos - 3);
         glVertex2f(xpos + width, UserInterface::vertMargin + chatComposeYPos - 3);
         glVertex2f(xpos + width, UserInterface::vertMargin + chatComposeYPos + FONTSIZE + 7);
         glVertex2f(xpos, UserInterface::vertMargin + chatComposeYPos + FONTSIZE + 7);
      glEnd();

      glColor4f(baseColor.r, baseColor.g, baseColor.b, .4);
      glBegin(GL_LINE_LOOP);
         glVertex2f(xpos, UserInterface::vertMargin + chatComposeYPos - 3);
         glVertex2f(xpos + width, UserInterface::vertMargin + chatComposeYPos - 3);
         glVertex2f(xpos + width, UserInterface::vertMargin + chatComposeYPos + FONTSIZE + 7);
         glVertex2f(xpos, UserInterface::vertMargin + chatComposeYPos + FONTSIZE + 7);
      glEnd();

   glColor3f(baseColor.r, baseColor.g, baseColor.b);
   drawString(xpos + 3, UserInterface::vertMargin + chatComposeYPos, FONTSIZE, promptStr);
   drawStringf(xpos + getStringWidth(FONTSIZE, promptStr) + 3, UserInterface::vertMargin + chatComposeYPos, FONTSIZE, "%s%s", mChatBuffer, cursorBlink ? "_" : " ");
   glDisable(GL_BLEND);
}

#undef fontGap


void GameUserInterface::onMouseDragged(S32 x, S32 y)
{
   onMouseMoved(x, y);
}

// We'll store the mouse location in mMousePoint for use when we need it
void GameUserInterface::onMouseMoved(S32 x, S32 y)
{
   mMousePoint = Point(x - (S32) windowWidth / 2, y - (S32) windowHeight / 2);
   mMousePoint.x = mMousePoint.x * canvasWidth / windowWidth;
   mMousePoint.y = mMousePoint.y * canvasHeight / windowHeight;

   if (gClientGame->getInCommanderMap())     // Ship not in center of the screen in cmdrs map.  Where is it?
   {
      // Here it is...
      GameObject *controlObject = gClientGame->getConnectionToServer()->getControlObject();
      Ship *u = dynamic_cast<Ship *>(controlObject);

      if(!u)      // Can sometimes happen when switching mazes. This will stop the ensuing crashing.
         return;
      Point p = gClientGame->worldToScreenPoint( u->getRenderPos() );

      mCurrentMove.angle = atan2(mMousePoint.y + canvasHeight / 2 - p.y, mMousePoint.x + canvasWidth / 2 - p.x);
   }

   else     // Ship is at center of the screen
      mCurrentMove.angle = atan2(mMousePoint.y, mMousePoint.x);
}

// Enter quick chat mode
void GameUserInterface::enterQuickChat()
{
   bool fromController = (gIniSettings.inputMode == Joystick);
   UserInterface::playBoop();
   mQuickChat.show(fromController);
   mCurrentMode = QuickChatMode;
}

// Enter loadout mode
void GameUserInterface::enterLoadout()
{
   bool fromController = (gIniSettings.inputMode == Joystick);
   UserInterface::playBoop();
   mLoadout.show(fromController);
   mCurrentMode = LoadoutMode;
}

// Set current mode to playout, duh!
void GameUserInterface::setPlayMode()
{
   mCurrentMode = PlayMode;
   setBusyChatting(false);
   mUpDisabled = false;
   mDownDisabled = false;
   mLeftDisabled = false;
   mRightDisabled = false;
}


// Send a message to the server that we are (or are not) busy chatting
void GameUserInterface::setBusyChatting(bool busy)
{
   if( gClientGame && gClientGame->getConnectionToServer() )
      gClientGame->getConnectionToServer()->c2sSetIsBusy(busy);
}


// Select next weapon
void GameUserInterface::advanceWeapon()
{
   GameType *g = gClientGame->getGameType();
   if(g)
      g->c2sAdvanceWeapon();
}

// Select a weapon by its index
void GameUserInterface::selectWeapon(U32 indx)
{
   GameType *g = gClientGame->getGameType();
   if(g)
      g->c2sSelectWeapon(indx);
}

// Temporarily disable the effects of a movement key to avoid unpleasant interactions between ship movement and loadout/quick chat entry
void GameUserInterface::disableMovementKey(KeyCode keyCode)
{
   InputMode inputMode = gIniSettings.inputMode;

   if(keyCode == keyUP[inputMode])
      mUpDisabled = true;
   else if(keyCode == keyDOWN[inputMode])
      mDownDisabled = true;
   else if(keyCode == keyLEFT[inputMode])
      mLeftDisabled = true;
   else if(keyCode == keyRIGHT[inputMode])
      mRightDisabled = true;
}

// Key pressed --> take action!
// Handles all keypress events, including mouse clicks and controller button presses
void GameUserInterface::onKeyDown(KeyCode keyCode, char ascii)
{
   S32 inputMode = gIniSettings.inputMode;

   if(keyCode == keyHELP)          // Turn on help screen
   {
      UserInterface::playBoop();
      gInstructionsUserInterface.activate();
      return;
   }
   else if(keyCode == keyOUTGAMECHAT)
   {
      setBusyChatting(true);
      gChatInterface.activate();
      return;
   }
   else if(keyCode == keyMISSION)
   {
      mMissionOverlayActive = true;
      gClientGame->getGameType()->mLevelInfoDisplayTimer.clear();    // Clear level-start display if user hits F2
   }
   else if(keyCode == KEY_M && getKeyState(KEY_CTRL))    //Ctrl-M, for now, to cycle through message dispaly modes
   {
      S32 m = mMessageDisplayMode + 1;
      if(m >= MessageDisplayModes)
         m = 0;
      mMessageDisplayMode = MessageDisplayMode(m); 

      return;
   }


   // First, if we are in loadout mode, check the key to see if
   // it does something with the loadout menu.  If not, we'll
   // further process it below.

   if(mCurrentMode == LoadoutMode)
   {  // (braces required)
      if(mLoadout.processKeyCode(keyCode))   // Will return true if key was processed
      {
         disableMovementKey(keyCode);
         return;                             // Leave if key did something, so we don't get "dual" effect
      }
   }
   else if(mCurrentMode == QuickChatMode)
      if(mQuickChat.processKeyCode(keyCode))     // Hand off key handling to the quick chat object
      {
         disableMovementKey(keyCode);
         return;
      }


   if(mCurrentMode == LoadoutMode || mCurrentMode == PlayMode || mCurrentMode == QuickChatMode)
   {
      // The following keys are allowed in both play mode and in
      // loadout or engineering menu modes if not used in the loadout
      // menu above

      if (keyCode == keyMOD1[inputMode])
         mModActivated[0] = true;
      else if (keyCode == keyMOD2[inputMode])
         mModActivated[1] = true;
      else if (keyCode == keyFIRE[inputMode])
         mFiring = true;
      else if(keyCode == keySELWEAP1[inputMode])
         selectWeapon(0);
      else if(keyCode == keySELWEAP2[inputMode])
         selectWeapon(1);
      else if(keyCode == keySELWEAP3[inputMode])
         selectWeapon(2);
      else if(keyCode == keyFPS)
         mFPSVisible = !mFPSVisible;
      else if(keyCode == keyADVWEAP[inputMode])
         advanceWeapon();
      else if(keyCode == KEY_ESCAPE || keyCode == BUTTON_BACK)
      {
         UserInterface::playBoop();

         if(!gClientGame->isConnectedToServer())      // Perhaps we're still joining?
         {
            endGame();
            gMainMenuUserInterface.activate();
         }
         else
         {
            setBusyChatting(true);
            gGameMenuUserInterface.activate();
         }
      }
      else if(keyCode == keyCMDRMAP[inputMode])
         gClientGame->zoomCommanderMap();

      else if(keyCode == keySCRBRD[inputMode])
      {     // (braces needed)
         if(!mInScoreboardMode)    // We're activating the scoreboard
         {
            mInScoreboardMode = true;
            GameType *g = gClientGame->getGameType();
            if(g)
               g->c2sRequestScoreboardUpdates(true);
         }
      }
      else if(keyCode == keyTOGVOICE[inputMode])
      {     // (braces needed)
         if(!mVoiceRecorder.mRecordingAudio)  // Turning recorder on
            mVoiceRecorder.start();
      }
      else if(mCurrentMode != LoadoutMode && mCurrentMode != QuickChatMode)
      {
         // The following keys are only allowed in PlayMode, and never work in LoadoutMode
         if(keyCode == keyTEAMCHAT[inputMode])
         {
            mCurrentChatType = TeamChat;
            mCurrentMode = ChatMode;
            setBusyChatting(true);
         }
         else if(keyCode == keyGLOBCHAT[inputMode])
         {
            mCurrentChatType = GlobalChat;
            mCurrentMode = ChatMode;
            setBusyChatting(true);
         }
         else if(keyCode == keyCMDCHAT[inputMode])
         {
            mCurrentChatType = CmdChat;
            mCurrentMode = ChatMode;
            setBusyChatting(true);
         }
         else if(keyCode == keyQUICKCHAT[inputMode])
            enterQuickChat();
         else if(keyCode == keyLOADOUT[inputMode])
            enterLoadout();

         else if(inputMode == Joystick)      // Check if the user is trying to use keyboard to move when in joystick mode
            if(keyCode == keyUP[Keyboard] || keyCode == keyDOWN[Keyboard] || keyCode == keyLEFT[Keyboard] || keyCode == keyRIGHT[Keyboard])
               mWrongModeMsgDisplay.reset(WrongModeMsgDisplayTime);
      }
   }     // End if in LoadoutMode or PlayMode

   else if(mCurrentMode == ChatMode)   // Player is entering a chat message
   {
      if(keyCode == KEY_ENTER)
         issueChat();
      else if(keyCode == KEY_BACKSPACE || keyCode == KEY_DELETE)
      {     // (braces required)
         if(mChatCursorPos > 0)
         {
            mChatCursorPos--;
            for(U32 i = mChatCursorPos; mChatBuffer[i]; i++)
               mChatBuffer[i] = mChatBuffer[i+1];
         }
      }
      else if(keyCode == KEY_ESCAPE || keyCode == BUTTON_BACK)
         cancelChat();
      else if(keyCode == KEY_TAB)      // Auto complete any commands
      {
         if(mChatBuffer[0] == '/' || mCurrentChatType == CmdChat)     // It's a command!
         {
            S32 found = -1;
            S32 start = mCurrentChatType ? 1 : 0;     // Do we need to lop the leading '/' off mChatCmds item?

            size_t len = strlen(mChatBuffer);    
            for(S32 i = 0; i < mChatCmds.size(); i++)
               if( mChatCmds[i].substr(start, len) == string(mChatBuffer) )
               {
                  if(found != -1)   // We've already found another command that matches this one, so that means it's not yet unique enough to autocomplete.
                     return;
                  found = i;
               }

            if(found == -1)      // Found no match
               return;

            U32 clen = (U32) mChatCmds[found].copy(mChatBuffer, mChatCmds[found].length(), start);      // Complete command
            mChatBuffer[clen] = ' ';        // append trailing space for ready entry of next param
            mChatBuffer[clen + 1] = '\0';   // terminate the string
            mChatCursorPos = clen + 1;
         }
      }
      else if(ascii)     // Append any other keys to the chat message
      {
         // Protect against crashes while game is initializing
         if(gClientGame && gClientGame->getConnectionToServer() && gClientGame->getConnectionToServer()->getControlObject())
         {
            for(U32 i = sizeof(mChatBuffer) - 2; i > mChatCursorPos; i--)
               mChatBuffer[i] = mChatBuffer[i-1];

            S32 promptSize = getStringWidth(FONTSIZE, mCurrentChatType == TeamChat ? "(Team): " : "(Global): ");

            Ship *ship = dynamic_cast<Ship *>(gClientGame->getConnectionToServer()->getControlObject());
            S32 nameSize = getStringWidthf(FONTSIZE, "%s: ", ship->getName().getString());
            S32 nameWidth = max(nameSize, promptSize);
            // Above block repeated above

            if(mChatCursorPos < sizeof(mChatBuffer) - 2 && nameWidth + (S32) getStringWidthf(FONTSIZE, "%s%c", mChatBuffer, ascii) < canvasWidth - 2 * horizMargin - 3)
            {
               mChatBuffer[mChatCursorPos] = ascii;
               mChatCursorPos++;
            }
         }
      }
   }     // End if in ChatMode
}

#undef FONTSIZE


void GameUserInterface::onKeyUp(KeyCode keyCode)
{
      S32 inputMode = gIniSettings.inputMode;

   // These keys works in any mode!  And why not??

   if(keyCode == keyMISSION)
      mMissionOverlayActive = false;
   else if (keyCode == keyMOD1[inputMode])
      mModActivated[0] = false;
   else if (keyCode == keyMOD2[inputMode])
      mModActivated[1] = false;
   else if (keyCode == keyFIRE[inputMode])
      mFiring = false;
   else if(keyCode == keySCRBRD[inputMode])
   {     // (braces required)
      if(mInScoreboardMode)     // We're turning scoreboard off
      {
         mInScoreboardMode = false;
         GameType *g = gClientGame->getGameType();
         if(g)
            g->c2sRequestScoreboardUpdates(false);
      }
   }
   else if(keyCode == keyTOGVOICE[inputMode])
   {     // (braces required)
      if(mVoiceRecorder.mRecordingAudio)  // Turning recorder off
         mVoiceRecorder.stop();
   }
   else if(keyCode == keyUP[inputMode])
      mUpDisabled = false;
   else if(keyCode == keyDOWN[inputMode])
      mDownDisabled = false;
   else if(keyCode == keyLEFT[inputMode])
      mLeftDisabled = false;
   else if(keyCode == keyRIGHT[inputMode])
      mRightDisabled = false;
}

// Return current move (actual move processing in ship.cpp)
// Will also transform move into "relative" mode if needed
// Note that all input supplied here will be overwritten if
// we are using a game controller.
Move *GameUserInterface::getCurrentMove()
{
   // (Possible modes = PlayMode, ChatMode, QuickChatMode, LoadoutMode)

   if((mCurrentMode == LoadoutMode || mCurrentMode == PlayMode || mCurrentMode == QuickChatMode ) && !gDisableShipKeyboardInput)
   {
      InputMode inputMode = gIniSettings.inputMode;
      mCurrentMove.up = !mUpDisabled && getKeyState(keyUP[inputMode]) ? 1 : 0;
      mCurrentMove.down = !mDownDisabled && getKeyState(keyDOWN[inputMode]) ? 1 : 0;
      mCurrentMove.left = !mLeftDisabled && getKeyState(keyLEFT[inputMode]) ? 1 : 0;
      mCurrentMove.right = !mRightDisabled && getKeyState(keyRIGHT[inputMode]) ? 1 : 0;

      mCurrentMove.fire = mFiring;

      for(U32 i = 0; i < ShipModuleCount; i++)
         mCurrentMove.module[i] = mModActivated[i];
   }
   else
   {
      mCurrentMove.up = 0;
      mCurrentMove.down = 0;
      mCurrentMove.left = 0;
      mCurrentMove.right = 0;

      mCurrentMove.fire = mFiring;     // should be false?

      for(U32 i = 0; i < ShipModuleCount; i++)
         mCurrentMove.module[i] = false;
   }


   if(!gIniSettings.controlsRelative)
      return &mCurrentMove;

   else     // Using relative controls -- all turning is done relative to the direction of the ship.
   {
      mTransformedMove = mCurrentMove;

      Point moveDir(mCurrentMove.right - mCurrentMove.left,
                    mCurrentMove.up - mCurrentMove.down);

      Point angleDir(cos(mCurrentMove.angle), sin(mCurrentMove.angle));

      Point rightAngleDir(-angleDir.y, angleDir.x);
      Point newMoveDir = angleDir * moveDir.y + rightAngleDir * moveDir.x;

      if(newMoveDir.x > 0)
      {
         mTransformedMove.right = newMoveDir.x;
         mTransformedMove.left = 0;
      }
      else
      {
         mTransformedMove.right = 0;
         mTransformedMove.left = -newMoveDir.x;
      }
      if(newMoveDir.y > 0)
      {
         mTransformedMove.down = newMoveDir.y;
         mTransformedMove.up = 0;
      }
      else
      {
         mTransformedMove.down = 0;
         mTransformedMove.up = -newMoveDir.y;
      }
      if(mTransformedMove.right > 1)
         mTransformedMove.right = 1;
      if(mTransformedMove.left > 1)
         mTransformedMove.left = 1;
      if(mTransformedMove.up > 1)
         mTransformedMove.up = 1;
      if(mTransformedMove.down > 1)
         mTransformedMove.down = 1;

      return &mTransformedMove;
   }
}


// User has finished entering a chat message and pressed <enter>
void GameUserInterface::issueChat()
{
   if(mChatBuffer[0])
   {
      // Check if chat buffer holds a message or a command
      if(mChatBuffer[0] != '/' && mCurrentChatType != CmdChat)     // It's a normal chat message
      {
         GameType *gt = gClientGame->getGameType();
         if(gt)
            gt->c2sSendChat(mCurrentChatType == GlobalChat, mChatBuffer);   // Broadcast message
      }
      else                          // It's a command
      {
         Vector<string> words = parseString(mChatBuffer);
         processCommand(words);
      }
   }
   cancelChat();
}


Vector<string> GameUserInterface::parseString(char buffer[])
{
   // Parse the string
   string word = "";
   Vector<string> words;

   S32 startIndex = (mCurrentChatType == CmdChat) ? 0 : 1;  // Start at 1 to omit the leading /

   for(size_t i = startIndex; i < strlen(mChatBuffer); i++)   
   {
      if(mChatBuffer[i] != ' ')
         word += words.size() == 0 ? tolower(mChatBuffer[i]) : mChatBuffer[i];      // Make first word all lower case for case insensitivity
      else if(word != "")
      {
         words.push_back(word);
         word = "";
      }
   }
   if(word != "")
      words.push_back(word);

   return words;
}


extern md5wrapper md5;
extern bool gDebugShowShipCoords;

// Process a command entered at the chat prompt
// Make sure any commands listed here are also included in mChatCmds for auto-completion purposes...
void GameUserInterface::processCommand(Vector<string> words)
{
   if(words.size() == 0)            // Just in case
      return;

   GameConnection *gc = gClientGame->getConnectionToServer();
   if(!gc)
   {
      displayMessage(gCmdChatColor, "!!! Not connected to server");
      return;
   }

   if(words[0] == "add")            // Add time to the game
   {
      if(words.size() < 2 || words[1] == "")
      {
         displayMessage(gCmdChatColor, "!!! Need to supply a time (in minutes)");
         return;
      }

      U8 mins;    // Use U8 to limit number of mins that can be added, while nominally having no limit!
      // Parse 2nd arg -- if first digit isn't a number, user probably screwed up.  
      // atoi will return 0, but this probably isn't what the user wanted.
      
      bool err = false;
      if(words[1][0] >= '0' && words[1][0] <= '9')
         mins = atoi(words[1].c_str());
      else
         err = true;

      if(err || mins == 0)
      {
         displayMessage(gCmdChatColor, "!!! Invalid value... game time not changed");
         return;
      }
      
      displayMessage(gCmdChatColor, "Extended game by %d minute%s", mins, (mins == 1) ? "" : "s");

      if(gClientGame->getGameType())
         gClientGame->getGameType()->addTime(mins * 60 * 1000);
   }

   else if(words[0] == "next")      // Go to next level
   {
      if(!gc->isLevelChanger())
      {
         displayMessage(gCmdChatColor, "!!! You don't have permission to change levels");
         return;
      }

      //if(words.size() < 2 || words[1] == "")
         gc->c2sRequestLevelChange(1, true);
   }

   else if(words[0] == "prev")      // Go to previous level
   {
      if(!gc->isLevelChanger())
      {
         displayMessage(gCmdChatColor, "!!! You don't have permission to change levels");
         return;
      }

      //if(words.size() < 2 || words[1] == "")
         gc->c2sRequestLevelChange(-1, true);
   }

   else if(words[0] == "restart")      // Restart current level
   {
      if(!gc->isLevelChanger())
      {
         displayMessage(gCmdChatColor, "!!! You don't have permission to change levels");
         return;
      }

      //if(words.size() < 2 || words[1] == "")
         gc->c2sRequestLevelChange(-2, false);
   }

   else if(words[0] == "kick")      // Kick a player
   {
      if(!gc->isAdmin())
      {
         displayMessage(gCmdChatColor, "!!! You don't have permission to kick players");
         return;
      }
      if(words.size() < 2 || words[1] == "")
      {
         displayMessage(gCmdChatColor, "!!! Need to specify who to kick");
         return;
      }
      // Did user provide a valid, known name?
      if(!gClientGame->getGameType())
         return;

      ClientRef *clientRef = gClientGame->getGameType()->findClientRef(words[1].c_str());
      if(!clientRef)
      {
         displayMessage(gCmdChatColor, "!!! Could not find player %s", words[1].c_str());
         return;
      }

      gc->c2sAdminPlayerAction(words[1].c_str(), PlayerMenuUserInterface::Kick, 0);     // Team doesn't matter with kick!
   }

   else if(words[0] == "admin")     // Request admin permissions
   {
      if(words.size() < 2 || words[1] == "")
      {
         displayMessage(gCmdChatColor, "!!! Need to supply a password");
         return;
      }
      if(gc->isAdmin())
      {
         displayMessage(gCmdChatColor, "!!! You are already an admin");
         return;
      }

      gc->submitAdminPassword(words[1].c_str());
   }

   else if(words[0] == "levpass" || words[0] == "levelpass" || words[0] == "lvlpass")
   {
      if(words.size() < 2 || words[1] == "")
      {
         displayMessage(gCmdChatColor, "!!! Need to supply a password");
         return;
      }
      if(gc->isLevelChanger())
      {
         displayMessage(gCmdChatColor, "!!! You can already change levels");
         return;
      }

      gc->submitLevelChangePassword(words[1].c_str());
   }
   else if(words[0] == "dcoords")
         gDebugShowShipCoords = !gDebugShowShipCoords;
   else if(words[0] == "svol")      // SFX volume
      setVolume(SfxVolumeType, words);
   else if(words[0] == "mvol")      // Music volume
      setVolume(MusicVolumeType, words);
   else if(words[0] == "vvol")      // Voice chat volume
      setVolume(VoiceVolumeType, words);
   else if(words[0] == "servvol")   // Server alerts volume
      setVolume(ServerAlertVolumeType, words);

   else
      displayMessage(gCmdChatColor, "!!! Invalid command: %s", words[0].c_str());
}


// For auto-completion purposes
void GameUserInterface::populateChatCmdList()
{
   // Our list of commands that can be entered at the chat prompt
   mChatCmds.push_back("/add");
   mChatCmds.push_back("/admin");
   mChatCmds.push_back("/dcoords");
   mChatCmds.push_back("/kick");
   mChatCmds.push_back("/levpass");
   mChatCmds.push_back("/mvol");
   mChatCmds.push_back("/next");
   mChatCmds.push_back("/prev");
   mChatCmds.push_back("/restart");
   mChatCmds.push_back("/svol");
   mChatCmds.push_back("/servvol");
   mChatCmds.push_back("/vvol");
}

// Set specified volume to the specefied level
void GameUserInterface::setVolume(VolumeType volType, Vector<string> words)
{
   S32 vol;

   if(words.size() < 2)
   {
      displayMessage(gCmdChatColor, "!!! Need to specify volume");
      return;
   }  

   string volstr = words[1];

   // Parse volstr -- if first digit isn't a number, user probably screwed up.  
   // atoi will return 0, but this probably isn't what the user wanted.
   if(volstr[0] >= '0' && volstr[0] <= '9')
      vol = max(min(atoi(volstr.c_str()), 10), 0);
   else
   {
      displayMessage(gCmdChatColor, "!!! Invalid value... volume not changed");
      return;
   }

  switch(volType)
  {
   case SfxVolumeType:
      gIniSettings.sfxVolLevel = (F32) vol / 10.0;
      displayMessage(gCmdChatColor, "SFX volume changed to %d %s", vol, vol == 0 ? "[MUTE]" : "");
      return;
   case MusicVolumeType:
      gIniSettings.musicVolLevel = (F32) vol / 10.0;
      displayMessage(gCmdChatColor, "Music volume changed to %d %s", vol, vol == 0 ? "[MUTE]" : "");
      return;
   case VoiceVolumeType:
      gIniSettings.voiceChatVolLevel = (F32) vol / 10.0;
      displayMessage(gCmdChatColor, "Voice chat volume changed to %d %s", vol, vol == 0 ? "[MUTE]" : "");
      return;
   case ServerAlertVolumeType:
      gClientGame->getConnectionToServer()->c2sSetServerAlertVolume((S8) vol);
      displayMessage(gCmdChatColor, "Server alerts chat volume changed to %d %s", vol, vol == 0 ? "[MUTE]" : "");
      return;

  }
}


void GameUserInterface::cancelChat()
{
   memset(mChatBuffer, 0, sizeof(mChatBuffer));
   mChatCursorPos = 0;
   setPlayMode();
}


// Constructor
GameUserInterface::VoiceRecorder::VoiceRecorder()
{
   mRecordingAudio = false;
   mMaxAudioSample = 0;
   mMaxForGain = 0;
   mVoiceEncoder = new LPC10VoiceEncoder;
}

void GameUserInterface::VoiceRecorder::idle(U32 timeDelta)
{
   if(mRecordingAudio)
   {
      if(mVoiceAudioTimer.update(timeDelta))
      {
         mVoiceAudioTimer.reset(VoiceAudioSampleTime);
         process();
      }
   }
}

void GameUserInterface::VoiceRecorder::render()
{
   if(mRecordingAudio)
   {
      F32 amt = mMaxAudioSample / F32(0x7FFF);
      U32 totalLineCount = 50;

      glColor3f(1, 1 ,1);
      glBegin(GL_LINES);
      glVertex2f(10, 130);
      glVertex2f(10, 145);
      glVertex2f(10 + totalLineCount * 2, 130);
      glVertex2f(10 + totalLineCount * 2, 145);

      F32 halfway = totalLineCount * 0.5;
      F32 full = amt * totalLineCount;
      for(U32 i = 1; i < full; i++)
      {
         if(i < halfway)
            glColor3f(i / halfway, 1, 0);
         else
            glColor3f(1, 1 - (i - halfway) / halfway, 0);

         glVertex2f(10 + i * 2, 130);
         glVertex2f(10 + i * 2, 145);
      }
      glEnd();
   }
}

void GameUserInterface::VoiceRecorder::start()
{
   if(!mRecordingAudio)
   {
      mRecordingAudio = SFXObject::startRecording();
      if(!mRecordingAudio)
         return;

      mUnusedAudio = new ByteBuffer(0);
      mRecordingAudio = true;
      mMaxAudioSample = 0;
      mVoiceAudioTimer.reset(FirstVoiceAudioSampleTime);

      // trim the start of the capture buffer:
      SFXObject::captureSamples(mUnusedAudio);
      mUnusedAudio->resize(0);
   }
}

void GameUserInterface::VoiceRecorder::stop()
{
   if(mRecordingAudio)
   {
      process();

      mRecordingAudio = false;
      SFXObject::stopRecording();
      mVoiceSfx = NULL;
      mUnusedAudio = NULL;
   }
}

void GameUserInterface::VoiceRecorder::process()
{
   U32 preSampleCount = mUnusedAudio->getBufferSize() / 2;
   SFXObject::captureSamples(mUnusedAudio);

   U32 sampleCount = mUnusedAudio->getBufferSize() / 2;
   if(sampleCount == preSampleCount)
      return;

   S16 *samplePtr = (S16 *) mUnusedAudio->getBuffer();
   mMaxAudioSample = 0;

   for(U32 i = preSampleCount; i < sampleCount; i++)
   {
      if(samplePtr[i] > mMaxAudioSample)
         mMaxAudioSample = samplePtr[i];
      else if(-samplePtr[i] > mMaxAudioSample)
         mMaxAudioSample = -samplePtr[i];
   }
   mMaxForGain = U32(mMaxForGain * 0.95f);
   S32 boostedMax = mMaxAudioSample + 2048;
   if(boostedMax > mMaxForGain)
      mMaxForGain = boostedMax;
   if(mMaxForGain > MaxDetectionThreshold)
   {
      // apply some gain to the buffer:
      F32 gain = 0x7FFF / F32(mMaxForGain);
      for(U32 i = preSampleCount; i < sampleCount; i++)
      {
         F32 sample = gain * samplePtr[i];
         if(sample > 0x7FFF)
            samplePtr[i] = 0x7FFF;
         else if(sample < -0x7FFF)
            samplePtr[i] = -0x7FFF;
         else
            samplePtr[i] = S16(sample);
      }
      mMaxAudioSample = U32(mMaxAudioSample * gain);
   }

   ByteBufferPtr sendBuffer = mVoiceEncoder->compressBuffer(mUnusedAudio);

   if(sendBuffer.isValid())
   {
      GameType *gt = gClientGame->getGameType();
      if(gt)
         gt->c2sVoiceChat(gIniSettings.echoVoice, sendBuffer);
   }
}

};
