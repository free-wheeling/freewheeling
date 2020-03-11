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

#include "fweelin_fluidsynth.h"


void FluidSynthParam_Int::Send(fluid_settings_t *settings) {
  printf("FLUID: Setting parameter '%s' = '%d'\n",name,val);
  fluid_settings_setint(settings, name, val);
};

void FluidSynthParam_Num::Send(fluid_settings_t *settings) {
  printf("FLUID: Setting parameter '%s' = '%f'\n",name,val);
  fluid_settings_setnum(settings, name, val);
};

void FluidSynthParam_Str::Send(fluid_settings_t *settings) {
  printf("FLUID: Setting parameter '%s' = '%s'\n",name,val);
  fluid_settings_setstr(settings, name, val);
};

FluidSynthProcessor::FluidSynthProcessor(Fweelin *app, char stereo) : 
  Processor(app), stereo(stereo), enable(1) {
  nframes_t bufsz = app->getBUFSZ();
  leftbuf = new sample_t[bufsz];
  rightbuf = new sample_t[bufsz];

  // Setup FluidSynth
  settings = new_fluid_settings();
  {
    FluidSynthParam *cur = app->getCFG()->GetFluidParam();
    while (cur != 0) {
      cur->Send(settings);
      cur = cur->next;
    }
  }
  // Set sampling rate
  fluid_settings_setnum(settings, "synth.sample-rate", 
                        app->getAUDIO()->get_srate());  
  // Create synth
  synth = new_fluid_synth(settings);

  // Set interpolation quality
  printf("FLUID: Setting interpolation quality: %d\n",
         app->getCFG()->GetFluidInterpolation());
  fluid_synth_set_interp_method(synth,-1,
                                app->getCFG()->GetFluidInterpolation());

  // User-specified global fluidsynth tuning
  float tuning = app->getCFG()->GetFluidTuning();
  if (tuning != 0.0) {
    printf("FLUID: Setting pitch tuning: %.2f cents\n",
           tuning);

    // Create tuning for each key in octave
    double pitches[12];
    for (int i = 0; i < 12; i++)
      pitches[i] = tuning;
    fluid_synth_activate_octave_tuning(synth, 0, 0, "DETUNE", pitches, false);

    // Select tuning
    for (int i = 0; i < MAX_MIDI_CHANNELS; i++) 
      fluid_synth_activate_tuning(synth, i, 0, 0, false);
  } else 
    printf("FLUID: Using default tuning\n");

  // Load soundfonts
  {
    FluidSynthSoundFont *cur = app->getCFG()->GetFluidFont();
    while (cur != 0) {
      printf("FLUID: Loading SoundFont '%s'\n",cur->name);
      fluid_synth_sfload(synth,cur->name,1);
      cur = cur->next;
    }
  }

  app->getEMG()->ListenEvent(this,0,T_EV_FluidSynthEnable);
};

FluidSynthProcessor::~FluidSynthProcessor() {
  app->getEMG()->UnlistenEvent(this,0,T_EV_FluidSynthEnable);

  delete_fluid_synth(synth); 
  delete_fluid_settings(settings);

  delete[] leftbuf;
  delete[] rightbuf;
};

void FluidSynthProcessor::process(char pre, nframes_t len, 
                                  AudioBuffers *ab) {
  if (!pre) {
    // Map our output to the end of the external inputs
    ab->ins[0][ab->numins_ext] = leftbuf;
    if (stereo)
      ab->ins[1][ab->numins_ext] = rightbuf;
    else 
      ab->ins[1][ab->numins_ext] = 0;

    if (enable) {
      // Run synth DSP
      fluid_synth_write_float(synth, len, 
                              leftbuf, 0, 1,
                              rightbuf, 0, 1);
    
      if (!stereo) {
        // Mono, so fold the right channel into the left like pudding
        for (nframes_t l = 0; l < len; l++)
          leftbuf[l] = (leftbuf[l] + rightbuf[l]) / 2;
      }
    } else {
      // Silence, not enabled
      memset(leftbuf,0,sizeof(sample_t) * len);
      if (stereo)
        memset(rightbuf,0,sizeof(sample_t) * len);
    }
  }
};

// Read new current patch from 'patches' and send patch change to synth
void FluidSynthProcessor::SendPatchChange(PatchItem *p) {
  fluid_synth_program_select(synth, p->channel, 
                             p->id, p->bank, p->prog);
};

// Sets up our internal patch list based on loaded soundfonts
void FluidSynthProcessor::SetupPatches() {
  PatchBrowser *br = (PatchBrowser *) app->getBROWSER(B_Patch);
#if FLUIDSYNTH_VERSION_MAJOR == 1
  // NOTE: fluidsynth v2 implements these
  #define fluid_sfont_iteration_start(_sf) (*(_sf)->iteration_start)(_sf)
  #define fluid_sfont_iteration_next(_sf,_pr) (*(_sf)->iteration_next)(_sf,_pr)
  #define fluid_preset_get_name(_preset) (*(_preset)->get_name)(_preset)
  #define fluid_preset_get_banknum(_preset) (*(_preset)->get_banknum)(_preset)
  #define fluid_preset_get_num(_preset) (*(_preset)->get_num)(_preset)
  fluid_preset_t preset;
#elif FLUIDSYNTH_VERSION_MAJOR == 2
  fluid_preset_t* preset;
#endif // FLUIDSYNTH_VERSION_MAJOR

  if (br != 0) {
    // Add FluidSynth patch bank
    int fluidchan = app->getCFG()->GetFluidChannel();
    br->PB_AddBegin(new PatchBank(0,-1,0)); // Use port 0 for FluidSynth
    
    // Store patches
    int sfcnt = fluid_synth_sfcount(synth);
    for (int i = 0; i < sfcnt; i++) {
      fluid_sfont_t *curfont = fluid_synth_get_sfont(synth,i);
      fluid_sfont_iteration_start(curfont);
#if FLUIDSYNTH_VERSION_MAJOR == 1
      while (fluid_sfont_iteration_next(curfont, &preset))
        br->AddItem(new PatchItem(fluid_sfont_get_id(curfont),
                                  fluid_preset_get_banknum(&preset),
                                  fluid_preset_get_num(&preset),
                                  fluidchan,
                                  fluid_preset_get_name(&preset)));
#elif FLUIDSYNTH_VERSION_MAJOR == 2
      while ((preset = fluid_sfont_iteration_next(curfont)) != NULL)
        br->AddItem(new PatchItem(fluid_sfont_get_id(curfont),
                                  fluid_preset_get_banknum(preset),
                                  fluid_preset_get_num(preset),
                                  fluidchan,
                                  fluid_preset_get_name(preset)));
#endif // FLUIDSYNTH_VERSION_MAJOR

      if (i+1 < sfcnt) {
        // End of soundfont- put in a divider
        br->AddItem(new BrowserDivision());
      }
    }
  }
};

void FluidSynthProcessor::ReceiveMIDIEvent(Event *ev) {
  FloConfig *fs = app->getCFG();

  switch (ev->GetType()) {
  case T_EV_Input_MIDIController :
    if (enable)
    {
      MIDIControllerInputEvent *mcev = (MIDIControllerInputEvent *) ev;
      fluid_synth_cc(synth,app->getCFG()->GetFluidChannel(),
                     mcev->ctrl,mcev->val);
    }
    break;

  case T_EV_Input_MIDIPitchBend :
    if (enable)
    {
      MIDIPitchBendInputEvent *mpev = (MIDIPitchBendInputEvent *) ev;
      fluid_synth_pitch_bend(synth,app->getCFG()->GetFluidChannel(),
                             mpev->val + FLUIDSYNTH_PITCHBEND_CENTER);
    }
    break;
      
  case T_EV_Input_MIDIKey :
    if (enable)
    {
      MIDIKeyInputEvent *mkev = (MIDIKeyInputEvent *) ev;
      if (mkev->down)
        fluid_synth_noteon(synth,
                           app->getCFG()->GetFluidChannel(),
                           mkev->notenum+fs->transpose,
                           mkev->vel);
      else
        fluid_synth_noteoff(synth,
                            app->getCFG()->GetFluidChannel(),
                            mkev->notenum+fs->transpose);
    }
    break;

  default:
    break;
  }
};

void FluidSynthProcessor::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_FluidSynthEnable :
    {
      FluidSynthEnableEvent *fev = (FluidSynthEnableEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("FLUID: Received FluidSynthEnable (%s)\n",
               (fev->enable ? "on" : "off"));
      SetEnable(fev->enable);
    }
    break;

  default:
    break;
  }
}

#endif 
