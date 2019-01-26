#ifndef __FWEELIN_SDLIO_H
#define __FWEELIN_SDLIO_H

/* Copyright 2004-2011 Jan Pekau
   
   This file is part of Freewheeling.
   
   Freewheeling is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.
   
   Freewheeling is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with Freewheeling.  If not, see <http://www.gnu.org/licenses/>. */

#include <pthread.h>

#include "fweelin_event.h"

#ifdef __MACOSX__
#include "FweelinMac.h"
#endif

class Fweelin;

// Deprecated!
class KeySettings {
 public:
  KeySettings() :
    leftshift(0), rightshift(0), leftctrl(0), rightctrl(0),
    leftalt(0), rightalt(0), upkey(0), downkey(0), spacekey(0) {};

  // Function modifier keys
  char leftshift,
    rightshift,
    leftctrl,
    rightctrl,
    leftalt,
    rightalt,
    upkey,
    downkey,
    spacekey;
};

class SDLKeyList {
 public:
  SDLKeyList (SDLKey k) : k(k), next(0) {};

  SDLKey k;
  SDLKeyList *next;
};

// SDL Input Handler
class SDLIO : public EventProducer, public EventListener {
public:
  SDLIO (Fweelin *app) : app(app), sdlthreadgo(0) {
    keyheld = new char[SDLK_LAST];
    for (int i = 0; i < SDLK_LAST; i++)
      keyheld[i] = 0;
  };
  virtual ~SDLIO() { 
    delete[] keyheld;
  };

  int activate ();
  void close ();

  char IsActive () { return sdlthreadgo; };
  char *GetKeysHeld () { return keyheld; };

  KeySettings *getSETS() { return &sets; };

  void ReceiveEvent(Event *ev, EventProducer */*from*/);

  // We use slightly modified keynames for the config system:
  // Gets the SDL keysym that corresponds to key with a given name
  static SDLKey GetSDLKey(char *keyname);
  // And the name corresponding to the keysym..
  static const char *GetSDLName(SDLKey sym);

  inline void EnableUNICODE(int enable) { SDL_EnableUNICODE(enable); };
  inline void EnableKeyRepeat(int enable) { 
    if (enable)
      SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
                          SDL_DEFAULT_REPEAT_INTERVAL);
    else 
      SDL_EnableKeyRepeat(0,0);
  };

  // SDL event handler thread
  static void *run_sdl_thread (void *ptr);

protected:
#ifdef __MACOSX__
  FweelinMac cocoa; // Cocoa thread setup
#endif

  // Core app
  Fweelin *app;

  // Joysticks
  int numjoy;           // Number of joysticks
  SDL_Joystick **joys;  // Control structure for each joystick

  // Keys currently held down- array for all SDLKeys, nonzero if held
  char *keyheld;

  // Deprecated
  KeySettings sets;

  void handle_key(int keycode, char press);

  pthread_t sdl_thread;
  char sdlthreadgo;
};

#endif
