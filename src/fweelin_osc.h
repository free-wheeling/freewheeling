#ifndef __FWEELIN_OSC_H
#define __FWEELIN_OSC_H

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

#ifndef __MACOSX__

#include <lo/lo.h>

class Fweelin;

#define QTRACTOR_OSC_PORT 5000

class OSCClient : public EventListener {
  
public:
  OSCClient (Fweelin *app);
  virtual ~OSCClient ();

  void ReceiveEvent(Event *ev, EventProducer */*from*/);

protected:
  // Core app
  Fweelin *app;

  // Opens a connection to the Qtractor DAW, for transmitting
  // loops.
  //
  // Returns zero on success.
  char open_qtractor_connection ();

  // Send all playing loops to a DAW via OSC
  void SendPlayingLoops();

  // Qtractor interface
  lo_address qtractor_addr;
  //

  pthread_mutex_t osc_client_lock;
};

#endif

#endif
