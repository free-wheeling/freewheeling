#ifndef __FWEELIN_CORE_DSP_H
#define __FWEELIN_CORE_DSP_H

/* Copyright 2004-2008 Jan Pekau (JP Mercury) <swirlee@vcn.bc.ca>
   
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

#include "fweelin_core.h"
#include "fweelin_event.h"
#include "fweelin_block.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

// Macros for converting between linear amplitude and dB
#define DB2LIN(db) (powf(10.0f, (db) * 0.05f))
#define LIN2DB(amp) (20.0f * log10f(amp))

class Fweelin;
class Loop;
class AudioBlock;
class AudioBlockIterator;
class PeaksAvgsManager;
class TimeMarker;
class InputSettings;
class AudioLevel;

// Class for converting between dB and vertical fader levels 
class AudioLevel {
  public:

  static float fader_to_dB (float level, float maxDb);
  static float dB_to_fader (float dB, float maxDb);
};

// Audio buffers encapsulates all buffers going into & out of
// a processor-- allowing for multiple ins and outs to be passed
class AudioBuffers {
 public:
  AudioBuffers(Fweelin *app);
  ~AudioBuffers();

  // Get input #n, left/mono (0) or right (1) channel
  inline sample_t *GetInput(int n, short channel) { return ins[channel][n]; };
  // Get output #n, left/mono (0) or right (1) channel
  inline sample_t *GetOutput(int n, short channel) { return outs[channel][n]; };

  // Get # of inputs/ouputs
  inline int GetNumInputs() { return numins; };
  inline int GetNumOutputs() { return numouts; };

  // Is input/output #n stereo?
  char IsStereoInput(int n);
  char IsStereoOutput(int n);
  // Is FreeWheeling running in stereo or completely in mono?
  char IsStereoMaster();

  // Mixes the selected inputs to dest (array of 2 channels)
  void MixInputs (nframes_t len, sample_t **dest, InputSettings *iset,
                  float inputvol, char compute_stats);

  // Get number of internal audio inputs into FreeWheeling
  static inline int GetIntAudioIns() { 
#if USE_FLUIDSYNTH
    return 1;
#else
    return 0;
#endif
  }

  // Number of audio outs
  static inline int GetAudioOuts() { return 1; };

  Fweelin *app; // Main app

  int numins, // Total number of ins
    numins_ext, // Number of external inputs
    numouts;  // & outs
  sample_t **ins[2], // 2 list of input sample buffers (mono/left and right) 
    **outs[2]; // & 2 lists of output sample buffers (mono/left and right)
};

// Settings for each input coming into FreeWheeling
// Now we can have multiple sets of settings & pass settings to recordprocessor.
// This allows differen recordprocessors to record from different inputs.
class InputSettings {
 public:
  InputSettings(Fweelin *app, int numins) : app(app), numins(numins) {
    selins = new char[numins];
    invols = new float[numins];
    dinvols = new float[numins];
    insums[0] = new sample_t[numins];
    insums[1] = new sample_t[numins];
    insavg[0] = new sample_t[numins];
    insavg[1] = new sample_t[numins];
    inpeak = new sample_t[numins];
    inpeaktime = new nframes_t[numins];
    inscnt = new int[numins];

    // Start with all inputs selected, and default volumes!
    for (int i = 0; i < numins; i++) {
      selins[i] = 1;
      invols[i] = 1.0;
      dinvols[i] = 1.0;
      insums[0][i] = 0.0;
      insums[1][i] = 0.0;
      insavg[0][i] = 0.0;
      insavg[1][i] = 0.0;
      inpeak[i] = 0.0;
      inpeaktime[i] = 0;
      inscnt[i] = 0;
    }
  };
  ~InputSettings() {
    delete[] selins;
    delete[] invols;
    delete[] dinvols;
    delete[] insums[0];
    delete[] insums[1];
    delete[] insavg[0];
    delete[] insavg[1];
    delete[] inpeak;
    delete[] inpeaktime;
    delete[] inscnt;
  };

  // (de)Select input
  void SelectInput(int n, char selected) { 
    if (n >= 0 || n < numins) {
      selins[n] = selected;
    } else {
      printf("CORE: InputSettings- input number %d not in range.\n",n);
    }
  };
  // Is input n selected?
  char InputSelected(int n) { 
    if (n >= 0 || n < numins) {
      return selins[n];
    } else {
      printf("CORE: InputSettings- input number %d not in range.\n",n);
      return 0;
    }
  };

  // Are any of the selected inputs stereo?
  char IsSelectedStereo();

  // Set input volume for input n
  void AdjustInputVol(int n, float adjust); 
  void SetInputVol(int n, float vol, float logvol);
  float GetInputVol(int n) { 
    if (n >= 0 || n < numins) {
      return invols[n];
    } else {
      printf("CORE: InputSettings- input number %d not in range.\n",n);
      return 0;
    }
  };
  float *GetInputVols() { return invols; };

  // Copy contents of settings from source object- don't fuss with pointers
  inline void operator = (InputSettings &src) {
    if (numins != src.numins)
      printf("CORE: InputSettings- number of inputs mismatch!\n");
    else {
      memcpy(selins,src.selins,sizeof(char)*numins);
      memcpy(invols,src.invols,sizeof(float)*numins);
      memcpy(dinvols,src.dinvols,sizeof(float)*numins);
      memcpy(insums[0],src.insums[0],sizeof(sample_t)*numins);
      memcpy(insums[1],src.insums[1],sizeof(sample_t)*numins);
      memcpy(insavg[0],src.insavg[0],sizeof(sample_t)*numins);
      memcpy(insavg[1],src.insavg[1],sizeof(sample_t)*numins);
      memcpy(inpeak,src.inpeak,sizeof(sample_t)*numins);
      memcpy(inpeaktime,src.inpeaktime,sizeof(sample_t)*numins);
      memcpy(inscnt,src.inscnt,sizeof(int)*numins);      
    }
  };

  Fweelin *app; // Main app- we get a few variables from it
  
  int numins; // Number of inputs 
  char *selins; // For each input, is it selected?
  float *invols, // For each input, what's the volume?
    *dinvols; // And the rate of volume change

  sample_t *insums[2], *insavg[2], *inpeak;
  nframes_t *inpeaktime;
  int *inscnt;
};

enum SyncStateType {
  SS_NONE,
  SS_START,
  SS_BEAT,
  SS_END,
  SS_ENDED
};

// Handles realtime DSP from a signal chain
// *** Consider making this an RT allocated type--
// because right now, triggering loops actually allocates memory
class Processor : public EventProducer {
public:
  const static float MIN_VOL;
  const static nframes_t DEFAULT_SMOOTH_LENGTH;

  Processor(Fweelin *app);
  virtual ~Processor();

  // Realtime process one fragment of audio-- process len samples-
  // { if pre is nonzero, preprocess len bytes ahead- 
  // used for smoothing sudden changes-- don't actually
  // advance pointers }
  virtual void process(char pre, nframes_t len, AudioBuffers *ab) = 0;

  Fweelin *getAPP() { return app; };

  // This is called by RootProcessor when a processor is flagged-
  // pending delete and should no longer perform any processing
  virtual void Halt() {};

  void dopreprocess();

 protected:

  // Fade together current with preprocessed to create a smoothed buffer
  void fadepreandcurrent(AudioBuffers *ab);

  // Parent Flo-Monkey app
  Fweelin *app;

  // Preprocessing audio buffers- store preprocessed output for
  // smoothing on control changes
  AudioBuffers *preab;
  nframes_t prelen;
  char prewritten, // Nonzero if a
                   // preprocess has been written to buffers
    prewriting;    // Nonzero if a
                   // preprocess is being written to buffers
};

// One processor in a linked list of processors
class ProcessorItem {
public:
  // Processor is running, or ready to be deleted
  const static int STATUS_GO = 0,   // Running
    STATUS_LIVE_PENDING_DELETE = 1, // Call RT thread, then delete
    STATUS_PENDING_DELETE = 2;      // Delete at next non-RT opportunity

  // Processor type- 
  // Global: processor will process
  //   after output volume transformation, and so, is not affected by it
  // Hipriority: these processors will process
  //   before all other processors
  // Final: processor will process after all other processors--
  // after TYPE_GLOBAL, after autolimit-- 
  // it will be fed the end signal chain of all other processors *as input*
  const static int TYPE_DEFAULT = 0,
    TYPE_GLOBAL = 1,
    TYPE_HIPRIORITY = 2,
    TYPE_FINAL = 3;

  ProcessorItem(Processor *p, int type = TYPE_DEFAULT) : p(p), next(0), 
    status(STATUS_GO), type(type) {};

  Processor *p;
  ProcessorItem *next;
  int status,
    type;
};

class PulseSyncCallback {
public:

  virtual void PulseSync (int syncidx, nframes_t actualpos) = 0;
};

class PulseSync {
public:
  PulseSync (PulseSyncCallback *cb = 0, nframes_t syncpos = 0) : cb(cb), syncpos(syncpos) {};
  
  PulseSyncCallback *cb;  // Callback instance
  nframes_t syncpos;      // Position where to call back
};

// Pulse defines a heartbeat for a piece
// It keeps track of a constant period for tempo, as well as
// the timing of the downbeat (current position in pulse)
class Pulse : public Processor {
public:
  // Length of metronome strike sound in samples
  const static nframes_t METRONOME_HIT_LEN;
  // Initial metronome volume
  const static float METRONOME_INIT_VOL;
  // Maximum number of user-defined pulse sync callbacks
  const static int MAX_SYNC_POS = 1000;

  Pulse(Fweelin *app, nframes_t len, nframes_t startpos);
  ~Pulse();

  inline float round(float num) {
    if (num-(long)num < 0.5)
      return floor(num);
    else
      return ceil(num);
  }

  virtual void process(char pre, nframes_t l, AudioBuffers *ab);

  inline char IsMetronomeActive() { return metroactive; };
  inline void SwitchMetronome(char active) { metroactive = active; };

  // Start/stop sending MIDI clock for this pulse
  void SetMIDIClock (char start);
  
  // Quantizes src length to fit to this pulse length 
  nframes_t QuantizeLength(nframes_t src);

  // These methods add and remove sync positions.
  // A pulse sends out an RT PulseSync event whenever any of these
  // positions is reached. (RT Safe, but can only be called from one thread- RT audio thread)
  inline int AddPulseSync(PulseSyncCallback *cb, nframes_t pos) { // Returns sync index of new position
    // Check position
    if (pos >= len) {
      printf("PULSE: Sync position adjusted for really short loop "
             "(pos %d, len %d)\n",pos,len);
      pos = len-1;
    }

    // First, search for unfilled positions in our array
    int i = 0;
    while (i < numsyncpos && syncpos[i].cb != 0)
      i++;
    if (i < numsyncpos) {
      // Position found, use this index
      syncpos[i] = PulseSync(cb,pos);
      return i;
    } else {
      // No holes found, add to end of array
      if (numsyncpos >= MAX_SYNC_POS) {
        printf("PULSE: Too many sync positions.\n");
        return -1;
      } else {
        int ret = numsyncpos;
        syncpos[numsyncpos++] = PulseSync(cb,pos);
        return ret;
      }
    }
  };
 
  // Removes sync position at index syncidx (RT Safe, but can only be called from one thread- RT audio thread)
  inline void DelPulseSync (int syncidx) {
    if (syncidx < 0 || syncidx >= numsyncpos)
      printf("PULSE: Invalid sync position %d (0->%d).\n",syncidx,numsyncpos);
    else {
      if (syncidx+1 == numsyncpos) {
        // Position exists on the end of array- shrink
        syncpos[syncidx] = PulseSync();
        numsyncpos--;
      } else
        // Position exists in the middle of the array- create a hole
        syncpos[syncidx] = PulseSync();
    }
  };

  // Returns nonzero if the wrapped bit is set-- which indicates
  // that the Meter has wrapped around to the beginning (downbeat)
  // in the last process frame
  inline char Wrapped() {
    if (wrapped) {
      wrapped = 0;
      return 1;
    }
    else
      return 0;
  }
  
  // Get current position in pulse in frames
  inline nframes_t GetPos() { return curpos; };

  // Set current position of pulse in frames
  inline void SetPos(nframes_t pos) { curpos = pos; };

  // Cause this pulse to wrap to its beginning, firing off any triggers
  // and producing a metronome pulse-- SetPos(0) does not do this
  inline void Wrap() { curpos = len; };

  // Set length of pulse in frames
  inline void SetLength(nframes_t newlen) { len = newlen; };

  // Get current position in %
  inline float GetPct() { return (float)curpos/len; };

  // Returns length of pulse in frames
  inline nframes_t GetLength() { return len; };

  nframes_t len, // Length of one revolution of this pulse in samples
    curpos; // Current position in samples into this pulse
  char wrapped,  // Wrapped?
    stopped;     // Stopped?

  int prev_sync_bb, // Previous beat/bar from transport (used for slave sync)
    sync_cnt,       // Number of external beats/bars that have elapsed since
                    // this pulse has wrapped around
    prev_sync_speed;
  char prev_sync_type;
  double prevbpm;   // Previous BPM from transport

  // Tap
  nframes_t prevtap; // samplecnt @ previous tap

  // Metronome
  sample_t *metro; // Sample data for metronome strike
  nframes_t metroofs, // Current position into metronome sample
    metrolen;         // Length of metronome sample
  char metroactive;   // Nonzero if metronome sound is active
  float metrovol;     // Volume of metronome

  PulseSync syncpos[MAX_SYNC_POS]; // Sync positions
  int numsyncpos; // Current number of sync positions
  
  SyncStateType clockrun;  // Status of MIDI clock
};

// This is the base of signal processing tree- it connects to system level audio
// and calls child processors which do signal processing
// Child processes execute in parallel and their signals are summed
class RootProcessor : public Processor {
public:
  RootProcessor(Fweelin *app, InputSettings *iset);
  virtual ~RootProcessor();

  // Adds a child processor.. the processor begins processing immediately
  // Possibly realtime safe?
  void AddChild (Processor *o, int type = ProcessorItem::TYPE_DEFAULT);

  // Removes a child processor from receiving processing time..
  // also, deletes the child processor
  // Realtime safe!
  void DelChild (Processor *o);

  void AdjustOutputVolume(float adjust);
  void SetOutputVolume(float set) { 
    // Preprocess audio for smoothing
    dopreprocess();

    outputvol = set; 
    doutputvol = 1.0; 
  };
  inline float GetOutputVolume() { return outputvol; }

  void AdjustInputVolume(float adjust);
  void SetInputVolume(float set) { 
    // Preprocess audio for smoothing
    dopreprocess();

    inputvol = set; 
    dinputvol = 1.0;
  };
  inline float GetInputVolume() { return inputvol; }

  float GetLimiterVolume() { return curlimitvol; };
  char GetLimiterFreeze() { return limiterfreeze; };
  void SetLimiterFreeze(char set) { limiterfreeze = set; };
  void ResetLimiter();

  // Sample accurate timing is provided through samplecnt
  inline nframes_t GetSampleCnt() { return samplecnt; };

  // Process len frames through all child processors of type 'ptype', 
  // passing abchild audio buffers to the processors and optionally mixing
  // into the main output buffers ab 
  void processchain(char pre, nframes_t len, AudioBuffers *ab,
                    AudioBuffers *abchild, const int ptype, 
                    const char mixintoout);

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);

  static void *run_cleanup_thread (void *ptr);
      
  pthread_t cleanup_thread;
  int threadgo;

  // Volumes- we are responsible for adjusting volumes in RT
  InputSettings *iset;
  float outputvol,
    doutputvol, // Delta output volume-- rate of change
    inputvol,
    dinputvol;  // Delta input volume-- rate of change
  
  ProcessorItem *firstchild;

  // Temporary buffers for summing signals
  AudioBuffers *abtmp, *preabtmp;
  sample_t *buf[2], *prebuf[2];
 
  // Count samples processed from start of execution
  volatile nframes_t samplecnt;

  // Autolimiter
  const static float LIMITER_ATTACK_LENGTH,
    LIMITER_START_AMP; 
  const static nframes_t LIMITER_ADJUST_PERIOD;
  float dlimitvol, // Limiter volume delta- how fast vol is being changed
    limitvol,      // Target volume- amplitude needed to stop clipping
    curlimitvol,   // Current volume- volume is moving towards limitvol
    maxvol;        // Maximum volume found in audio coming into limiter
  char limiterfreeze; // Nonzero if limiter is frozen- no changes in amp
};

class RecordProcessor : public Processor, public PulseSyncCallback {
public:
  // Extra tail on record end facilitates smooth crossfade for
  // sync-recorded loops
  const static nframes_t REC_TAIL_LEN = 1024;
  const static float OVERDUB_DEFAULT_FEEDBACK;
  
  // Notes:
 
  // One caveat, since hipri mgrs have granularity of only one fragment
  // loop points are not sample accurate but to nearest fragment
  // but there will be no drift since meter is sample accurate
  // and it is syncronizing time
  // ..
  // Should RecordProcessor create loops instead of LoopManager?
  // ..

  // Recording into preexisting fixed size block
  RecordProcessor(Fweelin *app,
                  InputSettings *iset, 
                  float *inputvol,
                  AudioBlock *dest, int suggest_stereo = -1);

  // Overdubbing version of record into existing loop
  RecordProcessor(Fweelin *app,
                  InputSettings *iset, 
                  float *inputvol,
                  Loop *od_loop,
                  float od_playvol,
                  nframes_t od_startofs,
                  float *od_feedback);  // Pointer to value for feedback (can be continuously varied)

  // Recording new blocks, growing size as necessary           
  RecordProcessor(Fweelin *app,
                  InputSettings *iset, 
                  float *inputvol,
                  Pulse *sync = 0, 
                  AudioBlock *audiomem = 0,
                  AudioBlockIterator *audiomemi = 0,
                  nframes_t peaksavgs_chunksize = 0);

  // Destructor- executed nonRT
  ~RecordProcessor();

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);

  // In Halt() method we ensure that no stray Pulse_Syncs will be responded to
  virtual void Halt() { stopped = 1; sync_state = SS_ENDED; };

  virtual void PulseSync (int syncidx, nframes_t actualpos);

  // Sync up overdubbing of the loop to a newly created pulse
  void SyncUp();
  
  nframes_t GetRecordedLength(); 

  AudioBlock *GetFirstRecordedBlock() { return recblk; }
  PeaksAvgsManager *GetPAMgr() { return pa_mgr; }

  // Is this an overdub record (1) or a fresh record (0)?
  char IsOverdub() { return (od_loop != 0); };

  long GetNBeats() { return nbeats; };

  // End this recording- if we are syncronized to an external pulse,
  // then we adjust recording end time as necessary
  void End();

  // Stops recording now-- RT safe!
  void EndNow();

  // Abort recording-- free memory associated with all audio blocks
  void AbortRecording();

  // Fades input samples out of mix- writing to 'dest'
  void FadeOut_Input(nframes_t len, 
                     sample_t *input_l, sample_t *input_r,
                     sample_t *loop_l, sample_t *loop_r, 
                     float old_fb, float new_fb, float fb_delta,
                     sample_t *dest_l, sample_t *dest_r);

  // Fades input samples into mix- writing to 'dest'
  void FadeIn_Input(nframes_t len, 
                    sample_t *input_l, sample_t *input_r,
                    sample_t *loop_l, sample_t *loop_r, 
                    float old_fb, float new_fb, float fb_delta,
                    sample_t *dest_l, sample_t *dest_r);

  // Jumps to a position within an overdubbing loop- fade of input & output
  void Jump(nframes_t ofs);

  void SetODPlayVol(float newvol) {
    // Preprocess audio for smoothing
    dopreprocess();

    od_playvol = newvol;
  }

  float GetODPlayVol() {
    return od_playvol;
  }

  AudioBlockIterator *GetIterator() { return i; };
  Pulse *GetPulse() { return sync; };

  SyncStateType sync_state; // Are we waiting for a downbeat, running, ended?

  // Recording in stereo?
  char stereo;

  // Which inputs to record from and at what volumes?
  InputSettings *iset;
  float *inputvol;   // Pointer to overall input volume- can change during record
  sample_t *mbuf[2]; // Mixed input buffers

  // Pulse to syncronize (quantize) record to
  Pulse *sync; 
  // Iterator with current record position, and temporary iterator
  AudioBlockIterator *i, *tmpi;
  // Block to record into
  AudioBlock *recblk;
  // (Syncrec) Number of beat triggers passed in this recording 
  long nbeats;

  int endsyncidx;   // Pulse sync index for delayed end-of-record
  char endsyncwait; // Are we waiting for a delayed end-of-record?
  
  int sync_idx;           // Index of sync callback added (or -1 if none is being used)
  nframes_t sync_add_pos; // Position in pulse where to add sync callback
  char sync_add;    // RT thread should call AddPulseSync to let pulse know we are waiting for a sync callback
  
  char stopped,
    growchain, // Nonzero if we should grow the chain of blocks--
               // Zero if record is fixed length
    compute_stats; // Nonzero if we should compute stats like DC offset,
                   // sums & peaks for InputSettings

  // Manager for peaks & averages computation alongside this record
  PeaksAvgsManager *pa_mgr;

  // Overdub settings
  Loop *od_loop;
  float od_playvol, 
    *od_feedback,
    od_feedback_lastval;  // Last value for feedback- used to determine delta, to remove
                          // zipper noise
  long od_curbeat; // Current beat in od_loop
  char od_fadein, od_fadeout, od_stop, // Fade in overdub, fade out overdub,
                                       // and stop overdub flags
    od_prefadeout;                     // Overdub, preprocess fadeout

  nframes_t od_lastofs; // Last position of record
  sample_t *od_last_mbuf[2], // Copy of last mixed input buffers
    *od_last_lpbuf[2];       // Copy of last loop buffers
};

class PlayProcessor : public Processor, public PulseSyncCallback {
public:
  PlayProcessor(Fweelin *app, Loop *playloop, float playvol, 
                nframes_t startofs = 0);
  virtual ~PlayProcessor();

  // Sync up playing of the loop to a newly created pulse
  void SyncUp();

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);

  virtual void PulseSync (int syncidx, nframes_t actualpos);

  nframes_t GetPlayedLength();

  void SetPlayVol(float newvol) {
    // Preprocess audio for smoothing
    dopreprocess();

    playvol = newvol;
  }

  float GetPlayVol() {
    return playvol;
  }

  // In Halt() method we ensure that no stray Pulse_Syncs will be responded to
  virtual void Halt() { stopped = 1; sync_state = SS_ENDED; };

  SyncStateType sync_state; // Are we waiting for a downbeat, running, ended?

  // Pulse to syncronize (quantize) play to
  Pulse *sync; 

  int sync_idx;           // Index of sync callback added (or -1 if none is being used)
  nframes_t sync_add_pos; // Position in pulse where to add sync callback
  char sync_add;    // RT thread should call AddPulseSync to let pulse know we are waiting for a sync callback

  // Playing in stereo?
  char stereo;

  // Stop- pause for right place to start
  char stopped;

  AudioBlockIterator *i;
  Loop *playloop;
  float playvol;

  long curbeat;
};

class FileStreamer : public Processor, public EventListener {
 public:
  const static nframes_t OUTPUTBUFLEN = 100000;
  const static int MARKERBUFLEN = 50;

  FileStreamer(Fweelin *app, nframes_t outbuflen = OUTPUTBUFLEN);
  virtual ~FileStreamer();

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);
  virtual void ReceiveEvent(Event *ev, EventProducer *from);

  // Starts writing to a new audio stream
  // Note all the heavy work is done in the encode thread!
  int StartWriting(char *filename, char *timingname, codec type) {
    if (outname != 0) 
      return -1; // Already writing!

    outname = filename;
    outputsize = 0;
    filetype = type;
    this->timingname = timingname;
    writerstatus = STATUS_START_PENDING;

    return 0;
  };

  // Stop writing to a stream
  // Note all the heavy work is done in the encode thread!
  void StopWriting() {
    writerstatus = STATUS_STOP_PENDING;
  };

  char GetStatus() { return writerstatus; };

  // Returns the number of *frames* written so far-- not bytes
  // The actual file size will vary depending on the codec used
  long int GetOutputSize() { return outputsize; };

  static void *run_encode_thread (void *ptr);

  const static char STATUS_STOPPED = 0,
    STATUS_RUNNING = 1,
    STATUS_STOP_PENDING = 2,
    STATUS_START_PENDING = 3;

 private:
  void InitStreamer();
  void EndStreamer();
  
  char writerstatus;  

  // Encoding in stereo?
  char stereo;
  codec filetype;

  // File
  FILE *outfd,   // Current output filedescriptor (audio)
    *timingfd;   // Current timing output filedescriptor (data- USX)
  char *outname, // Current output filename
    *timingname; // Current timing output filename

  // Time markers for storing downbeat points along with audio
  TimeMarker *marks;
  int mkwriteidx,
    mkreadidx;

  // Number of beat triggers passed in this recording 
  long nbeats;
  // Global sample count at start of stream file
  nframes_t startcnt;

  // Output buffers
  sample_t *outbuf[2];
  nframes_t outbuflen,
    outpos, // Current position of realtime data coming into output buffer
    encodepos; // Current position of encoder in buffer (lags behind outpos)
  char wrap; // Nonzero if outpos has wrapped but encodepos hasn't yet

  // Number of bytes written to output file
  long int outputsize;

  // Disk encode/disk write thread
  pthread_t encode_thread;
  int threadgo;

  // Encoder
  iFileEncoder *enc;
};

// PassthroughProcessor creates a monitor mix of all inputs into the given output
class PassthroughProcessor : public Processor {
public:
  PassthroughProcessor(Fweelin *app, InputSettings *iset, float *inputvol);
  virtual ~PassthroughProcessor();

  virtual void process(char pre, nframes_t len, AudioBuffers *ab);

  // Input settings with all inputs selected- to create monitor mix
  InputSettings *alliset,
    *iset; // Pointer to outside input settings from which levels will be taken
  float *inputvol; // Pointer to overall input volume- can change
};

#endif
