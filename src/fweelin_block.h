#ifndef __FWEELIN_BLOCK_H
#define __FWEELIN_BLOCK_H

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

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

#ifdef __MACOSX__
#include <Sndfile/sndfile.h>
#else
#include <sndfile.h>
#endif

#include "fweelin_audioio.h"
#include "fweelin_mem.h"
#include "fweelin_event.h"


// Types of audio codecs supported
typedef enum {
  UNKNOWN = -1,
  FIRST_FORMAT = 0, 
  VORBIS = 0,
  WAV = 1,
  FLAC = 2,
  AU = 3,
  END_OF_FORMATS = 4
} codec;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

class Fweelin;
class Loop;
class AudioBlock;
class BlockManager;
class PeaksAvgsManager;

// A list of all block extended data types
// Used as a kind of RTTI for getting BED of a certain type at runtime
enum BlockExtendedDataType {
  T_BED_None,
  T_BED_ExtraChannel,
  T_BED_PeaksAvgs,
  T_BED_MarkerPoints
};

class BlockExtendedData {
 public:
  
  BlockExtendedData() : next(0) {};
  BlockExtendedData(BlockExtendedData *prev) : next(0) {
    prev->next = this;
  };
  virtual ~BlockExtendedData() {};

  virtual BlockExtendedDataType GetType() { return T_BED_None; };

  // Pointer to next bit of extended data in list
  BlockExtendedData *next;
};

// A type of block extended data that allows
// peaks & averages across audioblocks to be stored inside them.
// Used for things like scope computations which are autocalculated 
// and stored inside blocks as BED_PeaksAvgs
class BED_PeaksAvgs : public BlockExtendedData {
 public:
  BED_PeaksAvgs(AudioBlock *peaks, AudioBlock *avgs, nframes_t chunksize) 
    : peaks(peaks), avgs(avgs), chunksize(chunksize) {};
  ~BED_PeaksAvgs(); 

  virtual BlockExtendedDataType GetType() { return T_BED_PeaksAvgs; };

  // Peaks & averages
  AudioBlock *peaks,
    *avgs; 

  // Chunk size across which peaks & averages are computed
  // Length of peaks & avgs blocks = length of parent block / chunksize
  nframes_t chunksize; 
};

class TimeMarker : public Preallocated {
 public:
  TimeMarker(nframes_t markofs = 0, long data = 0) : markofs(markofs), 
    data(data), next(0) {};
  // FWMEM_DEFINE_DELBLOCK;
  
  virtual void Recycle() {
    markofs = 0;
    data = 0;
    next = 0;
  };

  // Block mode allocate
  /* virtual Preallocated *NewInstance() { 
    return ::new TimeMarker[GetMgr()->GetBlockSize()]; 
    }; */
  
  virtual Preallocated *NewInstance() { return ::new TimeMarker(); };

  nframes_t markofs; // Marker position measured in samples
  long data;         // Unspecified extra data (use as req'd)
  TimeMarker *next;  // Next time marker
};

// A type of block extended data that stores time markers into a block
class BED_MarkerPoints : public BlockExtendedData {
 public:
  BED_MarkerPoints() : markers(0) {};
  ~BED_MarkerPoints();

  virtual BlockExtendedDataType GetType() { return T_BED_MarkerPoints; };

  int CountMarkers();

  // Returns the nth marker before (in time) the current offset passed
  // This method correctly handles a reverse wrap case when the
  // marker NBeforeCur is farther ahead in the attached block than the
  // current offset
  TimeMarker *GetMarkerNBeforeCur(int n, nframes_t curofs);

  TimeMarker *markers;
};

class AudioBlock : public Preallocated {
public:
  // Default length of new audio blocks (samples)
  static const nframes_t AUDIOBLOCK_DEFAULT_LEN = 20000,
    AUDIOBLOCK_SMOOTH_ENDPOINTS_LEN = 64;

  // Create a new audioblock as the beginning of a block list
  AudioBlock(nframes_t len = AUDIOBLOCK_DEFAULT_LEN);

  // Create a new audioblock and link up the specified block to it
  AudioBlock(AudioBlock *prev, nframes_t len);

  virtual ~AudioBlock();

  virtual Preallocated *NewInstance() { return ::new AudioBlock(); };

  // Is this block stereo? (does it have a BED_ExtraChannel attached?) 
  inline char IsStereo() {
    return (GetExtendedData(T_BED_ExtraChannel) != 0);
  };

  // Link us up to the specified block
  void Link(AudioBlock *to);

  // Clears this block chain- not changing length
  void Zero();

  // Erases any blocks in the chain after this block- this one
  // becomes the last
  void ChopChain();

  // Smooth an audio chain-
  // either smooth the beginning into the end (smoothtype == 1)
  // or smooth the end into the beginning     (smoothtype == 0)
  void Smooth(char smoothtype = 1, 
              nframes_t smoothlen = AUDIOBLOCK_SMOOTH_ENDPOINTS_LEN);

  // Erases this whole audioblock chain from first to last
  // Also erases any extended data!
  // RT safe
  void DeleteChain();

  // Returns the total length of this audio chain
  nframes_t GetTotalLen();

  // Gets extended data for this block
  BlockExtendedData *GetExtendedData(BlockExtendedDataType x);

  // Add this extended data to the list of extended data for this block
  void AddExtendedData(BlockExtendedData *nw);

  // Finds the audioblock and offset into that block that correspond
  // to the provided absolute offset into this chain 
  void SetPtrsFromAbsOffset(AudioBlock **ptr, nframes_t *blkofs, 
                            nframes_t absofs);

  // Generates a subchain of AudioBlocks by copying the samples between offsets
  // from & to.. offsets expressed absolutely with reference to
  // beginning of this chain!
  // Samples are copied up to but not including toofs
  // If toofs < fromofs, generates a subblock that includes the end of
  // the block and the beginning up to toofs (wrap case)
  // Realtime safe?
  AudioBlock *GenerateSubChain(nframes_t fromofs, nframes_t toofs,
                               char copystereo);

  // Removes the last 'hacklen' samples from this block chain
  // Returns nonzero on error!
  // Not realtime safe!
  int HackTotalLengthBy(nframes_t hacklen);

  // Inserts the new block at the beginning of this block chain
  // Returns a pointer to the new first block
  AudioBlock *InsertFirst(AudioBlock *nw);

  sample_t *buf, // Samples for block- this pointer can be adjusted if the
                 // block is shortened as in Smooth()
    *origbuf;    // Original unmodified sample pointer
  nframes_t len; // Length of block
  AudioBlock *next, // Next block
    *first;         // First block in chain

  // Extended data list for this block
  BlockExtendedData *xt;
};

// A type of block extended data that allows
// an extra channel of audio to be attached to an audioblock
// Used for stereo loops
class BED_ExtraChannel : public Preallocated, public BlockExtendedData {
 public:
  BED_ExtraChannel(nframes_t len = AudioBlock::AUDIOBLOCK_DEFAULT_LEN);
  virtual ~BED_ExtraChannel(); 
  
  virtual Preallocated *NewInstance() { return ::new BED_ExtraChannel(); };

  virtual BlockExtendedDataType GetType() { return T_BED_ExtraChannel; };
  
  // Sample data for extra channel- each stereo block must have an instance
  // of BED_ExtraChannel with a sample buffer that matches length with
  // the parent AudioBlock 
  sample_t *buf, // This pointer can be adjusted in Smooth()
    *origbuf;    // Original unmodified sample pointer
};

// Iterator for storing/extracting data in audio blocks.
// Freewheeling stores audio in small blocks, which are linked together
// in a chain. 
//
// The main functions for iteration are GetFragment (retrieves an audio
// fragment from block(s)), PutFragment (stores an audio fragment into
// block(s)), and NextFragment (advances the iterator).
//
// The iterator can also perform rate scaling.
//
// When rate scaling, the iterator writes to a different block chain than
// it reads from. The chain can then grow/shrink as the rate scaling 
// changes.
class AudioBlockIterator /*: public Elastin_SampleFeed*/ {
public:
  // Optionally pass preallocatedtype for extrachannel if you want
  // extra channels to be added as needed when putting fragments
  AudioBlockIterator(AudioBlock *firstblock, nframes_t fragmentsize,
                     PreallocatedType *pre_extrachannel = 0);
  ~AudioBlockIterator();

  inline float round(float num) {
    if (num-(long)num < 0.5)
      return floor(num);
    else
      return ceil(num);
  }

  // Stores in cnt the absolute count corresponding to the given
  // block and offset
  void GenCnt(AudioBlock *blk, nframes_t blkofs, nframes_t *cnt);

  void GenConstants();

  // Moves iterator to start position
  void Zero();

  // Advances to the next fragment
  void NextFragment();

  // Jumps to an absolute offset within the blockchain
  void Jump(nframes_t ofs);

  // PutFragment stores the specified fragment back into this AudioBlock
  // (optional right channel)
  // Returns nonzero if the end of the block is reached, and we wrap to next  
  // Optional size_override puts a different size fragment into the block-
  // any size can be put so long as the chain is long enough
  // Optional wait_alloc waits for allocation of new extra channels if needed
  // (don't use this flag in RT!)
  
  // If rate scaling is active, the iterator always writes to a new
  // block, because the length of the newly stored sample may be
  // different than the length of the original (see 'write block' 
  // variables). No scaling is done in PutFragment, just a choice of
  // which block chain to write to
  int PutFragment (sample_t *frag_l, sample_t *frag_r, 
                   nframes_t size_override = 0, char wait_alloc = 0);

  // Returns the current fragment of audio
  // Points frag_l and frag_r buffers to the current fragment
  // frag_r is optional
  // nextblock and nextblkofs become the new block pointer and offset
  // for the next fragment
  void GetFragment(sample_t **frag_l, sample_t **frag_r);

  // Adjusts the block chain so that the current iterator position
  // becomes the new end of the chain
  void EndChain();

  inline nframes_t GetTotalLength2Cur() { return (nframes_t) curcnt; }
  inline char IsStopped() { return stopped; };
  inline void Stop() { stopped = 1; };

  // Returns the block currently being iterated through
  inline AudioBlock *GetCurBlock() { return curblock; };

 private:

  // Optional right audio channel for the block we are iterating
  PreallocatedType *pre_extrachannel; // Preallocator for 2nd channel blocks
  BED_ExtraChannel *currightblock,
    *nextrightblock,

    // Values for write block
    *currightblock_w,
    *nextrightblock_w;

  AudioBlock *curblock,
    *nextblock,

    // Values for write block
    *curblock_w,
    *nextblock_w;

  double curblkofs, // Use doubles for position (rate scale needs them)
    nextblkofs,
    curcnt,
    nextcnt,

    // Values for write block
    curblkofs_w,
    nextblkofs_w,
    curcnt_w,
    nextcnt_w;


  // Buffers for storing smaller fragments from within AudioBlocks
  nframes_t fragmentsize;
  sample_t *fragment[2];

  char stopped; // Nonzero if this iterator is stopped
};

// List of all types of chain managers
enum ManagedChainType {
  T_MC_None,
  T_MC_GrowChain,
  T_MC_PeaksAvgs,
  T_MC_BlockRead,
  T_MC_BlockWrite,
  T_MC_HiPri,
  T_MC_StripeBlock
};

// Status of managed chain
enum ManagedChainStatus {
  T_MC_Running,
  T_MC_PendingDelete
};

// Generic class specifying a chain of blocks & iterator to manage
// Management happens when blockmanager periodically calls
// the Manage() method for all managed chains.
// Different types of manager classes do different management tasks
// with blocks.
class ManagedChain : public Preallocated {
public:
  ManagedChain(AudioBlock *b = 0, AudioBlockIterator *i = 0) : 
    b(b), i(i), next(0) {};
  virtual ~ManagedChain() {};

  virtual Preallocated *NewInstance() { return ::new ManagedChain(); };

  // Called periodically to manage a block
  // Return zero to proceed normally
  // Return nonzero to delete this manager 
  virtual int Manage() { return 0; };

  // This method is called whenever an object (ref) is deleted
  // that Managed Chains might want to know about. For example,
  // RootProcessor notifies BlockManager whenever child processors
  // are deleted. This allows ManagedChains to react, if they depend
  // on the deleted object. If we return nonzero, this manager is
  // deleted.
  virtual int RefDeleted(void *ref) { 
    if (ref == b || ref == i) 
      // Block or iterator gone!! End this manager!
      return 1;
    else
      return 0;
  };

  virtual ManagedChainType GetType() { return T_MC_None; };

  AudioBlock *b;
  AudioBlockIterator *i;
  
  ManagedChainStatus status;
  ManagedChain *next;
};

// GrowChainManager periodically grows a block chain so that the
// iterator i never reaches its end-- good for unlimited length records
class GrowChainManager : public ManagedChain {
 public:
  GrowChainManager(AudioBlock *b = 0, AudioBlockIterator *i = 0) :
    ManagedChain(b,i) {};

  virtual Preallocated *NewInstance() { return ::new GrowChainManager(); };

  virtual ManagedChainType GetType() { return T_MC_GrowChain; };

  virtual int Manage();
};

// Base class for different types of file encoders
class iFileEncoder {
 public:
  iFileEncoder (Fweelin *app, char stereo);
  virtual ~iFileEncoder() {};

  // Tell the encoder to dump to this file we have just opened
  // This writes the files header information 
  // Returns nonzero on error.
  virtual int SetupFileForWriting (FILE *file) = 0;

  // Get the samples from the relevant buffer (ibuf) and write them to file,
  // (startframe) tells us what index to start copying from the input buffer,
  // (numframes) tells us how many frames to encode.
  // Returns the number of frames written.
  virtual long int WriteSamplesToDisk (sample_t **ibuf, nframes_t startframe, nframes_t numframes) = 0;

  // Stop encoding and finish up- but don't close output file!
  virtual void PrepareFileForClosing () = 0;

  // Vorbis in particular needs to a bit of scaling down to avoid clipping... 
  // any simular processing should be put here
  virtual void Preprocess (sample_t *l, sample_t *r, nframes_t len) = 0;

  inline char IsStereo() { return stereo; };

 protected:

  Fweelin *app;
  char stereo;
  FILE *outfd;
};

class SndFileEncoder : public iFileEncoder {
 public:
  // Encoder for libSndfile formats- at the moment, WAV, AU and FLAC.
  // Maxframes is maximum number of frames to write in one call to 
  // WriteSamplesToDisk
  // Codec type is one of those values.
  // We also specify wether we are using Mono or Stereo Encoding
  SndFileEncoder (Fweelin *app, nframes_t maxframes, char stereo, codec type);
  ~SndFileEncoder() {
    if (tbuf != 0)
      delete[] tbuf;
  };
  
  int SetupFileForWriting (FILE *file);
  long int WriteSamplesToDisk (sample_t **ibuf, nframes_t startframe, nframes_t numframes);
  void PrepareFileForClosing (void);
  void Preprocess (sample_t */*left*/, sample_t */*right*/, nframes_t /*n_frames*/) {};

 private:

  codec filetype;
  int samplesize;
  SF_INFO sfinfo;
  SNDFILE *sndoutfd;
  float *tbuf; // Temporary audio buffer for saving
};

class VorbisEncoder : public iFileEncoder {
 public:

  // Vorbis encoder library init/end are done in constructor/destructor
  // We specify stereo or mono encoding
  VorbisEncoder (Fweelin *app, char stereo);
  virtual ~VorbisEncoder();

  int SetupFileForWriting (FILE *file);
  long int WriteSamplesToDisk (sample_t **ibuf, nframes_t startframe, nframes_t numframes);
  void PrepareFileForClosing (void) {
    // Tell vorbis we are done with the stream
    vorbis_analysis_wrote(&vd,0);
    // Encode any remaining stuff
    Encode();
  };
  void Preprocess (sample_t *l, sample_t *r, nframes_t len);

 private:

  // Returns vorbis encoder's analysis buffers for len frames
  // Depending on mono/stereo it is an array of 1/2 by len samples
  float **GetAnalysisBuffer(nframes_t len) { return vorbis_analysis_buffer(&vd,len); };

  // Tell vorbis we wrote some samples to its analysis buffer
  void WroteToBuffer(nframes_t len) { vorbis_analysis_wrote(&vd,len); };

  // Runs the encoder, returns number of bytes written
  long int Encode();

  // Vorbis encoder stuff
  ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */
  
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                          settings */
  vorbis_comment   vc; /* struct that stores all the user comments */

  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
};


class iFileDecoder {
 public:
  iFileDecoder (Fweelin *app);
  virtual ~iFileDecoder () {};
  
  // Tell the decoder to decode from a given file
  // This reads the files header
  virtual int ReadFromFile(FILE *in, nframes_t /*rbuf_len*/) = 0;
  // Decode a maximum of max_len samples and return buffers in pcm_channels
  // (format as per VorbisFile ov_read_float)
  // Returns # of samples read, 0 if EOF, <0 if error
  virtual int ReadSamples(AudioBlockIterator *i, nframes_t max_len) = 0;
  // Stop decoding and finish up-- and close the input file!
  // ??? -- old note? FIXME workaround for memory managment issue
  virtual void Stop() = 0;

  // Are we decoding in stereo? 
  inline char IsStereo() { return stereo; };

 protected:

  // Fweelin
  Fweelin *app; 
  
  // Stereo decoding?
  char stereo;

  // Input from this file
  FILE *infd;
};

class SndFileDecoder : public iFileDecoder {
 public:
  SndFileDecoder(Fweelin *app, codec type);
  ~SndFileDecoder();
  
  int ReadFromFile (FILE *in, nframes_t rbuf_len);
//   int Decode (float **pcm_channels, nframes_t max_len);
  int ReadSamples(AudioBlockIterator *it, nframes_t max_len);
  void Stop();

 private:

  codec filetype;
  int samplesize;
  SF_INFO sfinfo;
  SNDFILE *sfinfd;
  float *obuf[2];
  float *rbuf;
};

class VorbisDecoder : public iFileDecoder {
 public:

  // Vorbis library init/end are done in constructor/destructor
  VorbisDecoder(Fweelin *app);
  ~VorbisDecoder();

  // Decode a maximum of max_len samples and return buffers in pcm_channels
  // (format as per VorbisFile ov_read_float)
  // Returns # of samples read, 0 if EOF, <0 if error
  int ReadSamples(AudioBlockIterator *i, nframes_t max_len);
//   int Decode(float **pcm_channels, nframes_t max_len);

  // Stop decoding and finish up-- and close the input file!
  void Stop() {
    // Tell vorbis we are done with the stream
    ov_clear(&vf);
  };


  // Tell the decoder to decode from a given file
  // This reads the vorbis header
  int ReadFromFile(FILE *in, nframes_t /*rbuf_len*/);

 private:

  // Vorbis decoder stuff
  OggVorbis_File vf;
  int current_section;
  char pcmout[];
};

class AutoWriteControl {
 public:
  // This callback method is called periodically by BlockWriteManager
  // to get the next block chain to write. This allows the main app to 
  // decide which loops to save, while the BlockManager thread does the work
  // in the background
  virtual void GetWriteBlock(FILE **out, AudioBlock **b, 
                             AudioBlockIterator **i, nframes_t *len) = 0;
};

class AutoReadControl {
 public:
  // This callback method is called periodically by BlockReadManager
  // to get the next block chain to read. This allows the main app to 
  // decide which loops to load, while the BlockManager thread does the work
  // in the background
  //
  // GetReadBlock sets 'smooth_end' to nonzero to request for
  // BlockReadManager to smooth the end of the block into the beginning.
  // This is the new way to deal with begin-end inconsistencies when loading
  // OGG loops. To accomodate loops saved without the extra data at the end,
  // we can set 'smooth_end' to zero.
  virtual void GetReadBlock(FILE **in, char *smooth_end) = 0;
  // When the audio data read is complete, BlockReadManager calls ReadComplete
  // which tells you that you have a new loop in memory
  virtual void ReadComplete(AudioBlock *b) = 0;
};

// BlockReadManager reads & uncompresses an audio block chain
// this implementation is used for loading loops.
class BlockReadManager : public ManagedChain {
 public:

  // Crossfade length when loading old loops (not the new format ones)
  const static nframes_t SMOOTH_FIX_LEN = 64, 
    DECODE_CHUNKSIZE = 2048;
  // Increase this number will result in faster decoding but slower response
  // during decoding (less CPU given up for other Manage tasks)
  const static int NUM_DECODE_PASSES = 100;

  BlockReadManager(FILE *in = 0, AutoReadControl *arc = 0, 
                   BlockManager *bmg = 0, nframes_t peaksavgs_chunksize = 0);
  virtual ~BlockReadManager();

  virtual Preallocated *NewInstance() { return ::new BlockReadManager(); };

  void SetLoopType (codec looptype);
  
  // BlockReadManager's RefDeleted may block to avoid block data being
  // deleted while we are still encoding- we finish up our current decode pass
  // and then return
  virtual int RefDeleted(void *ref) { 
    if (ref == arc) {
      // AutoReadControl is ending, so we should end too
      arc = 0;
      return 1;
    }
    else
      return 0;
  };

  // Start decoding into memory from the given file
  void Start(FILE *new_in = 0);

  // Ends reading- with error (nonzero) or without error (zero)
  void End(char error);

  virtual ManagedChainType GetType() { return T_MC_BlockRead; };

  virtual int Manage();

  FILE *in;                 // File to read from
  char smooth_end;          // Smooth end of loop into beginning?
  AutoReadControl *arc;     // A way to ask app what blocks to read
  iFileDecoder *dec;        // Decoder
  BlockManager *bmg; 
  codec filetype;

  PeaksAvgsManager *pa_mgr; // Computer for peaks/avgs during load
  nframes_t peaksavgs_chunksize;

  pthread_mutex_t decode_lock;
};

// BlockWriteManager compresses and writes a block chain (OGG vorbis format)
// this implementation is used for saving loops,
// unlike the BlockStreamer implementation in core_dsp, which streams
// real-time
//
// Two modes are provided-
// 1) follow iterator i-- compresses behind iterator i
// 2) compress the whole chain
//
// We can also choose to invoke BlockWriteManager with AutoWriteControl,
// providing a callback which we invoke after writing a block chain. 
// The callback tells us whether to write another chain, and if so,
// gives us the new block and iterator pointers as well as a file out
class BlockWriteManager : public ManagedChain {
 public:
  const static nframes_t ENCODE_CHUNKSIZE = 10000,
    ENCODE_CROSSOVER_LEN = 1000;

  BlockWriteManager(FILE *out = 0, AutoWriteControl *awc = 0, 
                     BlockManager *bmg = 0, AudioBlock *b = 0, 
                     AudioBlockIterator *i = 0);
  virtual ~BlockWriteManager();

  virtual Preallocated *NewInstance() { return ::new BlockWriteManager(); };
  
  // BlockWriteManager's RefDeleted may block to avoid block data being
  // deleted while we are still encoding- we finish up our current encode pass
  // and then return
  virtual int RefDeleted(void *ref) { 
    if (ref == awc) {
      // AutoWriteControl is ending, so we should end too
      awc = 0;
      return 1;
    } else if (ref == b || ref == i) {
      // Block or iterator gone!! The chain we are encoding has been erased-
      // so stop encoding
      printf("DISK: Blocks deleted while saving- abort!\n");
      End();
      if (awc != 0)
        // Auto save, so keep running
        return 0;
      else 
        // Single save, so stop
        return 1;
    }
    else
      return 0;
  };

  // Start encoding the given block chain to the given file
  void Start(FILE *new_out = 0, AudioBlock *new_b = 0, 
             AudioBlockIterator *new_i = 0, nframes_t new_len = 0);

  // Ends writing
  void End();

  virtual ManagedChainType GetType() { return T_MC_BlockWrite; };

  virtual int Manage();

  FILE *out;              // File to write this loop to
  AudioBlockIterator *ei; // Encode iterator
  nframes_t len,          // Length of block to save
    pos;                  // Current save position
  AutoWriteControl *awc;  // A way to ask app what blocks to write
  iFileEncoder *enc;
  BlockManager *bmg; 

  pthread_mutex_t encode_lock;
};

// PeaksAvgsManager periodically calculates peaks and averages for
// blockchain b, keeping up with iterator i
// using BlockExtendedData to store peaks & averages 
class PeaksAvgsManager : public ManagedChain {
 public:
  PeaksAvgsManager(BlockManager *bmg = 0, 
                   AudioBlock *b = 0, AudioBlockIterator *i = 0, 
                   char grow = 0) : 
    ManagedChain(b,i), bmg(bmg), runmax(0), runmin(0), runtally(0), 
    lastcnt(0), chunkcnt(0), stereo(0), grow(grow), go(1), ended(0) {};
  virtual ~PeaksAvgsManager();

  virtual Preallocated *NewInstance() { return ::new PeaksAvgsManager(); };

  virtual int RefDeleted(void *ref) { 
    if (ref == b || ref == i) {
      // Block or iterator gone!! End this manager!
      b = 0;
      return 1;
    }
    else
      return 0;
  };

  // Call before starting!
  void Setup();

  // Ends computation of peaks & averages, chopping the
  // peaks & averages blocks at this length..
  void End();

  AudioBlockIterator *GetPeaksI() { return peaksi; };
  AudioBlockIterator *GetAvgsI() { return avgsi; };
  AudioBlock *GetPeaks() { return pa->peaks; };
  AudioBlock *GetAvgs() { return pa->avgs; };

  virtual ManagedChainType GetType() { return T_MC_PeaksAvgs; };

  virtual int Manage();

  BlockManager *bmg; 

  // Target place to store peaks & averages
  BED_PeaksAvgs *pa;

  // Iterators for blocks storing peaks & averages
  AudioBlockIterator *peaksi,
    *avgsi,
    *mi; // Iterator for block b for this manager

  // Used to keep syncronized up to latest changes to block b
  sample_t runmax, // Running sample maximum
    runmin,        // Running sample minimum
    runtally;      // Running sample tally
  nframes_t lastcnt, // Last count (curcnt) in iterator i
    chunkcnt;        // Count in current chunk 
                     // (where peaks & avgs are calculated)

  char stereo;    // Computing peaks from stereo block?

  char grow,      // Nonzero if peaks & avgs data should be grown
                  // (if input block b is also growing)
    go,           // Nonzero if we are computing peaks & averages
    ended;        // Nonzero if we have ended for good
};

// Base class for hipriority managed blocks--
// When a block becomes HiPriManaged it specifies a trigger pointer
// A realtime process can then call BlockManager with a trigger pointer
// and invoke the Manage() method for all blocks managed with matching 
// trigger. This allows blocks to interact with other realtime 
// components, such as time pulses, without knowing what they are.
// Manage() is not called periodically, like it is with other 
// manager types-- so Manage methods must work in realtime
class HiPriManagedChain : public ManagedChain {
 public:
  HiPriManagedChain(void *trigger = 0, 
                    AudioBlock *b = 0, AudioBlockIterator *i = 0) :
    ManagedChain(b,i), trigger(trigger), lastcnt(0) {};

  virtual Preallocated *NewInstance() { 
    return ::new HiPriManagedChain();
  };

  virtual int RefDeleted(void *ref) { 
    if (ref == b || ref == i || ref == trigger) 
      // Block, iterator, or trigger gone! End this manager!
      return 1;
    else
      return 0;
  };

  virtual ManagedChainType GetType() { return T_MC_HiPri; };

  void *trigger;
  nframes_t lastcnt; // Last iterated pos in block
};

// StripeBlockManager is a special hipriority block manager
// that stripes blocks with time markers whenever it is invoked
// through BlockManager with the specified trigger pointer.
// This allows a block to register that some other realtime process
// (for example, a time pulse), can trigger time points to be 
// striped!
// We use BED_MarkerPoints to store time markers in the block
class StripeBlockManager : public HiPriManagedChain {
 public:
  StripeBlockManager(PreallocatedType *pre_tm = 0,
                     void *trigger = 0, 
                     AudioBlock *b = 0, AudioBlockIterator *i = 0) :
    HiPriManagedChain(trigger,b,i), pre_tm(pre_tm) {};

  // Call before starting!
  void Setup();

  virtual Preallocated *NewInstance() { return ::new StripeBlockManager(); };

  // Do we need a destructor that erases all striped marks??
  // Depends on desired functionality--
  // do marks persist after the managers that created them are gone?

  virtual ManagedChainType GetType() { return T_MC_StripeBlock; };

  virtual int Manage();

  // Target place to store markers
  BED_MarkerPoints *mp;
  // Preallocated manager for time markers
  PreallocatedType *pre_tm;
};

// BlockManager handles different maintenance tasks related to
// audio blocks. 
//
// It handles -periodic maintenance-, such as resizing a block chain,
// performing peak calculations, saving to disk, and analysis on a chain.
//
// It also handles -time-critical events-, such as firing off pulse sync
// messages. This second function may soon be moved to the EventManager.
class BlockManager {
public:
  BlockManager (Fweelin *app);
  ~BlockManager ();

  Fweelin *GetApp() { return app; };

  // Turns on automatic allocation of new blocks at the end of the
  // specified chain. We work in conjunction with the specified iterator, 
  // so that when the iterator approaches the end of the block chain, 
  // the chain grows automatically
  void GrowChainOn (AudioBlock *b, AudioBlockIterator *i);
  void GrowChainOff (AudioBlock *b);

  // Turns on computation of running sample peaks and averages
  // for the specified Block & Iterator
  // We compute as the currenty iterated position advances
  PeaksAvgsManager *PeakAvgOn (AudioBlock *b, AudioBlockIterator *i, 
                               char grow = 0);
  void PeakAvgOff (AudioBlock *b);

  // Stripes the specified chain with TimeMarkers according to the specified
  // trigger. Works in conjunction with RT threads that call HiPriTrigger.
  void StripeBlockOn (void *trigger, AudioBlock *b, 
                      AudioBlockIterator *i);
  // Removes striping from the specified trigger on blockchain b
  void StripeBlockOff (void *trigger, AudioBlock *b);

  // Notify all Managers that the object pointed to has been deleted-
  // To avoid broken dependencies
  void RefDeleted (void *ref);

  // Activate a hipriority trigger- all hiprimanagedchains with
  // specified trigger pointer will have manage() method called
  // RT safe!
  void HiPriTrigger (void *trigger);

  // Returns the 1st chain manager associated with block o
  // that has type t
  ManagedChain *GetBlockManager(AudioBlock *o, ManagedChainType t);

  // Generic delete/add functions for managers (not hipri)
  void DelManager (ManagedChain *m);
  void AddManager (ManagedChain *nw);

 protected:

  void DelManager (ManagedChain **first, ManagedChain *m);
  void AddManager (ManagedChain **first, ManagedChain *nw);
  void AddHiManager (HiPriManagedChain **first, HiPriManagedChain *nw);
  void RefDeleted (ManagedChain **first, void *ref);
  void HiRefDeleted (HiPriManagedChain **first, void *ref);

  // Delete a managed chain for block o and manager type t
  // If t is T_MC_None, removes the first managed chain for 'o' 
  // of any type
  void DelManager (ManagedChain **first, AudioBlock *o,
                   ManagedChainType t = T_MC_None);

  // Delete a hiprimanaged chain for block o and manager type t
  // with specified trigger.
  // If t is T_MC_None, removes the first managed chain for 'o' 
  // with specified trigger, of any type
  void DelHiManager (HiPriManagedChain **first, 
                     AudioBlock *o,
                     ManagedChainType t, void *trigger);

  // NEED TO MAKE/USE GENERALIZED LIST CLASS
  // ^^ speed issues?

  static void *run_manage_thread (void *ptr);

  ManagedChain *manageblocks;
  HiPriManagedChain *himanageblocks;

  pthread_t manage_thread;
  pthread_mutex_t manage_thread_lock;
  int threadgo;

  // ****************** PREALLOCATED TYPE MANAGERS
  PreallocatedType *pre_growchain,
    *pre_peaksavgs,
    *pre_hipri,
    *pre_stripeblock;

  // Parent app
  Fweelin *app;
};

#endif
