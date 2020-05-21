//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UICredits.h"

#include "UIManager.h"

#include "gameObjectRender.h"    // For renderBitfighterLogo
#include "DisplayManager.h"
#include "Colors.h"

#include "RenderUtils.h"
#include "OpenglUtils.h"
#include "SoundSystem.h"

#include <stdio.h>
#include <math.h>
#include "FontManager.h"

namespace Zap
{

static const char *gameCredits[] = {
   "Developed by:",
   "Chris Eykamp (watusimoto)",
   "David Buck (raptor)",
   "Samuel Williams (sam686)",
   "Bryan Conrad (kaen)",
   "-",
   "Originally based on the Zap demo in OpenTNL",
   "-",
   "Mac support:",
   "Vittorio Giovara (koda)",
   "Ryan Witmer",
   "Max Hushahn (Zoomber)",
   "-",
   "Linux support:",
   "David Buck (raptor)",
   "Coding_Mike",
   "Janis Rucis",
   "-",
   "Level contributions:",
   "Qui",
   "Pierce Youatt (karamazovapy)",
   "Riordan Zentler (Quartz)",
   "-",
   "Bot development:",
   "Samuel Williams (sam686)",
   "Joseph Ivie (Unknown)",
   "-",
   "Web development:",
   "Bryan Conrad (kaen)",
   "-",
   "Testing and ideas:",
   "Pierce Youatt (karamazovapy)",
   "Jonathan Hansen (bobdaduck)",
   "-",
   "Google Code-In 2013 students",
   "Alex Velea",
   "Dariel Kremov",
   "Geoffrey Mon",
   "grypoB",
   "Jacob Logan",
   "Joey Watts",
   "JohnAudi",
   "Mandar Juvekar",
   "Ondrej Skopek",
   "Phillip Reeder",
   "Tomek Zalewski",
   "-",
   "Music:",
   "Andreas Viklund",
   "Chris Neal",
   "vovk50",
   "United States Marine Band",
   "-",
   "Join us",
   "at",
   "bitfighter.org",
   "-",                    // Need to end with this...
   NULL                    // ...then this
};


static bool quitting = false;

// Constructor
CreditsUserInterface::CreditsUserInterface(ClientGame *game) : Parent(game)
{
   mScroller = new CreditsScroller();
}


// Destructor (only gets run when we quit the game!)
CreditsUserInterface::~CreditsUserInterface()
{
   delete mScroller;
   mScroller = NULL;
}


void CreditsUserInterface::onActivate()
{
   quitting = false;

   // Show splash animation at beginning of credits
   getUIManager()->activate<SplashUserInterface>();

   mScroller->setActive(true);
}


void CreditsUserInterface::onReactivate()
{
   if(quitting)
      quit();     
}


void CreditsUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   if(mScroller->isActive())
      mScroller->updateFX(timeDelta);
}


void CreditsUserInterface::render()
{
   if(mScroller->isActive())
      mScroller->render();

   if(quitting)
   {
      quitting = false;
      quit();
   }
}


void CreditsUserInterface::quit()
{
   mScroller->resetPosition();

   getUIManager()->reactivatePrevUI();      // gMainMenuUserInterface
}


bool CreditsUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;
   else
      quit();     // Quit the interface when any key is pressed...  any key at all.  Except those handled by Parent.

   return false;
}


// Constructor
CreditsScroller::CreditsScroller()
{
   glLineWidth(gDefaultLineWidth);

   // Loop through each line in the credits looking for section breaks ("-")
   // thus creating groups, the first of which is generally the job, followed
   // by 0 or more people doing that job

   CreditsInfo c;    // Reusable object, gets implicitly copied when pushed
   S32 index = 0;
   while(gameCredits[index])
   {
      // If we've hit a section break, add the current CreditsInfo to the list
      if(strcmp(gameCredits[index], "-") == 0)
      {
         mCredits.push_back(c);
         c.lines.clear();
      }
      // Else add the current line to the current CreditsInfo
      else
         c.lines.push_back(gameCredits[index]);

      index++;
   }

   // Start credits position at the appropriate place
   resetPosition();

   mActivated = false;
}


// Destructor
CreditsScroller::~CreditsScroller()
{
   // Do nothing
}

void CreditsScroller::updateFX(U32 delta)
{
   static bool creditsMusicExists = false;
   static S32 delayTimer = 4000;

   // If the second-to-last credits has gone of the screen, don't update anymore.  This
   // leaves the final message drawn on the screen.
   U32 indexMinus1 = mCredits.size() - 2;
   if(mCredits[indexMinus1].pos > 150 - (CreditSpace * mCredits[indexMinus1].lines.size()))  // 150 = banner height
   {
      // Scroll the credits text from bottom to top
      for(S32 i = 0; i < mCredits.size(); i++)
         mCredits[i].pos -= (delta / 8.f);

      // Test if credit music is playing - this just picks an arbitrary time to test if the music loaded properly
      if(!creditsMusicExists && mCredits[indexMinus1].pos > DisplayManager::getScreenInfo()->getGameCanvasHeight() && SoundSystem::isMusicPlaying())
         creditsMusicExists = true;
   }
   else
   {
      delayTimer -= delta;

      // We will exit when the music has stopped, or the delay timer runs out
      if((creditsMusicExists && !SoundSystem::isMusicPlaying()) || (!creditsMusicExists && delayTimer < 0))
      {
         // Reset statics
         creditsMusicExists = false;
         delayTimer = 4000;

         // Quit
         quitting = true;
      }
   }
}


void CreditsScroller::render()
{
   FontManager::pushFontContext(MenuContext);
   glColor(Colors::white);

   // Draw the credits text, section by section, line by line
   for(S32 i = 0; i < mCredits.size(); i++)
      for(S32 j = 0; j < mCredits[i].lines.size(); j++)
         drawCenteredString(S32(mCredits[i].pos) + CreditSpace * (j), 25, mCredits[i].lines[j]);

   glColor(Colors::black);
   F32 vertices[] = {
         0, 0,
         0, 150,
         (F32)DisplayManager::getScreenInfo()->getGameCanvasWidth(), 150,
         (F32)DisplayManager::getScreenInfo()->getGameCanvasWidth(), 0
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, zGL_TRIANGLE_FAN);

   renderStaticBitfighterLogo();    // And add our logo at the top of the page

   FontManager::popFontContext();
}


void CreditsScroller::resetPosition()
{
   mCredits[0].pos = (F32)DisplayManager::getScreenInfo()->getGameCanvasHeight();

   for(S32 i = 1; i < mCredits.size(); i++)
      mCredits[i].pos = mCredits[i-1].pos + (CreditSpace * mCredits[i-1].lines.size()) + SectionSpace;
}


void CreditsScroller::setActive(bool active)
{
   mActivated = active;
}


bool CreditsScroller::isActive() const
{
   return mActivated;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SplashUserInterface::SplashUserInterface(ClientGame *game) : Parent(game)
{
   mPhase = SplashPhaseNone;
}

// Destructor
SplashUserInterface::~SplashUserInterface()
{
   // Do nothing
}


void SplashUserInterface::onActivate()
{
   mSplashTimer.reset(1500);     // Time of main animation
   mPhase = SplashPhaseAnimation;
}


void SplashUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   if(mSplashTimer.update(timeDelta))
   {
      mPhase = SplashPhase(S32(mPhase) + 1);

      if(mPhase == SplashPhaseResting)
         mSplashTimer.reset(150);      // Brief pause after main animation, before rising
      else if(mPhase == SplashPhaseRising)
         mSplashTimer.reset(800);      // Phase during which logo rises to the top
   }

   if(mPhase >= SplashPhaseDone)
      quit();
}


void SplashUserInterface::render()
{
   if(mPhase == SplashPhaseAnimation)             // Main animation phase
   {
      glColor(0, mSplashTimer.getFraction(), 1);

      F32 fr = pow(mSplashTimer.getFraction(), 2);

      S32 ctr = DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2;

      renderBitfighterLogo(ctr, fr * 20.0f + 1, 1 << 0);
      renderBitfighterLogo(ctr, fr * 50.0f + 1, 1 << 1);
      renderBitfighterLogo(ctr, fr * 10.0f + 1, 1 << 2);
      renderBitfighterLogo(ctr, fr *  2.0f + 1, 1 << 3);
      renderBitfighterLogo(ctr, fr * 14.0f + 1, 1 << 4);
      renderBitfighterLogo(ctr, fr *  6.0f + 1, 1 << 5);
      renderBitfighterLogo(ctr, fr * 33.0f + 1, 1 << 6);
      renderBitfighterLogo(ctr, fr *  9.0f + 1, 1 << 7);
      renderBitfighterLogo(ctr, fr * 30.0f + 1, 1 << 8);
      renderBitfighterLogo(ctr, fr * 15.0f + 1, 1 << 9);
   }
   else if(mPhase == SplashPhaseResting)          // Resting phase
   {
      glColor(Colors::blue);
      renderBitfighterLogo(DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2, 1);
   }
   else if(mPhase == SplashPhaseRising)           // Rising phase
   {
      glColor(0, sqrt(1 - mSplashTimer.getFraction()), 1 - pow(1 - mSplashTimer.getFraction(), 2));
      renderBitfighterLogo((S32)(73.0f + ((F32) DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2.0f - 73.0f) * mSplashTimer.getFraction()), 1);
   }
}


void SplashUserInterface::quit()
{
   getUIManager()->reactivatePrevUI();      //gMainMenuUserInterface
}


bool SplashUserInterface::onKeyDown(InputCode inputCode)
{
   if(!Parent::onKeyDown(inputCode))
   {
      quitting = true;
      quit();                              // Quit the interface when any key is pressed...  any key at all.  Almost.

      // Unless user hit Enter or Escape, or some other thing...
      if(inputCode != KEY_ESCAPE && inputCode != KEY_ENTER && inputCode != MOUSE_LEFT && inputCode != MOUSE_MIDDLE && inputCode != MOUSE_RIGHT)    
         getUIManager()->getCurrentUI()->onKeyDown(inputCode); // ...pass keystroke on  (after reactivate in quit(), current is now the prev

      if(inputCode == MOUSE_LEFT && inputCode == MOUSE_MIDDLE && inputCode == MOUSE_RIGHT)
         getUIManager()->getCurrentUI()->onMouseMoved();
   }

   return true;
}


};


