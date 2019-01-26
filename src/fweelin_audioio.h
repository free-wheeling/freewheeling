#ifndef __FWEELIN_AUDIOIO_H
#define __FWEELIN_AUDIOIO_H

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

extern "C"
{
#include <jack/jack.h>
}

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

class Fweelin;
class Processor;

// **************** SYSTEM LEVEL AUDIO

class AudioIO {
public:
  AudioIO(Fweelin *app) : sync_start_frame(0), timebase_master(0), sync_active(0), audio_thread(0), app(app) {};

  // Open up system level audio
  int open ();

  // Activate system level audio
  int activate (Processor *rp);

  // Close system level audio
  void close ();

  // Get realtime buffer size
  nframes_t getbufsz();

  // **Callbacks**

  // Realtime process function.. the beginning of the DSP chain
  static int process (nframes_t nframes, void *arg);

  // Timebase (jack transport sync) callback
  static void timebase_callback(jack_transport_state_t /*state*/,
                                jack_nframes_t /*nframes*/,
                                jack_position_t *pos, int new_pos, void *arg);
    
  // Sampling rate change callback
  static int srate_callback (nframes_t nframes, void *arg);

  // Callback for audio shutdown
  static void audio_shutdown (void */*arg*/);

  // Get current sampling rate
  inline nframes_t get_srate() { return srate; };

  // Get approximate audio CPU load
  inline float GetAudioCPULoad() { return cpuload; };
  
  inline float GetTimeScale() { return timescale; };
  
  // Transport sync methods

  // Reposition transport to the given position
  // Used for syncing external apps
  void RelocateTransport(nframes_t pos);

  // Get current bar in transport mechanism
  inline int GetTransport_Bar() { return jpos.bar; };

  // Get current beat in transport mechaniasm
  inline int GetTransport_Beat() { return jpos.beat; };

  // Get current BPM in transport mechanism
  inline double GetTransport_BPM() { return jpos.beats_per_minute; };

  // Get current # of beats per bar in transport mechanism
  inline float GetTransport_BPB() { return jpos.beats_per_bar; };

  // Are we sending or receiving sync?
  inline char IsSync() { return sync_active; };

  // Are we the timebase master?
  inline char IsTimebaseMaster() { return timebase_master; };
  
  // Is the transport rolling?
  inline char IsTransportRolling() { return transport_roll; };

  // Audio system client
  jack_client_t *client;

  // Inputs and outputs- stereo pairs
  jack_port_t **iport[2], **oport[2];

  float cpuload; // Current approximate audio CPU load
  float timescale; // fragment length/sample rate = length (s) of one fragment
  nframes_t srate; // Sampling rate

  // Variables for audio (Jack transport) sync

  // Jack frame where the first bar began in transport
  int32_t sync_start_frame; 
  char repos; // Nonzero if we have repositioned JACK internally
  jack_position_t jpos; // Current JACK position
  char timebase_master; // Nonzero if we are the JACK timebase master
  char sync_active;     // Nonzero if sync is active
  char transport_roll;  // Nonzero if the transport is rolling

  volatile pthread_t audio_thread;  // RT audio thread (created by JACK)

  // Pointer to the main app
  Fweelin *app;
  // Processor which is basically the root of the signal flow
  Processor *rp; 
};

#endif
