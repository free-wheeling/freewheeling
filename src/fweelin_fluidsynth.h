#ifndef __FWEELIN_FLUIDSYNTH_H
#define __FWEELIN_FLUIDSYNTH_H

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


#if USE_FLUIDSYNTH

#include <fluidsynth.h>

#include "fweelin_core_dsp.h"
#include "fweelin_browser.h"

// Whacky FluidSynth center of pitchbend is 0x2000 not 0
#define FLUIDSYNTH_PITCHBEND_CENTER 0x2000

// Integrated soft-synth based on libfluidsynth
class FluidSynthProcessor : public Processor, public EventListener {
  friend class Fweelin;

public:
  FluidSynthProcessor(Fweelin *app, char stereo);
  virtual ~FluidSynthProcessor();

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);

  virtual void ReceiveEvent(Event *ev, EventProducer */*from*/);

  // Send a new patch to synth
  void SendPatchChange(PatchItem *p);

  // Receive MIDI events for synth
  void ReceiveMIDIEvent(Event *ev);

  // Enable/disable FluidSynth- if disabled, bypasses
  // processor stage to reduce CPU usage, but leaves memory allocated
  inline void SetEnable(char en) { 
    // Preprocess audio for smoothing
    //dopreprocess();
    this->enable = en;
  };

  char GetEnable() { return enable; };

  // Sets up browser patches based on loaded soundfonts
  void SetupPatches();

 private:

  // Run FluidSynth in stereo?
  char stereo;

  // Left and right output buffers
  sample_t *leftbuf, 
    *rightbuf;

  // And fluidsynth variables..
  fluid_settings_t *settings;
  fluid_synth_t *synth;

  // Currently processing?
  char enable;
};

#endif
#endif
