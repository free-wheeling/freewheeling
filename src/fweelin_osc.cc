/*
   Recovery is a long process--

   1 day
   at
   a
   time
*/

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

#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include <string>
#include <sstream>

#include "fweelin_core.h"
#include "fweelin_osc.h"
#include "fweelin_looplibrary.h"

OSCClient::OSCClient(Fweelin *app) : app(app), qtractor_addr(0) {
  printf("OSC: Start.\n");

  // Init mutex/conditions
  pthread_mutex_init(&osc_client_lock,0);

  app->getEMG()->ListenEvent(this,0,T_EV_TransmitPlayingLoopsToDAW);
};

OSCClient::~OSCClient() {
  printf("OSC: End.\n");

  if (qtractor_addr != 0)
    lo_address_free(qtractor_addr);

  app->getEMG()->UnlistenEvent(this,0,T_EV_TransmitPlayingLoopsToDAW);

  pthread_mutex_destroy (&osc_client_lock);
}

void OSCClient::ReceiveEvent(Event *evt, EventProducer */*from*/) {
  switch (evt->GetType()) {
  case T_EV_TransmitPlayingLoopsToDAW :
    {
      // OK!
      if (CRITTERS)
        printf("OSC: Received TransmitPlayingLoopsToDAWEvent\n");

      SendPlayingLoops();
    }
    break;

  default:
    break;
  }
}

void OSCClient::SendPlayingLoops() {
  pthread_mutex_lock(&osc_client_lock);

  if (!open_qtractor_connection()) {
    // Get long count
    Pulse *p = 0;
    int lc = app->getLOOPMGR()->GetLongCountForAllPlayingLoops(p);
    nframes_t max_len = 0;

    // Set QTractor tempo based on pulse
    if (p != 0) {
      nframes_t bar_len;
      int sync_speed = (app->GetSyncSpeed() > 0 ? app->GetSyncSpeed() : 1),
          beats_per_bar = 4;

      if (!app->GetSyncType())
        // Bar-sync, one pulse represents several bars
        bar_len = p->GetLength()/sync_speed;
      else {
        // Beat-sync, one pulse represents one bar of several beats
        bar_len = p->GetLength();
        beats_per_bar = sync_speed;
      }

      // frames/sec * beats/bar / (frames/bar) == beats / sec
      float tempo = 60. * (float) app->getAUDIO()->get_srate() * beats_per_bar / bar_len;
      printf("OSC: Send tempo: %f, %d beats per bar - from pulse & sync settings\n",tempo,beats_per_bar);

      if (lo_send(qtractor_addr, "/SetGlobalTempo", "fi", tempo, beats_per_bar) == -1) {
        printf("OSC: Error %d: %s\n", lo_address_errno(qtractor_addr), lo_address_errstr(qtractor_addr));
      }
    }

    // Scan all loops for playing loops
    for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) {
      if (app->getLOOPMGR()->GetStatus(i) == T_LS_Playing) {
        Loop *l = app->getTMAP()->GetMap(i);
        if (l != 0 && l->GetSaveStatus() == SAVE_DONE) {
          // Saved loop. Get filename
          const std::string s = LibraryHelper::GetStubnameFromLoop(app,l);

          LibraryFileInfo loopfile = LibraryHelper::GetLoopFilenameFromStub(app,s.c_str());
          if (loopfile.exists) {
            char buf[PATH_MAX + 1];
            char *res = realpath(loopfile.name.c_str(), buf);

            if (res != 0) {
              // Do not send overlapping regions- this seems to cause clicks in QTractor.
              // Region ends look like fadeout then fadein (currently).
              nframes_t cflen = app->getAUDIO()->getbufsz() * 2,
                  len = app->getLOOPMGR()->GetRoundedLength(i); //  + cflen;
              float gain = l->vol;

              printf("OSC: Transfer loop: %s to Qtractor (nbeats: %d, lc: %d, cflen: %d, len: %d, gain: %0.2f)\n",
                  buf,(int) l->nbeats,lc,cflen,len,gain);

              // Fill pattern with reps
              int nbeats = (l->pulse != 0 && l->nbeats > 0 ? l->nbeats : lc);  // Only 1 rep if there is no pulse
              nframes_t curstart = 0;

              for (int reps = 0; reps < lc; reps += nbeats, curstart += app->getLOOPMGR()->GetRoundedLength(i)) {
                printf("OSC: Send loop @ %d\n",(int) curstart);
                if (lo_send(qtractor_addr, "/AddAudioClipOnUniqueTrack", "iiiifs",
                    (int) curstart, 0, len, cflen, gain, buf) == -1) {
                  printf("OSC: Error %d: %s\n", lo_address_errno(qtractor_addr), lo_address_errstr(qtractor_addr));
                }
                if (curstart + len > max_len)
                  max_len = curstart + len; // Length of pattern in frames
              }
            } else
              printf("OSC: Can't get path for loop: %s\n",loopfile.name.c_str());
          } else
            printf("OSC: Can't find loop: %s\n",s.c_str());
        }
      }
    }

    // Update loop point
    if (lo_send(qtractor_addr, "/AdvanceLoopRange", "ii", 0, max_len) == -1) {
      printf("OSC: Error %d: %s\n", lo_address_errno(qtractor_addr), lo_address_errstr(qtractor_addr));
    }
  } else {
    printf("OSC: Couldn't open an OSC connection to QTractor on port %d\n",QTRACTOR_OSC_PORT);
  }

  pthread_mutex_unlock(&osc_client_lock);
}

// Open or refresh connection to qtractor
char OSCClient::open_qtractor_connection() {
  if (qtractor_addr != 0)
    lo_address_free(qtractor_addr);

  char portbuf[256];
  snprintf(portbuf,255,"%d",QTRACTOR_OSC_PORT);
  qtractor_addr = lo_address_new(NULL, portbuf);

  return (qtractor_addr == 0);
}

#endif
