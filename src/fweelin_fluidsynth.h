#ifndef __FWEELIN_FLUIDSYNTH_H
#define __FWEELIN_FLUIDSYNTH_H

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

  virtual void ReceiveEvent(Event *ev, EventProducer *from);

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
