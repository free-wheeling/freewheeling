#ifndef __FWEELIN_CORE_H
#define __FWEELIN_CORE_H

/*
   ********
   Art is what's going on right under our noses.
   An artist is just one who listens to it.
   ********
*/

#ifdef __MACOSX__
#include <openssl/md5.h>
#else
#include <gnutls/openssl.h>
#endif

#include "fweelin_midiio.h"
#include "fweelin_videoio.h"
#include "fweelin_sdlio.h"
#include "fweelin_audioio.h"

#include "fweelin_core_dsp.h"
#include "fweelin_block.h"
#include "fweelin_event.h"
#include "fweelin_config.h"
#include "fweelin_browser.h"

class SDLIO;
class EventManager;
class BlockManager;
class RootProcessor;
class RecordProcessor;
class TriggerMap;
class AudioBlock;
class AudioBlockIterator;
class Loop;
class Pulse;
class BED_MarkerPoints;
class FileStreamer;
class PreallocatedType;
class AudioBuffers;
class InputSettings;
class Browser;

#if USE_FLUIDSYNTH
class FluidSynthProcessor;
#endif

// ****************** CORE CLASSES

// Base class for all saveable types of objects
#define SAVEABLE_HASH_LENGTH 16 //MD5_DIGEST_LENGTH
#define GET_SAVEABLE_HASH_TEXT(s) \
  char hashtext[SAVEABLE_HASH_LENGTH*2+1]; \
  { \
    char *ptr = hashtext; \
    for (int i = 0; i < SAVEABLE_HASH_LENGTH; i++, ptr += 2) \
      sprintf(ptr,"%02X",s[i]); \
    *ptr = '\0'; \
  }

enum SaveStatus {
  NO_SAVE,
  SAVE_DONE
}; 

class Saveable {
 public:
  Saveable() : savestatus(NO_SAVE) {};

  virtual void Save(Fweelin *app) {}; // Save the object

  inline unsigned char *GetSaveHash() { return savehash; };
  inline int CompareHash(unsigned char *hash2) { 
    return memcmp(savehash,hash2,sizeof(unsigned char) * SAVEABLE_HASH_LENGTH);
  };
  inline SaveStatus GetSaveStatus() { return savestatus; };
  inline void SetSaveStatus(SaveStatus s) { savestatus = s; };
  inline int SetSaveableHashFromText(char *stext) {
    int slen = strlen(stext);
    if (slen != SAVEABLE_HASH_LENGTH*2) {
      printf("DISK: Invalid MD5 hash '%s'\n",stext);
      return 1;
    }
    else {
      char htmp[3];
      htmp[2] = '\0';
      
      for (int i = 0, j = 0; i < slen; i += 2, j++) {
	htmp[0] = stext[i];
	htmp[1] = stext[i+1];
	int sh_tmp;
	sscanf(htmp,"%X",&sh_tmp);
	savehash[j] = (unsigned char) sh_tmp;
      }
      
      SetSaveStatus(SAVE_DONE);
      return 0;
    }
  };

  // Splits a saveable filename in the format 'basename-hash-objectname'
  // into its base name, hash and object name components
  //
  // Returns zero on success
  static char SplitFilename(char *filename, int baselen, char *basename, 
			    char *hash, char *objname,
			    int maxlen);
  
  // Rename the file with name *filename_ptr to a new name
  // use the format 'basename-hash-objectname'
  // Here, we specify a new objectname 'newname', but the basename and hash
  // are retained in the filename
  // Exts is a list of filename extensions to try for renaming
  // num_exts is the size of the list
  //
  // This is for renaming an item on disk
  static void RenameSaveable(char **filename_ptr, int baselen, char *newname,
			     const char **exts, int num_exts);

  // Renames -this- saveable object on disk to correspond with the new name
  // given. Any files having an MD5 corresponding to this saveable are
  // renamed.
  //
  // This is for renaming an item in memory, so that the disk corresponds
  // with the new name
  //
  // *Old_filename and *new_filename are set to point to the old and new
  // filename-- you must delete[] the memory that is allocated for these
  void RenameSaveable(char *librarypath, char *basename, 
		      char *old_objname, char *nw_objname,
		      const char **exts, int num_exts,
		      char **old_filename, char **new_filename);

  // Gets the first two characters of the hash in the given filename,
  // given the base length-- store in *c1 and *c2-- only if the
  // filename contains a valid hash
  inline static void GetHashFirst(char *filename, int baselen, 
				  char *c1, char *c2) {
    char *c = filename + baselen + 1;
    if (c < filename+strlen(filename)) {
      *c1 = *c++;
      *c2 = *c;
      
#if 0
      char *slashptr = strchr(c,'-');

      if (slashptr == 0) // If name is not in filename
	slashptr = strrchr(filename,'.'); // Use extension to get hash

      printf("c1 - %d vs %d\n",slashptr-c,SAVEABLE_HASH_LENGTH*2);

      if (slashptr != 0 && slashptr-c == SAVEABLE_HASH_LENGTH*2) {
	printf("set\n");

	*c1 = *c++;
	*c2 = *c;
      }
#endif
    }
  }

 protected:

  // When we save an object, we first compute the MD5 hash for that object
  // and put that in the filename for the object. Other objects can refer to
  // the object by hash. 
  SaveStatus savestatus;
  unsigned char savehash[SAVEABLE_HASH_LENGTH+1];
};

// Loop 
// -Wraps around a list of audio blocks
// -Stores basic parameters for a loop, such as volumes
class Loop : public Saveable {
  friend class LoopManager;

public:
  const static float MIN_VOL;

  Loop (AudioBlock *blocks, Pulse *pulse, float quant, float vol, 
	long nbeats, codec format) : 
    name(0), format(format), blocks(blocks), pulse(pulse), quant(quant), 
    vol(vol), dvol(1.0), nbeats(nbeats){};
  virtual ~Loop () {
    if (name != 0)
      delete[] name; 
  };

  // Save loop
  virtual void Save(Fweelin *app);

  // RT update volume
  inline void UpdateVolume() {
    if (dvol != 1.0) {
      // Apply delta
      if (dvol > 1.0 && vol < MIN_VOL)
	vol = MIN_VOL;
      vol *= dvol;
    }
  };

  // Perhaps store the length of the loop...
  // Because right now GetTotalLength is a calculation
  // in AudioBlock

  char *name;         // Name of loop, or null
  codec format;       // File format on disk
  AudioBlock *blocks; // Chain of blocks that form this loop
  Pulse *pulse;       // Time pulse to which this loop is quantized
  float quant;        // Quantization factor

  float vol,          // Volume multiplier
    dvol;             // Rate of volume change

  long nbeats;        // Number of beats in this loop
};

// This class manages sets of loops, allowing them to be triggered..
// like a keymap
class TriggerMap : public Saveable, public EventProducer {
public:
  TriggerMap (Fweelin *app, int mapsize) : app(app), mapsize(mapsize), 
    lastupdate(0) {
    // Map is an array of pointers to different loops
    map = new Loop *[mapsize];
    memset(map,0,mapsize * sizeof(Loop *));
  };

  virtual ~TriggerMap () {
    delete[] map;
  }

  // Save a whole scene, with an optional filename-
  // if none is given, saves a new scene
  virtual void Save(Fweelin *app, char *filename = 0);

  // Go save- when all loops are saved, this part saves the XML data
  // for the scene
  void GoSave(char *filename);

  inline void SetMap (int index, Loop *smp);

  inline Loop *GetMap (int index) {
    if (index < 0 || index >= mapsize) {
      printf("GetMap: Invalid loop index!\n");
      return 0;
    }
    else
      return map[index];
  }

  inline int SearchMap (Loop *l) {
    for (int i = 0; i < mapsize; i++)
      if (map[i] == l)
	return i;
    return -1;
  };

  // Return the first free loopid within the range of indexes 'lo -> hi'
  // or -1 if no free IDs found
  inline int GetFirstFree (int lo, int hi) {
    int i_start = lo,
      i_end = hi;

    if (i_start < 0)
      i_start = 0;
    if (i_start >= mapsize)
      i_start = mapsize-1;
    if (i_end < 0)
      i_end = 0;
    if (i_end >= mapsize)
      i_start = mapsize-1;

    for (int i = i_start; i <= i_end; i++)
      if (map[i] == 0)
	return i;

    return -1;
  };

  // Returns the loop ID for the first loop whose 'savehash' matches the given
  // hash, or -1 if none found
  inline int ScanForHash (unsigned char *hash) {
    for (int i = 0; i < mapsize; i++)
      if (map[i] != 0 && map[i]->GetSaveStatus() == SAVE_DONE &&
	  !map[i]->CompareHash(hash))
	return i;
    return -1;
  };

  inline int GetMapSize() { return mapsize; }
  inline double GetLastUpdate() { return lastupdate; }
  inline void TouchMap() {
    lastupdate = mygettime();
    SetSaveStatus(NO_SAVE);
  };

private:
  Fweelin *app;

  int mapsize;
  Loop **map;

  // Time of last update of the map- used for rebuilding tables & such
  double lastupdate;
};

// Status types for loops
enum LoopStatus {
  T_LS_Off,

  T_LS_Recording,
  T_LS_Overdubbing,
  T_LS_Playing
};

// LoopTrayItem is for loaded loops
class LoopTrayItem : public BrowserItem {
 public: 
  LoopTrayItem(Loop *l, int loopid, char *name, char default_name,
	       char *placename) : 
    BrowserItem(name,default_name), l(l), loopid(loopid),
    xpos(-1), ypos(-1), placename(placename) {};

  virtual int Compare(BrowserItem *second) { 
    if (second->GetType() == B_Loop_Tray)
      return ((LoopTrayItem *) second)->loopid-loopid;
    else
      return 0;
  };

  virtual int MatchItem(int itemmatch) { 
    if (loopid == itemmatch)
      return 0;
    else 
      return 1;
  };

  virtual BrowserItemType GetType() { return B_Loop_Tray; };
  
  Loop *l;         // Loop that this item refers to
  int loopid,      // Loop ID for it
    xpos,          // Position in tray window
    ypos;
  char *placename; // Place name for where the loop is mapped
};

// LoopBrowserItem is for loops in the library on disk
class LoopBrowserItem : public BrowserItem {
 public:
  LoopBrowserItem(time_t time, char *name, char default_name, char *fn) : 
    BrowserItem(name,default_name), time(time) {
    if (fn == 0)
      filename = 0;
    else {
      // Copy filename
      filename = new char[strlen(fn)+1];
      strcpy(filename,fn);
      // Remove extension
      char *ext_ptr = strrchr(filename,'.');
      if (ext_ptr != 0)
	*ext_ptr = '\0';
    }
  };
  virtual ~LoopBrowserItem() {
    if (filename != 0)
      delete[] filename;
  };

  virtual int Compare(BrowserItem *second) { 
    if (second->GetType() == B_Loop)
      return ((LoopBrowserItem *) second)->time-(signed int) time;
    else
      return 0;
  };

  virtual BrowserItemType GetType() { return B_Loop; };
  
  time_t time;
  char *filename;
};

class SceneBrowserItem : public BrowserItem {
 public:
  SceneBrowserItem(time_t time, char *name, char default_name, char *fn) : 
    BrowserItem(name, default_name), time(time) {
    if (fn == 0)
      filename = 0;
    else {
      // Copy filename
      filename = new char[strlen(fn)+1];
      strcpy(filename,fn);
      // Remove extension
      char *ext_ptr = strrchr(filename,'.');
      if (ext_ptr != 0)
	*ext_ptr = '\0';
    }
  };
  virtual ~SceneBrowserItem() {
    if (filename != 0)
      delete[] filename;
  };

  virtual int Compare(BrowserItem *second) { 
    if (second->GetType() == B_Scene)
      return ((SceneBrowserItem *) second)->time - (signed int) time;
    else 
      return 0;
  };

  virtual BrowserItemType GetType() { return B_Scene; };
  
  time_t time;
  char *filename;
};

// LoopManager contains all loops, and wraps up recording, playing, and
// other RT & non-RT processing on loops
class LoopManager : public EventListener, public AutoWriteControl, 
		    public AutoReadControl, public BrowserCallback,
		    public RenameCallback {
  friend class Loop;

public:
  // Version tracking for saving of loop data
  const static int LOOP_SAVE_FORMAT_VERSION = 1;

  LoopManager (Fweelin *app);
  virtual ~LoopManager();

  virtual void ReceiveEvent(Event *ev, EventProducer *from);

  virtual void ItemBrowsed(BrowserItem *i);
  virtual void ItemSelected(BrowserItem *i);
  virtual void ItemRenamed(BrowserItem *i);

  // Populate the loop/scene browser with any loops/scenes on disk
  void SetupLoopBrowser();
  void SetupSceneBrowser();

  // Get length returns the length of any loop on the specified index
  nframes_t GetLength(int index);

  // Get length returns the length of any loop on the specified index
  // Rounded to its currently quantized length
  // Or 0 if the loop has no pulse
  nframes_t GetRoundedLength(int index);

  // Returns from 0.0-1.0 how far through a block chain with specified index
  // we are
  float GetPos(int index);

  // Get current # of samples into block chain with given index
  nframes_t GetCurCnt(int index);

  // Returns nonzero if there is a processor at the specified mapindex
  int IsActive(int index) { return (plist[index] != 0); }

  // Sets trigger volume on specified index
  // If index is not playing, activates the index
  void SetTriggerVol(int index, float vol);

  // Gets triggered volume on specified index
  // If index is not playing, returns 0
  float GetTriggerVol(int index);

  // Returns the status at the specified index
  inline LoopStatus GetStatus(int index) { return status[index]; };

  inline Processor *GetProcessor(int index) { return plist[index]; };

  // Returns a loop with the specified index, if one exists
  Loop *GetSlot(int index);

  void AdjustNewLoopVolume(float adjust) {
    newloopvol += adjust;
    if (newloopvol < 0.0)
      newloopvol = 0.0;
  }
  float GetNewLoopVolume() { return newloopvol; }

  void AdjustOutputVolume(float adjust);
  void SetOutputVolume(float set);
  float GetOutputVolume();

  void AdjustInputVolume(float adjust);
  void SetInputVolume(float set);
  float GetInputVolume();

  void SetLoopVolume(int index, float val);
  float GetLoopVolume(int index);
  void AdjustLoopVolume(int index, float adjust);
  void SetLoopdVolume(int index, float val);
  float GetLoopdVolume(int index);

  void SelectPulse(int pulseindex);

  // Create a time pulse around the specified index
  // The length of the loop on the specified index becomes
  // a time constant around which other loops center themselves
  // subdivide the length of the loop by subdivide to get the core pulse
  void CreatePulse(int index, int pulseindex, int sub);
  
  // Creates a pulse of the given length in the first available slot,
  // if none already exists of the right length
  Pulse *CreatePulse(nframes_t len);

  // Taps a pulse- starting at the downbeat- if newlen is nonzero, the pulse's
  // length is adjusted to reflect the length between taps- and a new pulse
  // is created if none exists
  void TapPulse(int pulseindex, char newlen);

  void SwitchMetronome(int pulseindex, char active);

  inline Pulse *GetCurPulse() {
    if (curpulseindex >= 0) 
      return pulses[curpulseindex];
    else
      return 0;
  };

  inline int GetCurPulseIndex() { return curpulseindex; };
  inline Pulse *GetPulseByIndex(int pulseindex) { return pulses[pulseindex]; };

  // Deletes the specified pulse, stopping striping
  void DeletePulse(int pulseindex);

  // Gets the pulse to which the loop on index is attached
  Pulse *GetPulse(int index);

  inline void SetSubdivide(int sub) {
    //printf("Set subdivide: %d\n", sub);
    subdivide = sub;
  }
  inline int GetSubdivide() { return subdivide; };

  // Move the loop at specified index to another index
  // only works if target index is empty
  // returns 1 if success
  int MoveLoop (int src, int tgt);

  // Prompts the user for a new name for the given loop
  // We can only rename one loop at a time
  void RenameLoop(int loopid);

  virtual void ItemRenamed(char *nw);
  ItemRenamer *renamer; // Renamer instance, or null if we are not renaming
  Loop *rename_loop;    // Loop being renamed

  inline ItemRenamer *GetRenamer() { return renamer; };
  inline Loop *GetRenameLoop() { return rename_loop; };

  // Delete the loop at the specified index..
  void DeleteLoop (int index);

  // Trigger the loop at index within the map
  // The exact behavior varies depending on what is already happening with
  // this loop and the settings passed- see ~/.fweelin/.fweelin.rc
  void Activate (int index, char shot = 0, float vol = 1.0, nframes_t ofs = 0,
		 char overdub = 0, float *od_feedback = 0);

  void Deactivate (int index);

  // Saves the loop with given index
  void SaveLoop (int index);
  // Load loop from disk into the given index
  void LoadLoop(char *filename, int index, float vol = 1.0);

  // Saves a new scene of all loops
  void SaveNewScene();
  // Saves over the current scene
  void SaveCurScene();
  // Load scene from disk
  void LoadScene(SceneBrowserItem *i);

  // We receive calls periodically for saving of loops-
  // here, we return blocks to save from loops which need saving
  virtual void GetWriteBlock(FILE **out, AudioBlock **b, 
			     AudioBlockIterator **i, nframes_t *len);

  // We receive calls periodically for loading of loops-
  virtual void GetReadBlock(FILE **in, char *smooth_end);
  virtual void ReadComplete(AudioBlock *b);

  // Check if the needs_saving map is up to date, rebuild if needed.
  void CheckSaveMap();

  // Adds the loop/scene with given filename to the browser br
  void AddLoopToBrowser(Browser *br, char *filename);
  SceneBrowserItem *AddSceneToBrowser(Browser *br, char *filename);

  inline void SetAutoLoopSaving(char save) { autosave = save; };
  void AddToSaveQueue(Event *ev);

  inline int GetNumSave() { return numsave; };
  inline int GetCurSave() { return cursave; };
  inline int GetNumLoad() { return numload; };
  inline int GetCurLoad() { return curload; };

  Event *savequeue,          // Loop/scene save queue
    *loadqueue;              // Loop/scene load queue
  int cursave, curload,      // # of loops/scenes saved/loaded
    numsave, numload;        // Total # of loops/scenes to save/load
    
  int loadloopid;            // Index where to load new loops from disk
  double needs_saving_stamp; // Timestamp from trigger map from which 
                             // needs_saving was built

  Range default_looprange;   // Default placement for loops

  int *lastrecidx;   // List of last indexes recorded to
  int numloops,      // Total number of loops in map
    numrecordingloops; // Number of loops currently recording in map

  inline void LockLoops() {
    pthread_mutex_lock (&loops_lock);
  };
  inline void UnlockLoops() {
    pthread_mutex_unlock (&loops_lock);
  };

 protected:

  // Rename a loop in memory (threadsafe)
  inline void RenameLoop(Loop *l, char *nw) {
    LockLoops();
    if (l->name != 0) // Erase loop stored name
      delete[] l->name;
    if (nw != 0) {
      l->name = new char[strlen(nw)+1];
      strcpy(l->name,nw);
    } else
      l->name = 0;
    UnlockLoops();
  };

  // Adds the given loop to the list of loops to save
  void AddLoopToSaveQueue(Loop *l);
  // Adds the given loop to the list of loops to load
  void AddLoopToLoadQueue(char *filename, int index, float vol);

  // Saves loop XML data & prepares to save loop audio
  void SetupSaveLoop(Loop *l, int l_idx, FILE **out, AudioBlock **b, 
		     AudioBlockIterator **i, nframes_t *len);
  // Loads loop XML data & prepares to load loop audio-
  // returns nonzero on error
  int SetupLoadLoop(FILE **in, char *smooth_end, 
		    Loop **new_loop, int l_idx, float l_vol,
		    char *l_filename);

  // Setup time marker striping on audio memory when a new
  // pulse is selected
  void StripePulseOn(Pulse *pulse);
  
  // Deactivate time marker striping on audio memory for the
  // given pulse
  void StripePulseOff(Pulse *pulse);

  // Turn on/off auto loop saving
  char autosave; // Autosave loops?

  // Core Fweelin app on which this player is working on 
  Fweelin *app;

  // We also need to keep track of record/playprocessors attached to each
  // index
  Processor **plist;  // Pointer to a processor for each index
  // *** Replace this with well designed event queue system
  int *waitactivate;  // For each index, are we waiting to activate?
  float *waitactivate_vol;
  char *waitactivate_od,
    *waitactivate_shot;
  float **waitactivate_od_fb;
  LoopStatus *status; // For each index, what's the status?

  // Block managers that load/save loops
  BlockReadManager *bread;
  BlockWriteManager *bwrite;

  // Initial volume of new loops
  float newloopvol;

  // Subdivision for creating new pulses
  int subdivide;
  // Index of last deactivated trigger index.. used when creating new pulses
  int lastindex;
  // Current pulse (index) for new loops.. will be quantized to this pulse
  int curpulseindex;
  // List of possible pulses
  Pulse *pulses[MAX_PULSES];
  
  pthread_mutex_t loops_lock; // A way to lock up loops so two threads
                              // don't race on one loop
};

// This is Fweelin
class Fweelin : public EventProducer, public BrowserCallback {
 public:

  Fweelin() : 

#if USE_FLUIDSYNTH
    fluidp(0), 
#endif

    mmg(0), bmg(0), emg(0), rp(0), tmap(0), 
    loopmgr(0), browsers(0), abufs(0), iset(0), audio(0), midi(0), sdlio(0), 
    vid(0), scope(0), scope_len(0), audiomem(0), amrec(0),  
    running(0) {};
  ~Fweelin() {};

  char IsRunning() { return running; };

  // Setup
  int setup();
  // Start
  int go();

  void ToggleDiskOutput();
  void FlushStreamOutName() { strcpy(streamoutname,""); };

  inline nframes_t getBUFSZ() { return fragmentsize; };
  inline sample_t *getSCOPE() { return scope; };
  inline nframes_t getSCOPELEN() { return scope_len; };

#if USE_FLUIDSYNTH
  inline FluidSynthProcessor *getFLUIDP() { return fluidp; };
  FluidSynthProcessor *fluidp;
#endif

  inline AudioBlock *getAUDIOMEM() { return audiomem; };
  AudioBlockIterator *getAUDIOMEMI();
  inline RecordProcessor *getAMREC() { return amrec; };
  AudioBlock *getAMPEAKS();
  AudioBlock *getAMAVGS();
  AudioBlockIterator *getAMPEAKSI();
  AudioBlockIterator *getAMAVGSI();
  BED_MarkerPoints *getAMPEAKSPULSE();

  inline MemoryManager *getMMG() { return mmg; };
  inline BlockManager *getBMG() { return bmg; };
  inline EventManager *getEMG() { return emg; };
  inline RootProcessor *getRP() { return rp; };
  inline FileStreamer *getSTREAMER() { return fs; };
  inline TriggerMap *getTMAP() { return tmap; };
  inline LoopManager *getLOOPMGR() { return loopmgr; };
  
  inline AudioIO *getAUDIO() { return audio; };
  inline MidiIO *getMIDI() { return midi; };
  inline VideoIO *getVIDEO() { return vid; };
  inline SDLIO *getSDLIO() { return sdlio; };

  inline FloConfig *getCFG() { return cfg; };

  inline char *getSTREAMOUTNAME() { return streamoutname; };

  inline SceneBrowserItem *getCURSCENE() { return curscene; };
  inline void setCURSCENE(SceneBrowserItem *nw) { curscene = nw; };

  inline PreallocatedType *getPRE_EXTRACHANNEL() { return pre_extrachannel; };
  inline PreallocatedType *getPRE_AUDIOBLOCK() { return pre_audioblock; };
  inline PreallocatedType *getPRE_TIMEMARKER() { return pre_timemarker; };

  inline AudioBuffers *getABUFS() { return abufs; };
  inline InputSettings *getISET() { return iset; };

  inline Browser *getBROWSER(BrowserItemType b) { 
    if (b >= 0 && b < B_Last)
      return browsers[b]; 
    else
      return 0;
  }

  // Patch browser callbacks
  virtual void ItemBrowsed(BrowserItem *i) { ItemSelected(i); };
  virtual void ItemSelected(BrowserItem *i);
  // Patches can not yet be renamed
  virtual void ItemRenamed(BrowserItem *i) { return; }; 

 private:
  MemoryManager *mmg;
  BlockManager *bmg;
  EventManager *emg;
  RootProcessor *rp;
  TriggerMap *tmap;
  LoopManager *loopmgr;

  Browser **browsers;

  Browser *GetBrowserFromConfig(BrowserItemType b);

  // ****************** PREALLOCATED TYPE MANAGERS
  PreallocatedType *pre_audioblock,
    *pre_extrachannel,
    *pre_timemarker;

  // ******************  DISK STREAMER
  FileStreamer *fs;
  int writenum; // Number of audio output file currently being written
  char streamoutname[FWEELIN_OUTNAME_LEN], // Name of output file
    timingname[FWEELIN_OUTNAME_LEN];       // Name of timing stripe file

  // ******************  SCENE INFO
  SceneBrowserItem *curscene;

  // ****************** SYSTEM LEVEL AUDIO  
  // Audio buffers
  AudioBuffers *abufs;
  // Input settings
  InputSettings *iset;
  // Audio interface
  AudioIO *audio;

  // ****************** MIDI
  MidiIO *midi;
  
  // ****************** SDL INPUT (KEYBOARD / MOUSE / JOYSTICK)
  SDLIO *sdlio;
  
  // ****************** VIDEO
  VideoIO *vid;

  // Audio buffer size
  nframes_t fragmentsize;

  // Buffers for visual sample scope
  sample_t *scope;
  nframes_t scope_len;

  // Audio memory & the processor which records to it
  AudioBlock *audiomem;
  RecordProcessor *amrec;

  // Control settings 
  FloConfig *cfg;
  
  char running; // Nonzero if FW is fully started
};

#endif
