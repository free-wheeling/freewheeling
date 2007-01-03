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

#define fluid_sfont_iteration_start(_sf) (*(_sf)->iteration_start)(_sf)
#define fluid_sfont_iteration_next(_sf,_pr) (*(_sf)->iteration_next)(_sf,_pr)
#define fluid_preset_get_name(_preset) (*(_preset)->get_name)(_preset)
#define fluid_preset_get_banknum(_preset) (*(_preset)->get_banknum)(_preset)
#define fluid_preset_get_num(_preset) (*(_preset)->get_num)(_preset)

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
  fluid_preset_t preset;

  PatchBrowser *br = (PatchBrowser *) app->getBROWSER(B_Patch);

  if (br != 0) {
    // Add FluidSynth patch bank
    int fluidchan = app->getCFG()->GetFluidChannel();
    br->PB_AddBegin(new PatchBank(0,0)); // Use port 0 for FluidSynth
    
    // Store patches
    int sfcnt = fluid_synth_sfcount(synth);
    for (int i = 0; i < sfcnt; i++) {
      fluid_sfont_t *curfont = fluid_synth_get_sfont(synth,i);
      fluid_sfont_iteration_start(curfont);
      while (fluid_sfont_iteration_next(curfont, &preset))
	br->AddItem(new PatchItem(fluid_sfont_get_id(curfont),
				  fluid_preset_get_banknum(&preset),
				  fluid_preset_get_num(&preset),
				  fluidchan,
				  fluid_preset_get_name(&preset)));

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

void FluidSynthProcessor::ReceiveEvent(Event *ev, EventProducer *from) {
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
