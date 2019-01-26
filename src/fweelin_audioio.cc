/*
   Does power come from fancy toys
   or does power come from the integrity of our walk?
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

#include "fweelin_config.h"
#include "fweelin_audioio.h"
#include "fweelin_core_dsp.h"

#if USE_FLUIDSYNTH
#include "fweelin_fluidsynth.h"
#endif

// **************** SYSTEM LEVEL AUDIO

int AudioIO::process (nframes_t nframes, void *arg) {
  AudioIO *inst = static_cast<AudioIO *>(arg);

  if (inst->audio_thread == 0)
    inst->audio_thread = pthread_self();

  // Check if EMG or MEM needs wakeup
  inst->app->getEMG()->WakeupIfNeeded();
  inst->app->getMMG()->WakeupIfNeeded();

  // Get CPU load
  inst->cpuload = jack_cpu_load(inst->client);

  // Get JACK transport timing and status
  int tmp_roll = 
    (jack_transport_query(inst->client,&(inst->jpos)) == JackTransportRolling);
  if (inst->jpos.valid & JackPositionBBT) 
    inst->sync_active = 1;
  else
    inst->sync_active = 0;

  // Get buffers from jack
  AudioBuffers *ab = inst->app->getABUFS();
  for (int i = 0; i < ab->numins_ext; i++) {
    // Left/mono channel
    ab->ins[0][i] = 
      (sample_t *) jack_port_get_buffer (inst->iport[0][i], nframes);
    // Right channel
    ab->ins[1][i] = (inst->iport[1][i] != 0 ?
                     (sample_t *) 
                     jack_port_get_buffer (inst->iport[1][i], nframes) :
                     0);
  }
  for (int i = 0; i < ab->numouts; i++) {
    // Left/mono channel
    ab->outs[0][i] = 
      (sample_t *) jack_port_get_buffer (inst->oport[0][i], nframes);
    // Right channel
    ab->outs[1][i] = (inst->oport[1][i] != 0 ?
                      (sample_t *) 
                      jack_port_get_buffer (inst->oport[1][i], nframes) :
                      0);
  }

  if (inst->rp != 0) {
    if (nframes != inst->app->getBUFSZ()) {
      printf("AUDIO: We've got a problem, honey!--\n");
      printf("Audio buffer size has changed: %d->%d\n",
             inst->app->getBUFSZ(),nframes);
      exit(1);
    }

    // Run through audio processors
    inst->rp->process(0, nframes, ab);
  }

  inst->timebase_master = 0; // Reset timebase master flag-
                             // callback will set to 1 if we are the master
  inst->transport_roll = tmp_roll; // Set transport rolling status
  return 0;      
}

// Reposition JACK transport to the given position
// Used for syncing external apps
void AudioIO::RelocateTransport(nframes_t pos) {
  if (timebase_master) {
    jack_transport_locate(client,pos);
    repos = 1;
  }
};

void AudioIO::timebase_callback(jack_transport_state_t /*state*/,
                                jack_nframes_t /*nframes*/,
                                jack_position_t *pos, int new_pos, void *arg) {
  AudioIO *inst = static_cast<AudioIO *>(arg);

  // Set timebase master flag
  inst->timebase_master = 1;

  /* printf("timebase called back (frame: %d, framerate: %d)!\n", 
     pos->frame,pos->frame_rate); */

  // Use our pulse information plus JACK's frame information
  // to calculate bars and beats and ticks
  Pulse *p = inst->app->getLOOPMGR()->GetCurPulse();
  if (p != 0) {
    if (new_pos) {
      if (inst->repos) 
        inst->repos = 0; // JACK telling us we have moved- but we initiated
                         // the move, so ignore
      else {
        // Somebody has started the transport at a new position-
        // signal back that we want to start at a new position--
        // based on the current pulse position.

        // printf("relocate: posframe: %d to %d\n",pos->frame,p->GetPos()); 
        jack_transport_locate(inst->client,p->GetPos());
        inst->repos = 1;
        
#if 0
        inst->sync_start_frame = pos->frame + (p->GetLength()-p->GetPos());
        printf("posframe: %d syncstartframe set: %d\n",pos->frame,
               inst->sync_start_frame);
#endif
        
        inst->sync_start_frame = 0; //pos->frame;
      }
    }

#define TICKS_PER_BEAT 1920

    int32_t rel_frame = (int) pos->frame - inst->sync_start_frame;
    float rel_bar = (float) rel_frame*inst->app->GetSyncSpeed()/p->GetLength();
    if (inst->app->GetSyncType() != 0)
      // Beat sync, adjust bar by SYNC_BEATS_PER_BAR
      rel_bar /= SYNC_BEATS_PER_BAR;
    pos->valid = JackPositionBBT;
    pos->bar = (int32_t) rel_bar;
    float bar_frac = rel_bar - pos->bar;
    pos->beat = (int) (bar_frac * SYNC_BEATS_PER_BAR);
    pos->beats_per_bar = SYNC_BEATS_PER_BAR;
    if (inst->app->GetSyncType() == 0)
      pos->beats_per_minute = SYNC_BEATS_PER_BAR*
        60.0*(double) pos->frame_rate/p->GetLength();
    else
      pos->beats_per_minute = 60.0*(double) pos->frame_rate/p->GetLength();
    pos->beat_type = SYNC_BEATS_PER_BAR;
    pos->ticks_per_beat = TICKS_PER_BEAT;
    float beat_frac = bar_frac*SYNC_BEATS_PER_BAR - (float)pos->beat;
    pos->tick = (int) (beat_frac * TICKS_PER_BEAT);

    pos->bar++;
    pos->beat++;
    //pos->tick++;

    pos->bar_start_tick = pos->bar * SYNC_BEATS_PER_BAR * TICKS_PER_BEAT;

    //printf("ticks per beat: %f\n",pos->ticks_per_beat);
    //printf("rel bar: %f, bar: %d, beat: %d, tick: %d\n",rel_bar, pos->bar,
    //       pos->beat, pos->tick);
  }
}

int AudioIO::srate_callback (nframes_t nframes, void *arg) {
  AudioIO *inst = static_cast<AudioIO *>(arg);
  printf ("AUDIO: Sample rate is now %d/sec\n", nframes);
  inst->srate = nframes;
  return 0;
}

void AudioIO::audio_shutdown (void */*arg*/)
{
  printf("AUDIO: shutdown! exiting..\n");
  exit(1);
}

int AudioIO::activate (Processor *rp) {
  const char **ports;

  // Store the rootprocessor passed as beginning of signal chain
  this->rp = rp;

  // Start rolling audio through JACK server
  if (jack_activate (client)) {
    printf("AUDIO: ERROR: Cannot activate client!\n");
    return 1;
  }
  
  // Connect ports
  //AudioBuffers *ab = app->getABUFS();
  
  // INPUT
  // No longer connect audio ports because the mapping isn't clear with
  // multi stereoins/outs
  if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput)) == NULL)
    printf("AUDIO: WARNING: Cannot find any physical capture ports!\n");

#if 0
  for (int i = 0; i < ab->numins_ext && ports[i] != 0; i++)
    if (jack_connect (client, ports[i], jack_port_name (iport[i]))) {
      printf("AUDIO: Cannot connect input port %d->%s!\n",i,
             jack_port_name(iport[i]));
    }
#endif

  free(ports);
  
  // OUTPUT
  if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL)
    printf("AUDIO: WARNING: Cannot find any physical playback ports!");

#if 0
  for (int i = 0; i < ab->numouts && ports[i] != 0; i++)
    if (jack_connect (client, jack_port_name (oport[i]), ports[i])) {
      printf("AUDIO: Cannot connect output port %d->%s!\n",i,
             jack_port_name(oport[i]));
    }
#endif

  free(ports);

  while (audio_thread == 0)
    // Wait for first process callback to register audio thread
    usleep(10000);

  RT_RWThreads::RegisterReaderOrWriter(audio_thread);

  return 0;
}

nframes_t AudioIO::getbufsz() {
  return jack_get_buffer_size (client);
}

void AudioIO::close () {
  jack_release_timebase (client);
  jack_client_close (client);

  delete[] iport[0];
  delete[] iport[1];
  delete[] oport[0];
  delete[] oport[1];

  printf("AUDIO: end\n");
}

int AudioIO::open () {
  // **** AUDIO startup
  
  // Try to become a client of the JACK server
  client = jack_client_open("FreeWheeling", JackNoStartServer, NULL);
  if (!client) {
    fprintf (stderr, "AUDIO: ERROR: Jack server not running!\n");
    return 1;
  }
  
  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */  
  jack_set_process_callback (client, process, this);
  
  jack_nframes_t bufsz = jack_get_buffer_size (client);
  printf ("AUDIO: Audio buffer size is: %d\n", bufsz);

  /* tell the JACK server to call `srate_callback()' whenever
     the sample rate of the system changes.
  */
  jack_set_sample_rate_callback (client, srate_callback, this);
  
  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */
  jack_on_shutdown (client, audio_shutdown, this);
  
  // Set timebase callback
  jack_set_timebase_callback (client, 1, timebase_callback, this);

  /* display the current sample rate. once the client is activated 
     (see below), you should rely on your own sample rate
     callback (see above) for this value.
  */
  srate = jack_get_sample_rate (client);
  printf ("AUDIO: Engine sample rate is %d\n", srate);

  // Set time scale
  timescale = (float) bufsz/srate;

  repos = 0;

  // Create buffers
  AudioBuffers *ab = app->getABUFS();

  printf("AUDIO: Using %d external inputs, %d total inputs\n",
         ab->numins_ext,ab->numins);
  iport[0] = new jack_port_t *[ab->numins_ext];
  iport[1] = new jack_port_t *[ab->numins_ext];
  oport[0] = new jack_port_t *[ab->numouts];
  oport[1] = new jack_port_t *[ab->numouts];

  // Create ports
  char tmp[255];
  for (int i = 0; i < ab->numins_ext; i++) {
    char stereo = ab->IsStereoInput(i);
    snprintf(tmp,255,"in_%d%s",i+1,(stereo ? "L" : ""));
    iport[0][i] = jack_port_register(client, tmp, JACK_DEFAULT_AUDIO_TYPE, 
                                     JackPortIsInput, 0);
    if (stereo) {
      snprintf(tmp,255,"in_%d%s",i+1,"R");
      iport[1][i] = jack_port_register(client, tmp, JACK_DEFAULT_AUDIO_TYPE, 
                                       JackPortIsInput, 0);
    } else
      iport[1][i] = 0;
  }

  for (int i = 0; i < ab->numouts; i++) {
    char stereo = ab->IsStereoOutput(i);
    snprintf(tmp,255,"out_%d%s",i+1,(stereo ? "L" : ""));
    oport[0][i] = jack_port_register(client, tmp, JACK_DEFAULT_AUDIO_TYPE, 
                                     JackPortIsOutput, 0);
    if (stereo) {
      snprintf(tmp,255,"out_%d%s",i+1,"R");
      oport[1][i] = jack_port_register(client, tmp, JACK_DEFAULT_AUDIO_TYPE, 
                                       JackPortIsOutput, 0);
    } else
      oport[1][i] = 0;
  }
  
  return 0;
}
