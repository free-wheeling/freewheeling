/* 
   Things, in their Essence,
   are not of this world.
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

#include "fweelin_block.h"
#include "fweelin_core.h"

iFileDecoder::iFileDecoder(Fweelin *app) : app(app), infd(0) {};

SndFileDecoder::SndFileDecoder(Fweelin *app, codec type) : iFileDecoder(app), 
                                                           filetype(type)
{
  memset (&sfinfo, 0, sizeof (sfinfo)) ;
  samplesize = sizeof(float);
  sfinfd = NULL;
  //   rbsize = 0;
};
SndFileDecoder::~SndFileDecoder() {};

int SndFileDecoder::ReadFromFile(FILE *in, nframes_t rbuf_len) {
  if (!(sfinfd = sf_open_fd(fileno(in), SFM_READ, &sfinfo, SF_FALSE) )) {
    printf("DISK: unable to open file for reading \n");
    return 1;
  }

  if (sfinfo.channels == 2) {
    printf("DISK: (SndFile) Stereo loop\n");
    stereo = 1;
  } else if (sfinfo.channels == 1) {
    printf("DISK: (SndFile) Mono loop\n");
    stereo = 0;
  } else {
    printf("DISK: (SndFile) Unknown # of audio channels %d\n",sfinfo.channels);
    return 1;
  }
  
  rbuf = new float[sfinfo.channels*rbuf_len];
  obuf[0] = new float[rbuf_len];
  if (stereo)
    obuf[1] = new float[rbuf_len];

  if (sfinfo.samplerate != (signed int) app->getAUDIO()->get_srate()) {
    printf("DISK: (SndFile) Audio encoded at %dHz but we are running at "
           "%dHz.\n"
           "Samplerate conversion not yet supported.\n",
           (int) sfinfo.samplerate, app->getAUDIO()->get_srate());
    return 1;
  }
    
  infd = in;
  return 0;
};

int SndFileDecoder::ReadSamples(AudioBlockIterator *it, nframes_t max_len) {
  int len = 0;

  if (stereo) {    
    if (filetype == FLAC){ // need to convert from int24
      len = sf_read_float(sfinfd,rbuf,sfinfo.channels*max_len);
      len /= sfinfo.channels;
    } else { // everything else is currently native float
      len = sf_read_raw(sfinfd,rbuf,sfinfo.channels*max_len*samplesize);
      len /= (sfinfo.channels *samplesize);
    }
    for (nframes_t i = 0; i < max_len; i++) { // deinterleave channels using brute strength
      obuf[0][i] = rbuf[2*i];
      obuf[1][i] = rbuf[2*i+1];
    }
    if (len > 0) {
      it->PutFragment(obuf[0],obuf[1],len,1);
      it->NextFragment();
    } 

  } else {
    if (filetype == FLAC) { // need to convert from int24
      len = sf_read_float(sfinfd,obuf[0],sfinfo.channels*max_len);
    } else {                // everything else is currently native float
      len = sf_read_raw(sfinfd,obuf[0],sfinfo.channels*max_len*samplesize);
    }
    if (len > 0) {
      it->PutFragment(obuf[0],0,len,1);
      it->NextFragment();
    }
  }
  return len;
};

void SndFileDecoder::Stop(){ 
  if(rbuf != NULL)
    delete[] rbuf;
  if(obuf[0] != NULL)
    delete[] obuf[0];
  if(obuf[1] != NULL)
    delete[] obuf[1];
  sf_close(sfinfd);
}

VorbisDecoder::VorbisDecoder(Fweelin *app) : iFileDecoder(app) {};
VorbisDecoder::~VorbisDecoder() {};

// Returns nonzero on error
int VorbisDecoder::ReadFromFile(FILE *in, nframes_t /*rbuf_len*/) {
  if (ov_open(in, &vf, NULL, 0) < 0) {
    printf("DISK: (VorbisFile) Input does not appear to be an Ogg "
           "bitstream.\n");
    return 1;
  }

  vorbis_info *vi = ov_info(&vf,-1);

  if (vi->channels == 2) {
    printf("DISK: (VorbisFile) Stereo loop\n");
    stereo = 1;
  }
  else if (vi->channels == 1) {
    printf("DISK: (VorbisFile) Mono loop\n");
    stereo = 0;
  }
  else {
    printf("DISK: (VorbisFile) Unknown # of audio channels %d\n",vi->channels);
    return 1;
  }

  if (vi->rate != (signed int) app->getAUDIO()->get_srate()) {
    printf("DISK: (VorbisFile) Audio encoded at %dHz but we are running at "
           "%dHz.\n"
           "Samplerate conversion not yet supported.\n",
           (int) vi->rate,app->getAUDIO()->get_srate());
    return 1;
  }
    
  infd = in;
  return 0;
};

int VorbisDecoder::ReadSamples(AudioBlockIterator *i, nframes_t max_len) {
  int len;
  float **outb;
  len = ov_read_float(&vf,&outb,max_len,&current_section);
  if (len > 0) {
    if (stereo) 
      i->PutFragment(outb[0],outb[1],len,1);
    else 
      i->PutFragment(outb[0],0,len,1);
    i->NextFragment();
  }
  return len;
};

// int VorbisDecoder::Decode(float **pcm_channels, nframes_t max_len) {
//   return ov_read_float(&vf,&pcm_channels,max_len,&current_section);
// };

iFileEncoder::iFileEncoder(Fweelin *app, char stereo) : app(app), 
                                                        stereo(stereo),
                                                        outfd(0) {
};

SndFileEncoder::SndFileEncoder (Fweelin *app, nframes_t maxframes,
                                char stereo, codec format) : 
  iFileEncoder(app,stereo), tbuf(0) {
  filetype = format;
  memset(&sfinfo, 0, sizeof (sfinfo)) ;

  // set params
  sfinfo.samplerate     = app->getAUDIO()->get_srate();
  sfinfo.frames         = 0x7FFFFFFF;
  if (stereo) {
    sfinfo.channels     = 2;
    tbuf = new float[maxframes*sfinfo.channels];
  } else 
    sfinfo.channels     = 1;

  if (filetype == FLAC)
    sfinfo.format = (SF_FORMAT_FLAC | SF_FORMAT_PCM_24);
  else 
    sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT);
  
  samplesize = sizeof(float);
//   buffer = NULL;
//   printf("SndFileEncoder init\n");
};

int SndFileEncoder::SetupFileForWriting (FILE *file) {
  if (!(sndoutfd = sf_open_fd(fileno(file), SFM_WRITE, &sfinfo, SF_FALSE))) {
    printf("DISK: Couldn't open output sound file!\n");
    return 1;
  } 
  return 0;
};

long int SndFileEncoder::WriteSamplesToDisk (sample_t **ibuf, 
                                             nframes_t startframe, 
                                             nframes_t numframes) {
  if (stereo) {
    for (nframes_t i = 0; i < numframes; i++) {
      tbuf[2*i] = ibuf[0][startframe+i];
      tbuf[2*i+1] = ibuf[1][startframe+i];
    }
    if (filetype == FLAC)
      // FIXME: perhaps some error checking
      sf_write_float(sndoutfd, tbuf, numframes*sfinfo.channels); 
    else
      sf_write_raw(sndoutfd, static_cast<void *>(tbuf),
                   numframes*samplesize*sfinfo.channels);
  } else {
    if (filetype == FLAC)
      // FIXME: perhaps some error checking
      sf_write_float(sndoutfd, ibuf[0], numframes); 
    else
      sf_write_raw(sndoutfd, ibuf[0], numframes*samplesize);
  }
  
  return (long int) numframes;
}

void SndFileEncoder::PrepareFileForClosing(){ 
  sf_close(sndoutfd);
}

VorbisEncoder::VorbisEncoder(Fweelin *app, char stereo) : iFileEncoder(app,stereo) {
  // Setup vorbis
  vorbis_info_init(&vi);
  if (vorbis_encode_init_vbr(&vi,(stereo ? 2 : 1),app->getAUDIO()->get_srate(),
                             app->getCFG()->GetVorbisEncodeQuality()))
    return;
  
  // Comment
  vorbis_comment_init(&vc);
  vorbis_comment_add_tag(&vc,"ENCODER","FreeWheeling");
  
  // Analysis state/Aux encoding storage
  vorbis_analysis_init(&vd,&vi);
  vorbis_block_init(&vd,&vb);
  
  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
     chained streams just by concatenation */
  srand(time(NULL));
  ogg_stream_init(&os,rand());
};

VorbisEncoder::~VorbisEncoder() {
  /* Vorbis clean up and exit.  vorbis_info_clear() must be called last */  
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
};

int VorbisEncoder::SetupFileForWriting(FILE *out) {
  outfd = out;

  // Write vorbis header
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;
  
  vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
  ogg_stream_packetin(&os,&header); /* automatically placed in its own page */
  ogg_stream_packetin(&os,&header_comm);
  ogg_stream_packetin(&os,&header_code);
  
  while(1) {
    int result = ogg_stream_flush(&os,&og);
    if (result == 0)
      break;
    fwrite(og.header,1,og.header_len,outfd);
    fwrite(og.body,1,og.body_len,outfd);
  }
  return 0;
};


long int VorbisEncoder::WriteSamplesToDisk (sample_t **ibuf, 
                                            nframes_t startframe, 
                                            nframes_t numframes) {
  // Here we make assumption that sample_t is equivalent to float
  // And that there is only 1 vorbis channel of data
  float **obuf = GetAnalysisBuffer(numframes);
    
  if (stereo)  { 
    memcpy(obuf[0], &ibuf[0][startframe], sizeof(float) * numframes);
    memcpy(obuf[1], &ibuf[1][startframe], sizeof(float) * numframes);
  } else {
    memcpy(obuf[0], &ibuf[0][startframe], sizeof(float) * numframes);
  }
  
  WroteToBuffer(numframes);
  Encode();
  return (long int) numframes;
};

// This analyzes and dumps any remaining frames out to file
long int VorbisEncoder::Encode() {
  if (outfd != 0) {
    long int outputsize = 0;
    
    // Now do the real analysis!! Meat n potatoes!
    while(vorbis_analysis_blockout(&vd,&vb) == 1) {
      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&vb,NULL);
      vorbis_bitrate_addblock(&vb);
      
      while(vorbis_bitrate_flushpacket(&vd,&op)) {
        /* weld the packet into the bitstream */
        ogg_stream_packetin(&os,&op);
        
        /* write out pages (if any) */
        char eos = 0;
        while (!eos) {
          int result = ogg_stream_pageout(&os,&og);
          if (result == 0)
            break;
          fwrite(og.header,1,og.header_len,outfd);
          fwrite(og.body,1,og.body_len,outfd);
          
          // Add to the output size
          outputsize += og.header_len;
          outputsize += og.body_len;
          
          if (ogg_page_eos(&og))
            eos = 1;
        }
      }
    }

    return outputsize;
  } else {
    printf("BLOCK: ERROR: OGG encoder finished but Encode() called!\n");
    return 0;
  }
}

void VorbisEncoder::Preprocess(sample_t *l, sample_t *r, nframes_t len) {
  if (r == 0) //mono
    for (nframes_t i = 0; i < len; i++)
      l[i] *= 0.9;
  else {
    for (nframes_t i = 0; i < len; i++) {
      l[i] *= 0.9;
      r[i] *= 0.9;
    }
  }
};

BlockReadManager::BlockReadManager(FILE *in, AutoReadControl *arc, 
                                   BlockManager *bmg, 
                                   nframes_t peaksavgs_chunksize) :
  ManagedChain(0,0), in(in), smooth_end(0), 
  arc(arc), dec(0), bmg(bmg), pa_mgr(0),
  peaksavgs_chunksize(peaksavgs_chunksize)
{
  pthread_mutex_init(&decode_lock,0);
};

BlockReadManager::~BlockReadManager() {
  if (in != 0) {
    End(0);
  }

  pthread_mutex_destroy (&decode_lock);
};

void BlockReadManager::SetLoopType (codec looptype){
  // printf("DISK: Looptype: %s\n", bmg->GetApp()->getCFG()->GetCodecName(looptype));
  filetype = looptype;
};

// Start decoding from the given file
void BlockReadManager::Start(FILE *new_in) {
  if (dec != 0)
    return; // Already running

  if (in == 0 && new_in != 0) {
    // New input specified
    in = new_in;
  }

  if (in != 0) {
    // Start vorbis decoder
    switch (filetype) {
      case VORBIS:
        dec = new VorbisDecoder(bmg->GetApp());
        break;
      case WAV:
        dec = new SndFileDecoder(bmg->GetApp(),WAV);
        break;
      case FLAC:
        dec = new SndFileDecoder(bmg->GetApp(),FLAC);
        break;
      case AU:
        dec = new SndFileDecoder(bmg->GetApp(),AU);
        break;
      default:
        dec = new VorbisDecoder(bmg->GetApp());
        break;
    }
    b = (AudioBlock *) bmg->GetApp()->getPRE_AUDIOBLOCK()->RTNewWithWait();
    i = new AudioBlockIterator(b,DECODE_CHUNKSIZE,
                               bmg->GetApp()->getPRE_EXTRACHANNEL());
    
    // Compute audio peaks & averages for display
    if (peaksavgs_chunksize != 0) {
      AudioBlock *peaks = (AudioBlock *) b->RTNew(),
        *avgs = (AudioBlock *) b->RTNew();
      if (peaks == 0 || avgs == 0) {
        printf("BlockReadManager: ERROR: No free blocks for peaks/avgs\n");
        if (peaks != 0)
          peaks->RTDelete();
        if (avgs != 0)
          avgs->RTDelete();
        pa_mgr = 0;
      } else {
        b->AddExtendedData(new BED_PeaksAvgs(peaks,avgs,peaksavgs_chunksize));
        pa_mgr = bmg->PeakAvgOn(b,i,1);
      }
    }
    
    // Tell decoder to read from file
    if (dec->ReadFromFile(in,DECODE_CHUNKSIZE))
      // End in error, freeing vorbis 
      End(1);
  }
};

void BlockReadManager::End(char error) {
  // Can't end while decoding- lock mutex
  pthread_mutex_lock (&decode_lock);

  // Stop decoder
  if (dec != 0) {
    dec->Stop();
    delete dec;

    // Chop block to current position
    i->EndChain();

    // End peaks avgs compute now
    if (pa_mgr != 0) {
      // Catchup then end
      pa_mgr->Manage();
      pa_mgr->End();
      bmg->PeakAvgOff(b);
      pa_mgr = 0;
    }

    delete i;
    i = 0;
    dec = 0;
  }

  if (in != 0) {
    // printf("DISK: Close input.\n");
    // Vorbis decoder closes file through ov_clear!
    in = 0;
  }

  if (b != 0) {
    if (error) {
      // If error, delete chain and send zero to ReadComplete
      b->DeleteChain();
      b = 0;
    } else {
      // Finished read, handle looppoint smoothing

      // if (smooth_end)
        // New way
        // printf("blen: %d\n",b->GetTotalLen()); // b->Smooth(0,BlockWriteManager::ENCODE_CROSSOVER_LEN);

      if (!smooth_end) {
        // Old way
        b->Smooth(1,BlockReadManager::SMOOTH_FIX_LEN);
        // We will have to adjust the pulselength because we are changing the
        // length of the loop here

      }
    }
    
    // Callback
    if (arc != 0)
      arc->ReadComplete(b);

    b = 0;
  }

  pthread_mutex_unlock(&decode_lock);
};

int BlockReadManager::Manage() {
  if (in == 0) {
    // Not currently decoding
    if (arc != 0) {
      // We have a callback, get a chain to load
      arc->GetReadBlock(&in,&smooth_end);
      
      if (in != 0)
        // We got a chain to load, so begin
        Start();
    } else {
      // No callback, so we are done!
      return 1;
    }
  }

  if (in != 0) {
    // Make sure we have started on this blockchain
    if (dec == 0)
      Start();

    // Continue decoding
    pthread_mutex_lock (&decode_lock);

    // Now that we have the lock, make sure we are still active
    if (in != 0) {
      for (int pass = 0; in != 0 && pass < NUM_DECODE_PASSES; pass++) {
        // Make sure we have an extra block in our chain
        if (i->GetCurBlock()->next == 0) {
          AudioBlock *nw = (AudioBlock *) i->GetCurBlock()->RTNew();
          if (nw == 0) {
            // Pause decode until block is here
            printf("BlockReadManager: Waiting for block.\n");
            pthread_mutex_unlock (&decode_lock);
            return 0;
          }
          
          i->GetCurBlock()->Link(nw);
        }
        
        // Decode samples
        int len = dec->ReadSamples(i,DECODE_CHUNKSIZE);
        if (len == 0) {
          // EOF
          pthread_mutex_unlock (&decode_lock);
          End(0);
        } else if (len < 0) {
          // Stream error- just continue
          printf("BlockReadManager: Stream read error!\n");
        }
      }

    }
    
    pthread_mutex_unlock (&decode_lock);
  }

  return 0;
};

BlockWriteManager::BlockWriteManager(FILE *out, AutoWriteControl *awc, 
                                     BlockManager *bmg, AudioBlock *b, 
                                     AudioBlockIterator *i) : 
  ManagedChain(b,i), out(out), len(0), awc(awc), enc(0), bmg(bmg)
{
  pthread_mutex_init(&encode_lock,0);
};

BlockWriteManager::~BlockWriteManager() {
  if (b != 0) {
    End();
  }

  pthread_mutex_destroy (&encode_lock);
};

// Start encoding the given block chain to the given file
void BlockWriteManager::Start(FILE *new_out, AudioBlock *new_b, 
                              AudioBlockIterator *new_i,
                              nframes_t new_len) {
  if (enc != 0)
    return; // Already running

  if (b == 0 && new_b != 0) {
    // New chain specified
    // printf("start: new chain b: %p new_b: %p\n",b,new_b);
    b = new_b;
    i = new_i;
    out = new_out;
    len = new_len;
  }

  if (b != 0) {
    //printf("start: out: %p new_out: %p, b: %p new_b: %p\n",out,new_out,
    //   b,new_b);

    // Encode an extra ENCODE_CROSSOVER_LEN frames at the end
    // because OGG loops are not sample aligned from end-to-begin
    // len += ENCODE_CROSSOVER_LEN;
    // printf("slen: %d\n", len);
    pos = 0;

    // Start File encoder
    Fweelin *app = bmg->GetApp();
    switch (app->getCFG()->GetLoopOutFormat()) {
      case VORBIS:
        enc = new VorbisEncoder(app,b->IsStereo());
        break;
      case WAV: 
        enc = new SndFileEncoder(app,ENCODE_CHUNKSIZE,b->IsStereo(),WAV);
        break;
      case FLAC:
        enc = new SndFileEncoder(app,ENCODE_CHUNKSIZE,b->IsStereo(),FLAC);
        break;
      case AU:
        enc = new SndFileEncoder(app,ENCODE_CHUNKSIZE,b->IsStereo(),AU);
        break;
      default:
        enc = new VorbisEncoder(app,b->IsStereo());
        break;
    };

    ei = new AudioBlockIterator(b,ENCODE_CHUNKSIZE);
    
    // Set encoder to dump to file
    enc->SetupFileForWriting(out);
  }
};

void BlockWriteManager::End() {
  // Can't end while encoding- lock mutex
  pthread_mutex_lock (&encode_lock);

  // Stop encoder
  if (enc != 0) {
    enc->PrepareFileForClosing();
    delete enc;
    delete ei;
    enc = 0;
  }

  if (out != 0) {
    printf("DISK: Close output.\n");
    fclose(out);
    out = 0;
  }

  b = 0;
  i = 0;
  len = 0;
  
  pthread_mutex_unlock(&encode_lock);
};

int BlockWriteManager::Manage() {
  if (b == 0) {
    // Not currently encoding
    if (awc != 0) {
      // We have a callback, get a chain to save
      awc->GetWriteBlock(&out,&b,&i,&len);
      
      if (b != 0)
        // We got a chain to save, so begin
        Start();
    } else {
      // No callback, so we are done!
      return 1;
    }
  }

  if (b != 0) {
    // Make sure we have started on this blockchain
    if (enc == 0)
      Start();

    // Continue encoding
    pthread_mutex_lock (&encode_lock);

    // Now that we have the lock, make sure we are still active
    if (b != 0) {
      nframes_t remaining = len-pos,
        num = MIN(ENCODE_CHUNKSIZE,remaining);
      
      sample_t *ibuf[2];
      if (enc->IsStereo()) {
        // Stereo
        ei->GetFragment(&ibuf[0],&ibuf[1]);
        pos += enc->WriteSamplesToDisk(ibuf,0,num);
      } else {
        // Mono
        ei->GetFragment(&ibuf[0],0);
        pos += enc->WriteSamplesToDisk(ibuf,0,num);
      }

      if (remaining <= ENCODE_CHUNKSIZE) {
        // Finished encoding
        pthread_mutex_unlock (&encode_lock);
        End();
      }
      else
        ei->NextFragment();
    }

    pthread_mutex_unlock (&encode_lock);
  }

  return 0;
};

BED_PeaksAvgs::~BED_PeaksAvgs() {
  peaks->DeleteChain();
  avgs->DeleteChain();
};

int BED_MarkerPoints::CountMarkers() {
  TimeMarker *cur = markers;
  int markcnt = 0;
  while (cur != 0) {
    markcnt++;
    cur = cur->next;
  };
  
  return markcnt;
};

BED_MarkerPoints::~BED_MarkerPoints() {
  TimeMarker *cur = markers;
  while (cur != 0) {
    TimeMarker *tmp = cur->next;
    cur->RTDelete();
    cur = tmp;
  } 
};

// Returns the nth marker before (in time) the current offset passed
// This method correctly handles a reverse wrap case when the
// marker NBeforeCur is farther ahead in the attached block than the
// current offset
TimeMarker *BED_MarkerPoints::GetMarkerNBeforeCur(int n, nframes_t curofs) {
  TimeMarker *cur = markers;
  signed int markcnt = 0;
  int totalmarks = CountMarkers();
  
  if (totalmarks == 0) 
    return 0; // No solution because there are no markers!
  else {
    while (cur != 0 && cur->markofs < curofs) {
      markcnt++;
      cur = cur->next;
    }
    
    // Markcnt now indexes the next marker after curofs
    // Go N before!
    markcnt -= n;
    while (markcnt < 0) {
      // Reverse wrap case
      markcnt += totalmarks;
    }
    
    // Now get marker indexed by markcnt
    cur = markers;
    int markcnt2 = 0;
    while (cur != 0 && markcnt2 != markcnt) {
      markcnt2++;
      cur = cur->next;
    }
    
    // & return it
    return cur;
  }
};

// Create a new extra channel
BED_ExtraChannel::BED_ExtraChannel(nframes_t len) {
  if (len > 0) {
    //printf("allocating origbuf..\n");
    origbuf = buf = new sample_t[len];
    //printf("done: origbuf %p: buf %p!..\n",origbuf,buf);
  }
  else
    origbuf = buf = 0;
};

BED_ExtraChannel::~BED_ExtraChannel() {
  if (origbuf != 0) {
    //printf("deleting origbuf %p: buf %p!..\n",origbuf,buf);
    delete[] origbuf;
    //printf("done!\n");
  }
};

// Create a new audioblock as the beginning of a block list
AudioBlock::AudioBlock(nframes_t len) : len(len), next(0),
                                        first(this), xt(0)
{
  if (len > 0)
    origbuf = buf = new sample_t[len];
}

// Create a new audioblock and link up the specified block to it
AudioBlock::AudioBlock(AudioBlock *prev, nframes_t len) : 
  len(len), next(0), first(prev->first), xt(0) {
  prev->next = this;
  if (len > 0)
    origbuf = buf = new sample_t[len];
}

AudioBlock::~AudioBlock() {
  //printf("~AudioBlock len: %d dsz: %d\n",len,sizeof(*buf));
  if (origbuf != 0)
    delete[] origbuf;
}

// Clears this block chain- not changing length
void AudioBlock::Zero() {
  AudioBlock *cur = first;
  while (cur != 0) {
    memset(cur->buf,0,sizeof(sample_t)*cur->len);
    cur = cur->next;
  }
};

// Link us up to the specified block
void AudioBlock::Link(AudioBlock *to) {
  to->first = first;
  to->next = 0;
  next = to;
}

// RT safe
void AudioBlock::ChopChain() {
  // Chop off the chain at this block
  // Freeing unused blocks
  AudioBlock *cur = next;
  next = 0;
  while (cur != 0) {
    AudioBlock *tmp = cur->next;
    if (cur->xt != 0)
      printf("BLOCK: WARNING: XT data not freed in ChopChain! "
             "Possible leak.\n");

    cur->RTDelete();
    cur = tmp;
  }
}

// Smooth an audio block-
// either smooth the beginning into the end (smoothtype == 1)
// or smooth the end into the beginning     (smoothtype == 0)
void AudioBlock::Smooth(char smoothtype, nframes_t smoothlen) {
  // Smooth the end of this chain into the beginning for looping
  AudioBlock *smoothblk;
  nframes_t smoothofs,
    smoothcnt,
    totallen = GetTotalLen();
  if (totallen >= smoothlen)
    smoothcnt = totallen - smoothlen;
  else
    return; // Very short chain, can't smooth it

  // Get block & offset for start of smooth
  SetPtrsFromAbsOffset(&smoothblk, &smoothofs, smoothcnt);
  if (smoothblk == 0) {
    // Shouldn't happen
    printf("AudioBlock: ERROR: Block position mismatch in Smooth.\n");
    exit(1);
  }

  AudioBlock *startblk = first;
  nframes_t startofs = 0;

  // Second channel?
  BED_ExtraChannel *rightstartblk = 
    (BED_ExtraChannel *) startblk->GetExtendedData(T_BED_ExtraChannel),
    *rightsmoothblk = (BED_ExtraChannel *)
    smoothblk->GetExtendedData(T_BED_ExtraChannel);
  char stereo = 0;
  if (rightstartblk != 0 && rightsmoothblk != 0)
    stereo = 1; 
    
  float mix = 0.0,
    dmix = 1./(float)smoothlen;
  for (nframes_t i = 0; i < smoothlen; i++, 
         mix += dmix) {
    if (smoothtype) {
      // Write beginning into end
      smoothblk->buf[smoothofs] = mix*startblk->buf[startofs] + 
        (1.0-mix)*smoothblk->buf[smoothofs];
      if (stereo)
        rightsmoothblk->buf[smoothofs] = mix*rightstartblk->buf[startofs] + 
          (1.0-mix)*rightsmoothblk->buf[smoothofs];
    } else {
      // Write end into beginning
      startblk->buf[startofs] = mix*startblk->buf[startofs] + 
        (1.0-mix)*smoothblk->buf[smoothofs];
      if (stereo)
        rightstartblk->buf[startofs] = mix*rightstartblk->buf[startofs] + 
          (1.0-mix)*rightsmoothblk->buf[smoothofs];
    }

    smoothofs++;
    startofs++;
    if (smoothofs >= smoothblk->len) {
      smoothblk = smoothblk->next;
      smoothofs = 0;
      if (smoothblk == 0) {
        if (i+1 < smoothlen) {
          // Shouldn't happen
          printf("AudioBlock: ERROR: (smoothblk) Block size mismatch in "
                 "Smooth: i: %d\n", i);
          exit(1);
        }
        
        rightsmoothblk = 0;
      } else {
        rightsmoothblk = (BED_ExtraChannel *)
          smoothblk->GetExtendedData(T_BED_ExtraChannel);
        if (stereo && rightsmoothblk == 0) {
          // Shouldn't happen
          printf("AudioBlock: ERROR: (smoothblk) Right channel "
                 "disappeared!\n");
          exit(1);
        }
      }
    }
    if (startofs >= startblk->len) {
      startblk = startblk->next;
      startofs = 0;
      if (startblk == 0) {
        if (i+1 < smoothlen) {
          // Shouldn't happen
          printf("AudioBlock: ERROR: (startblk) Block size mismatch in "
                 "Smooth.\n");
          exit(1);
        }
        
        rightstartblk = 0;
      } else {
        rightstartblk = (BED_ExtraChannel *)
          startblk->GetExtendedData(T_BED_ExtraChannel);
        if (stereo && rightstartblk == 0) {
          // Shouldn't happen
          printf("AudioBlock: ERROR: (startblk) Right channel disappeared!\n");
          exit(1);
        }
      }
    }
  }

  if (smoothtype) {
    // Adjust first buf to skip samples that were smoothed into end
    if (first->len < smoothlen) {
      printf("AudioBlock: WARNING: First block is very short, "
             "no adjust in Smooth.\n");
    } else {
      first->buf += smoothlen;
      first->len -= smoothlen;
      
      BED_ExtraChannel *rightfirst = 
        (BED_ExtraChannel *) first->GetExtendedData(T_BED_ExtraChannel);
      if (rightfirst != 0) 
        rightfirst->buf += smoothlen;
    }
  } else {
    // Shorten the chain to skip end which is now smoothed into beginning
    HackTotalLengthBy(smoothlen);
  }

  //printf("endsmooth: startofs: %d smoothofs: %d len: %d\n",
  //     startofs, smoothofs, totallen);
}

// not RT safe
void AudioBlock::DeleteChain() {
  AudioBlock *cur = first;
  while (cur != 0) {
    // First erase any extended data
    BlockExtendedData *curxt = cur->xt;
    while (curxt != 0) {
      BlockExtendedData *tmpxt = curxt->next;
      if (curxt->GetType() == T_BED_ExtraChannel)
        ((BED_ExtraChannel *)curxt)->RTDelete();
      else
        delete curxt;
      curxt = tmpxt;
    }

    // Then the block itself!
    AudioBlock *tmp = cur->next;
    cur->RTDelete();
    cur = tmp;
  }
}

// Returns the total length of this audio chain
nframes_t AudioBlock::GetTotalLen() {
  nframes_t tally = 0;
  AudioBlock *cur = first;
  while (cur != 0) {
    tally += cur->len;
    cur = cur->next;
  }
  
  return tally;
};

// Gets extended data for this block
BlockExtendedData *AudioBlock::GetExtendedData(BlockExtendedDataType x) { 
  BlockExtendedData *cur = xt;
  while (cur!=0 && cur->GetType()!=x)
    cur = cur->next;
  return cur;
};

// Add this extended data to the list of extended data for this block
void AudioBlock::AddExtendedData(BlockExtendedData *nw) {
  nw->next = xt;
  xt = nw;
};

// Finds the audioblock and offset into that block that correspond
// to the provided absolute offset into this chain 
void AudioBlock::SetPtrsFromAbsOffset(AudioBlock **ptr, nframes_t *blkofs, 
                                      nframes_t absofs) {
  AudioBlock *cur = first,
    *prev = 0;
  nframes_t curofs = 0,
    prevofs = 0;
  while (cur != 0 && curofs <= absofs) {
    prevofs = curofs;
    curofs += cur->len;
    prev = cur;
    cur = cur->next;
  }  
  
  if (curofs < absofs || prev == 0) {
    // We're at the end and we still haven't traversed far enough!
    // Return err
    *ptr = 0;
    return;
  }
  
  *ptr = prev;
  *blkofs = absofs-prevofs;
};

// Generates a subchain of AudioBlocks by copying the samples between offsets
// from & to.. offsets expressed absolutely with reference to
// beginning of this chain!
// Samples are copied up to but not including toofs
// If toofs < fromofs, generates a subblock that includes the end of
// the block and the beginning up to toofs (wrap case)
// Returns the last block in the new subchain
// Flag sets whether to copy stereo channel if there is one
// Realtime safe?
AudioBlock *AudioBlock::GenerateSubChain(nframes_t fromofs, nframes_t toofs,
                                         char copystereo) {
  if (toofs == fromofs)
    return 0;
  // Handle wrap case
  if (toofs < fromofs)
    toofs += GetTotalLen();

  // Stereo?
  BED_ExtraChannel *rightfirst = (BED_ExtraChannel *)
    first->GetExtendedData(T_BED_ExtraChannel);
  char stereo = (copystereo && rightfirst != 0);
    
  // Get enough new blocks to fit the subblock size
  nframes_t sublen = toofs - fromofs,
    suballoc = 0;
  AudioBlock *subfirst = 0,
    *subcur = 0;
  while (suballoc < sublen) {
    if (subfirst == 0) {
      subcur = subfirst = (AudioBlock *)RTNew();
      if (subfirst == 0) {
        printf("Err: GenerateSubChain- No new blocks available\n");
        return 0;
      }
    }
    else {
      AudioBlock *tmp = (AudioBlock *)RTNew();
      if (tmp == 0) {
        printf("Err: GenerateSubChain- No new blocks available\n");
        return 0;
      }
      subcur->Link(tmp);
      subcur = subcur->next;
    }

    if (stereo) {
      BED_ExtraChannel *rightsub = (BED_ExtraChannel*)rightfirst->RTNew(); 
      if (rightsub == 0) {
        printf("Err: GenerateSubChain- No new right blocks available\n");
        return 0;
      }
      subcur->AddExtendedData(rightsub);
    }

    suballoc += subcur->len;
  }
    
  // Find starting pos in chain
  AudioBlock *cur;
  nframes_t curofs = 0;
  SetPtrsFromAbsOffset(&cur,&curofs,fromofs);

  subcur = subfirst;
  nframes_t subofs = 0,
    remaining = sublen; 

  // Extra channel
  BED_ExtraChannel *subright = 
    (stereo ? (BED_ExtraChannel *) subcur->GetExtendedData(T_BED_ExtraChannel)
     : 0),
    *right = 
    (stereo ? (BED_ExtraChannel *) cur->GetExtendedData(T_BED_ExtraChannel)
     : 0);

  do {
    nframes_t n = MIN(cur->len-curofs,remaining);
    n = MIN(n,subcur->len-subofs);

    memcpy(&subcur->buf[subofs],&cur->buf[curofs],n*sizeof(sample_t));
    if (stereo)
      memcpy(&subright->buf[subofs],&right->buf[curofs],n*sizeof(sample_t));
      
    subofs += n;
    curofs += n;
    remaining -= n;
    if (curofs >= cur->len) {
      cur = cur->next;
      curofs = 0;
      if (cur == 0)
        // Past the end of the block chain-- wrap around to first
        cur = first;

      if (stereo)
        right = (BED_ExtraChannel *) cur->GetExtendedData(T_BED_ExtraChannel);
    }

    if (subofs >= subcur->len) {
      subcur = subcur->next;
      subofs = 0;
      if (subcur == 0 && remaining) {
        // This should never happen, because we make the destination
        // chain long enough to hold the subblock
        printf("Err: GenerateSubChain destination block size mismatch\n");
        exit(1);
      }

      if (stereo)
        subright = (BED_ExtraChannel *) 
          subcur->GetExtendedData(T_BED_ExtraChannel);
    }
  } while (remaining);

  // Truncate the last presized subblock to the right length
  subcur->len = subofs;

  // And return the last block in the chain
  return subcur;
};

// *** To be tested! ***
//
// Removes the last 'hacklen' samples from this block chain
// Returns nonzero on error!
// Realtime safe?
int AudioBlock::HackTotalLengthBy(nframes_t hacklen) {
  // Compute the position of hack
  nframes_t chainlen = GetTotalLen();
  //printf("Hack (b): %ld\n",chainlen);
  AudioBlock *hackblk;
  nframes_t hackofs = 0;
  if (chainlen <= hacklen)
    return 1;
  SetPtrsFromAbsOffset(&hackblk,&hackofs,chainlen-hacklen);
  
  // Now hackblk[hackofs] should be the first sample to erase at the
  // end of the chain
  hackblk->len = hackofs; // Truncate length of block
  // Note: Block memory isnt resized, so some extra memory lingers until
  // the block is erased
  
  AudioBlock *tmp = hackblk->next;
  hackblk->next = 0; // End of chain at hack position
  
  // Erase remaining blocks in chain
  hackblk = tmp; 
  while (hackblk != 0) {
    tmp = hackblk->next;

    // First erase any extended data
    BlockExtendedData *hackxt = hackblk->xt;
    while (hackxt != 0) {
      BlockExtendedData *tmpxt = hackxt->next;
      delete hackxt;
      hackxt = tmpxt;
    }

    // Then the block itself
    hackblk->RTDelete();

    hackblk = tmp;
  }

  //printf("Hack (e): %ld\n",GetTotalLen());
  return 0;
};

// Inserts the new blockchain at the beginning of this block chain
// Returns a pointer to the new first block
AudioBlock *AudioBlock::InsertFirst(AudioBlock *nw) {
  // Move to the end of the passed chain
  while (nw->next != 0)
    nw = nw->next;

  // Link new block to the beginning of our chain
  nw->next = first;
  
  // Link all first pointers to first block in new chain
  AudioBlock *cur = first;
  while (cur != 0) {
    cur->first = nw->first;
    cur = cur->next;
  }
  
  return nw->first;
};

AudioBlockIterator::AudioBlockIterator(AudioBlock *firstblock,  
                                       nframes_t fragmentsize,
                                       PreallocatedType *pre_extrachannel) :
  pre_extrachannel(pre_extrachannel), 

  currightblock(0), nextrightblock(0), currightblock_w(0), nextrightblock_w(0),
  curblock(firstblock), nextblock(0), curblock_w(0), nextblock_w(0),
  curblkofs(0), nextblkofs(0), curcnt(0), nextcnt(0),
  curblkofs_w(0), nextblkofs_w(0), curcnt_w(0), nextcnt_w(0),

  fragmentsize(fragmentsize), stopped(0) {
  fragment[0] = new sample_t[fragmentsize];
  fragment[1] = new sample_t[fragmentsize];
}

AudioBlockIterator::~AudioBlockIterator() { 
  delete[] fragment[0];
  delete[] fragment[1];
};

// Stores in cnt the absolute count corresponding to the given
// block and offset
void AudioBlockIterator::GenCnt(AudioBlock *blk, nframes_t blkofs, 
                                nframes_t *cnt) {
  AudioBlock *cur = curblock->first;
  nframes_t curofs = 0;
  while (cur != 0 && cur != blk) {
    // Add the length of this whole block
    curofs += cur->len;
    cur = cur->next;
  }
  
  if (cur == 0) {
    // Given block not found!
    *cnt = 0;
  }
  else {
    // We are now on the given block- add only current block offset
    curofs += blkofs;
    *cnt = curofs;
  }
}

void AudioBlockIterator::Jump(nframes_t ofs) {
  // Quantize the specified offset to within the limits of the blockchain
  ofs = ofs % curblock->GetTotalLen();

  nframes_t cb_ofs = 0;
  curblock->SetPtrsFromAbsOffset(&curblock,&cb_ofs,ofs);
  curblkofs = cb_ofs;
  if (curblock == 0) {
    printf("Err: AudioBlockIterator::Jump- Pointer/size mismatch\n");
    exit(1);
  }
  currightblock = 0;
  nextrightblock = 0;
  nextblock = 0;
  nextblkofs = 0;
  curcnt = ofs;
  nextcnt = 0;
}

void AudioBlockIterator::GenConstants() {
  // Need to recompute curcnt to account for extra blocks?
  nframes_t tmp;
  GenCnt(curblock,(nframes_t) curblkofs,&tmp);
  curcnt = tmp;
  GenCnt(nextblock,(nframes_t) nextblkofs,&tmp);
  nextcnt = tmp;
}

// Moves iterator to start position
void AudioBlockIterator::Zero() {
  currightblock = 0;
  nextrightblock = 0;
  curblock = curblock->first;     
  curblkofs = 0; 
  curcnt = 0;
  nextblock = 0;
}

// Advances to the next fragment
void AudioBlockIterator::NextFragment() {
  // Only advance if not stopped
  if (!stopped) {
    // We must first get a fragment before advancing
    if (nextblock == 0)
      GetFragment(0,0);
 
    currightblock = nextrightblock;
    nextrightblock = 0;
    curblock = nextblock;
    curblkofs = nextblkofs;
    curcnt = nextcnt;
    nextblock = 0;

    // Also advance write block (if rate scaling)
    char ratescale = 0;    
    if (ratescale) {
      currightblock_w = nextrightblock_w;
      nextrightblock_w = 0;
      curblock_w = nextblock_w;
      curblkofs_w = nextblkofs_w;
      curcnt_w = nextcnt_w;
      nextblock_w = 0;
    }    
  }
}

// PutFragment stores the specified fragment into this AudioBlock
// returns nonzero if the end of the block is reached, and we wrap to next
int AudioBlockIterator::PutFragment (sample_t *frag_l, sample_t *frag_r,
                                     nframes_t size_override, 
                                     char wait_alloc) {
  nframes_t fragofs = 0;
  nframes_t n = (size_override == 0 ? fragmentsize : size_override);
  int wrap = 0;

  char ratescale = 0;
  
  // Keep local track of pointers
  // Since we have to be threadsafe
  BED_ExtraChannel *lclrightblock;
  AudioBlock *lclblock;
  double lclblkofs, lclcnt;

  if (ratescale) {
    // Use write block
    lclrightblock = currightblock_w;
    lclblock = curblock_w;
    lclblkofs = curblkofs_w;
    lclcnt = curcnt_w;
  } else {
    // Write to read block (no scaling on read, 
    // so same buffers can be used)
    lclrightblock = currightblock;
    lclblock = curblock;
    lclblkofs = curblkofs;
    lclcnt = curcnt;
  }

  nframes_t nextbit;
  do {
    nframes_t lclblkofs_d = (nframes_t) lclblkofs;
    nextbit = MIN((nframes_t) n, lclblock->len - lclblkofs_d);

    // Copy into block

    // Left
    memcpy(&lclblock->buf[lclblkofs_d],
           &frag_l[fragofs],
           sizeof(sample_t)*nextbit);

    // If right channel is given and we don't know the right block, find it
    if (frag_r != 0) {
      if (lclrightblock == 0) {
        lclrightblock = (BED_ExtraChannel *) 
          lclblock->GetExtendedData(T_BED_ExtraChannel);
        if (lclrightblock == 0) {
          // No right channel exists but we are being told to put data there--
          // so create a right channel!
          if (pre_extrachannel == 0) {
            printf("BLOCK: ERROR: Need to make right channel buffer but no "
                   "preallocator was passed!\n");
            return 0;
          } else {
            do {
              lclrightblock = (BED_ExtraChannel *) pre_extrachannel->RTNew();
              if (lclrightblock == 0 && wait_alloc) {
                printf("BLOCK: Waiting for BED_ExtraChannel.\n");
                usleep(10000); // Wait then try again
              }
            } while (lclrightblock == 0 && wait_alloc);
            if (lclrightblock != 0)
              lclblock->AddExtendedData(lclrightblock);
            else {
              printf("BLOCK: ERROR: RTNew() failed for BED_ExtraChannel.\n");
              return 0;
            }
          }
        }
      }
      
      // Right
      memcpy(&lclrightblock->buf[lclblkofs_d],
             &frag_r[fragofs],
             sizeof(sample_t)*nextbit);
    }
    
    fragofs += nextbit;
    lclblkofs += nextbit;
    lclcnt += nextbit;
    n -= nextbit;
    
    if (lclblkofs >= lclblock->len) {
      // If we get here, it means this block has been fully dumped
      // so we need the next block
      
      wrap = 1;
      if (lclblock->next == 0) {
        // END OF AUDIOBLOCK LIST, LOOP TO BEGINNING
        lclblock = lclblock->first;
        lclblkofs = 0;
        lclcnt = lclblkofs;
        lclrightblock = 0;
      }
      else {
        lclblock = lclblock->next;
        lclblkofs = 0; 
        lclrightblock = 0;
      }
    }
  } while (n);
  
  if (ratescale) {
    nextblock_w = lclblock;
    nextrightblock_w = lclrightblock;
    nextblkofs_w = lclblkofs;
    nextcnt_w = lclcnt;
  } else {
    nextblock = lclblock;
    nextrightblock = lclrightblock;
    nextblkofs = lclblkofs;
    nextcnt = lclcnt;
  } 

  return wrap;
}

// Returns the current fragment of audio
// nextblock and nextblkofs become the new block pointer and offset
// for the next fragment
void AudioBlockIterator::GetFragment(sample_t **frag_l, sample_t **frag_r) {
  // Keep local track of pointers
  // Since we have to be threadsafe
  BED_ExtraChannel *lclrightblock = currightblock;
  AudioBlock *lclblock = curblock;
  double lclblkofs = curblkofs,
    lclcnt = curcnt;

  char ratescale = 0;
  float rate = 2.0;

  if (ratescale) {
    double n = fragmentsize*rate;
      
    if (lclblkofs + n + 1 >= lclblock->len) {
      // Wrap happens here- check bounds in loop

      for (nframes_t cnt = 0; cnt < fragmentsize; cnt++) {
        // Linear interpolation rate scale
        nframes_t decofs = (nframes_t) lclblkofs;
        float fracofs = lclblkofs - decofs;

        if (frag_r != 0) {
          if (lclrightblock == 0) {
            lclrightblock = (BED_ExtraChannel *) 
              lclblock->GetExtendedData(T_BED_ExtraChannel);
            if (lclrightblock == 0) {
              // No right channel exists but we are being told to get data--
              printf("BLOCK: ERROR: Iterator asked for right channel but none "
                     "exists!\n");
              return;
            }
          }
        }
       
        sample_t s1 = lclblock->buf[decofs],
          s2,
          s1r,
          s2r;
        if (frag_r != 0)
          s1r = lclrightblock->buf[decofs];

        nframes_t decofs_p1 = decofs+1;
        if (decofs_p1 < lclblock->len) {
          s2 = lclblock->buf[decofs_p1];
          if (frag_r != 0)
            s2r = lclrightblock->buf[decofs_p1];
        } else {
          AudioBlock *next_b;
          if (lclblock->next != 0)
            next_b = lclblock->next;
          else
            next_b = lclblock->first;

          s2 = next_b->buf[0];
          if (frag_r != 0) {
            BED_ExtraChannel *tmp_r = (BED_ExtraChannel *) 
              next_b->GetExtendedData(T_BED_ExtraChannel);
            if (tmp_r != 0)
              s2r = tmp_r->buf[0];
            else
              s2r = s1r;
          }
        }

        float fracofs_om = 1.0-fracofs;
        fragment[0][cnt] = fracofs*s2 + fracofs_om*s1;
        if (frag_r != 0)
          fragment[1][cnt] = fracofs*s2r + fracofs_om*s1r;

        lclblkofs += rate;
        lclcnt += rate;
        if (lclblkofs >= lclblock->len) {
          // If we get here, it means this block is at an end
          // so we need the next block  
          lclblkofs -= lclblock->len;
          
          if (lclblock->next == 0) {
            // END OF AUDIOBLOCK LIST, LOOP TO BEGINNING
            lclcnt = lclblkofs;
            lclblock = lclblock->first;
            lclrightblock = 0;
          } else {
            lclblock = lclblock->next;
            lclrightblock = 0;
          }
        }
      }
    } else {
      // No wrap- don't check bounds

      for (nframes_t cnt = 0; cnt < fragmentsize; cnt++, lclblkofs += rate,
           lclcnt += rate) {
        // Linear interpolation rate scale
        nframes_t decofs = (nframes_t) lclblkofs;
        float fracofs = lclblkofs - decofs;

        if (frag_r != 0) {
          if (lclrightblock == 0) {
            lclrightblock = (BED_ExtraChannel *) 
              lclblock->GetExtendedData(T_BED_ExtraChannel);
            if (lclrightblock == 0) {
              // No right channel exists but we are being told to get data--
              printf("BLOCK: ERROR: Iterator asked for right channel but none "
                     "exists!\n");
              return;
            }
          }
        }
        
        nframes_t decofs_p1 = decofs+1;
        sample_t s1 = lclblock->buf[decofs],
          s2 = lclblock->buf[decofs_p1];

        float fracofs_om = 1.0-fracofs;
        fragment[0][cnt] = fracofs*s2 + fracofs_om*s1;

        if (frag_r != 0) {
          sample_t s1r = lclrightblock->buf[decofs],
            s2r = lclrightblock->buf[decofs_p1];

          fragment[1][cnt] = fracofs*s2r + fracofs_om*s1r;
        }
      }
    }
  } else {
    // No rate scale
    nframes_t fragofs = 0;
    nframes_t n = fragmentsize;
        
    nframes_t nextbit;
    do {
      nframes_t lclblkofs_d = (nframes_t) lclblkofs;
      nextbit = MIN((nframes_t) n, lclblock->len - lclblkofs_d);
      
      if (nextbit) {
        // Left
        memcpy(&fragment[0][fragofs],
               &lclblock->buf[lclblkofs_d],
               sizeof(sample_t)*nextbit);
        
        // If right channel is given and we don't know the right block, find it
        if (frag_r != 0) {
          if (lclrightblock == 0) {
            lclrightblock = (BED_ExtraChannel *) 
              lclblock->GetExtendedData(T_BED_ExtraChannel);
            if (lclrightblock == 0) {
              // No right channel exists but we are being told to get data--
              printf("BLOCK: ERROR: Iterator asked for right channel but none "
                     "exists!\n");
              return;
            }
          }
          
          // Right
          memcpy(&fragment[1][fragofs],
                 &lclrightblock->buf[lclblkofs_d],
                 sizeof(sample_t)*nextbit);
        } else
          // Make -sure- we don't jump blocks and keep old rightblock
          lclrightblock = 0;
        
        fragofs += nextbit;
        lclblkofs += nextbit;
        lclcnt += nextbit;
        n -= nextbit;
      }
      
      if (lclblkofs >= lclblock->len) {
        // If we get here, it means this block is at an end
        // so we need the next block  
        if (lclblock->next == 0) {
          // END OF AUDIOBLOCK LIST, LOOP TO BEGINNING
          lclblock = lclblock->first;
          lclblkofs = 0;
          lclcnt = lclblkofs;
          lclrightblock = 0;
        } else {
          lclblock = lclblock->next;
          lclblkofs = 0; // Beginning of next block
          lclrightblock = 0;
        }
      }
    } while (n);
  }
    
  nextblock = lclblock;
  nextrightblock = lclrightblock;
  nextblkofs = lclblkofs;
  nextcnt = lclcnt;
  
  // Return fragment buffers
  if (frag_l != 0)
    *frag_l = fragment[0];
  if (frag_r != 0) 
    *frag_r = fragment[1];
};

// RT safe
void AudioBlockIterator::EndChain() {
  // Mark iterator stopped
  stopped = 1;
  
  //printf("Crop: %ld to %ld\n", curblock->len, curblkofs);
  if (curblkofs < (nframes_t) fragmentsize) {
    //printf("WARNING: REALLY SHORT BLOCK: %d\n",curblkofs);
    curblock->len = fragmentsize;
  }
  else
    curblock->len = (nframes_t) curblkofs;    
  
  // Stop the chain at this place
  curblock->ChopChain();
}

int GrowChainManager::Manage() {
  // Manage chain growth
  if (!i->IsStopped()) 
    if (i->GetCurBlock()->next == 0) {
      // Iterator is running and no more blocks at the end
      // of this chain! Get another and link it up.
      AudioBlock *nw = (AudioBlock *) i->GetCurBlock()->RTNew();
      if (nw == 0) {
        printf("GrowChainManager: ERROR: No free blocks to grow chain\n");
        return 0;
      }
      i->GetCurBlock()->Link(nw);

      // Second channel allocating is done as needed by PutFragment
      // in the iterator
    }

  return 0;
};

void PeaksAvgsManager::Setup() {
  // Setup iterators
  pa = (BED_PeaksAvgs *)(b->GetExtendedData(T_BED_PeaksAvgs));
  // To do: Optimize this to bypass iterator- 
  // it would be faster since we are moving one sample at a time!
  peaksi = new AudioBlockIterator(pa->peaks,1);
  avgsi = new AudioBlockIterator(pa->avgs,1);
  mi = new AudioBlockIterator(b,1);
  stereo = b->IsStereo();

  if (grow) {
    // Grow peaks & avgs blocks
    bmg->GrowChainOn(pa->peaks,peaksi);
    bmg->GrowChainOn(pa->avgs,avgsi);
  }
};

PeaksAvgsManager::~PeaksAvgsManager() {
  if (bmg != 0) {
    End();

    delete peaksi;
    delete avgsi;
    delete mi;
  }
};

void PeaksAvgsManager::End() {
  if (!ended) {
    ended = 1;

    if (grow) {
      // End chain at current pos!
      peaksi->EndChain();
      avgsi->EndChain();

      bmg->GrowChainOff(pa->peaks);
      bmg->GrowChainOff(pa->avgs);
    }
  }
}

int PeaksAvgsManager::Manage() {
  // Stop if ended..
  if (ended)
    return 1;

  // Compute running peaks and averages
  
  char wrap;
  do {
    wrap = 0;
    
    // Get current position in iterator 
    nframes_t curcnt = i->GetTotalLength2Cur();
    
    if (curcnt < lastcnt) {
      // We have a wrap condition!- loop
      // printf("Wrap! Now at: %ld\n",curcnt);
      curcnt = b->GetTotalLen();
      wrap = 1;
    }
    
    // Update peaks & averages to current position    
    while (lastcnt < curcnt) {
      sample_t *sptr[2];
      sample_t s_l, s_r;
      if (stereo) {
        mi->GetFragment(&sptr[0],&sptr[1]);     
        s_l = *sptr[0];
        s_r = *sptr[1];
      } else {
        mi->GetFragment(&sptr[0],0);    
        s_l = *sptr[0];
        s_r = 0;
      }

      // If chunkcnt is -1, we have temporarily stopped until a wrap
      if (go) {
        if (s_l > runmax)
          runmax = s_l;
        if (s_l < runmin)
          runmin = s_l;

        if (stereo) {
          if (s_r > runmax)
            runmax = s_r;
          if (s_r < runmin)
            runmin = s_r;

          runtally += (fabs(s_l)+fabs(s_r))/2;
        }
        else
          runtally += fabs(s_l);
          
        chunkcnt++;
        
        if (chunkcnt >= pa->chunksize) {
          // One chunk done
          sample_t peak = runmax-runmin,
            avg = (sample_t) (runtally/pa->chunksize);
          
          if (peaksi->PutFragment(&peak,0) || 
              avgsi->PutFragment(&avg,0)) {
            // Peaks should not be wrapping before main buf, stop!
            //
            // Note that this does happen!!
            go = 0;
          }
          else {
            peaksi->NextFragment();
            avgsi->NextFragment();
            chunkcnt = 0;
          }
          
          runtally = 0;
          runmax = 0;
          runmin = 0;
        }
      }
      
      mi->NextFragment();
      lastcnt++;
    }
    
    if (wrap) {
      /* printf("Main wrap!: MI: %ld PI: %ld AI: %ld\n",
         mi->GetTotalLength2Cur(),
         peaksi->GetTotalLength2Cur(),
         avgsi->GetTotalLength2Cur()); */
      mi->Zero();
      peaksi->Zero();
      avgsi->Zero();
      lastcnt = 0; // Wrap to beginning, and do the samples there
      go = 1;
      
      chunkcnt = 0;
      runtally = 0;
      runmax = 0;
      runmin = 0;
    }
  } while (wrap);

  return 0;
};

void StripeBlockManager::Setup() {
  // Does this block have BED_MarkerPoints?
  mp = (BED_MarkerPoints *)(b->GetExtendedData(T_BED_MarkerPoints));
  if (mp == 0) {
    // No marker block  
    b->AddExtendedData(mp = new BED_MarkerPoints());
  }
};

int StripeBlockManager::Manage() {
  // This is called whenever a time marker should be striped
  // to the block

  char wrap = 0;
  do {
    if (wrap)
      wrap = 2; // Special case if we just wrapped and are on a 2nd pass
    else
      wrap = 0;

    // Get current iterated offset into block
    nframes_t curcnt = i->GetTotalLength2Cur();

    if (curcnt < lastcnt) {
      // We have a wrap condition!- loop
      // printf("Wrap! Now at: %ld\n",curcnt);
      curcnt = b->GetTotalLen();
      wrap = 1;
    }

    // Scan through time markers for a good place to insert a new one 
    TimeMarker *cur = mp->markers,
      *prev = 0;
    if (wrap == 2) {
      // On 2nd pass, delete markers at 0.
      wrap = 0;
      while (cur != 0 && cur->markofs < lastcnt) {
        prev = cur;
        cur = cur->next;
      }
    } else {
      while (cur != 0 && cur->markofs <= lastcnt) {
        prev = cur;
        cur = cur->next;
      }
    }

    // Delete any markers between last position & current
    while (cur != 0 && cur->markofs <= curcnt) {
      TimeMarker *tmp = cur->next;
      cur->RTDelete();
      
      cur = tmp;
      if (prev != 0)
        prev->next = cur;
      else
        mp->markers = cur;
    } 

    if (!wrap) {
      // Now insert a new marker in this position
      // Use realtime new method- no problem
      TimeMarker *nw = (TimeMarker *) pre_tm->RTNew();
      if (nw == 0) {
        printf("StripeBlockManager: ERROR: No free TimeMarkers\n");
        return 0;
      }
      nw->markofs = curcnt;
      nw->next = cur;
      if (prev != 0)
        prev->next = nw;
      else
        mp->markers = nw;
      
      // Update counters
      lastcnt = curcnt;
    }
    else
      lastcnt = 0; // Start again from beginning to curcnt
  } while (wrap);

  return 0;
};

BlockManager::BlockManager (Fweelin *app) : 
  manageblocks(0), himanageblocks(0), threadgo(1), app(app) {
  pre_growchain = new PreallocatedType(app->getMMG(),
                                       ::new GrowChainManager(),
                                       sizeof(GrowChainManager));
  pre_peaksavgs = new PreallocatedType(app->getMMG(),
                                       ::new PeaksAvgsManager(),
                                       sizeof(PeaksAvgsManager));
  pre_hipri = new PreallocatedType(app->getMMG(),
                                   ::new HiPriManagedChain(),
                                   sizeof(HiPriManagedChain));
  pre_stripeblock = new PreallocatedType(app->getMMG(),
                                         ::new StripeBlockManager(),
                                         sizeof(StripeBlockManager));

  pthread_mutex_init(&manage_thread_lock,0);

  const static size_t STACKSIZE = 1024*128;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("BLOCK: Stacksize: %zd.\n",STACKSIZE);

  // Start a block managing thread
  int ret = pthread_create(&manage_thread,
                           &attr,
                           run_manage_thread,
                           static_cast<void *>(this));
  if (ret != 0) {
    printf("(blockmanager) pthread_create failed, exiting");
    exit(1);
  }
  RT_RWThreads::RegisterReaderOrWriter(manage_thread);
  
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  // Manage thread calls delete- can't be SCHED_FIFO
  schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
  //  schp.sched_priority = sched_get_priority_min(SCHED_FIFO);
  if (pthread_setschedparam(manage_thread, SCHED_OTHER, &schp) != 0) {    
    printf("BLOCK: Can't set realtime thread, will use nonRT!\n");
  }
}

BlockManager::~BlockManager () {
  // Terminate the management thread
  threadgo = 0;
  pthread_join(manage_thread,0);

  pthread_mutex_destroy (&manage_thread_lock);

  delete pre_growchain;
  delete pre_peaksavgs;
  delete pre_hipri;
  delete pre_stripeblock;
}

// Turns on automatic allocation of new blocks at the end of the
// specified chain. We work in conjunction with the specified iterator, 
// so that when the iterator approaches the end of the block chain, 
// the chain grows automatically
void BlockManager::GrowChainOn (AudioBlock *b, AudioBlockIterator *i) {
  // Check if the block is already being watched
  DelManager(&manageblocks,b,T_MC_GrowChain);
  // Tell the manage thread to grow this chain
  GrowChainManager *nw = (GrowChainManager *) pre_growchain->RTNewWithWait();
  nw->b = b;
  nw->i = i;
  AddManager(&manageblocks,nw);
}

void BlockManager::GrowChainOff (AudioBlock *b) {
  DelManager(&manageblocks,b,T_MC_GrowChain);
}

// Turns on computation of running sample peaks and averages
// for the specified Block & Iterator
// We compute as the currenty iterated position advances
PeaksAvgsManager *BlockManager::PeakAvgOn (AudioBlock *b, 
                                           AudioBlockIterator *i,
                                           char grow) {
  // Check if the block is already being watched
  DelManager(&manageblocks,b,T_MC_PeaksAvgs);
  // Tell the manage thread to compute peaks & averages
  PeaksAvgsManager *nw = (PeaksAvgsManager *) pre_peaksavgs->RTNewWithWait();
  nw->bmg = this;
  nw->b = b;
  nw->i = i;
  nw->grow = grow;
  nw->Setup();
  AddManager(&manageblocks,nw);
  
  return nw;
}

void BlockManager::PeakAvgOff (AudioBlock *b) {
  DelManager(&manageblocks,b,T_MC_PeaksAvgs);
}

void BlockManager::StripeBlockOn (void *trigger, AudioBlock *b, 
                                  AudioBlockIterator *i) {
  // Check if the block is already being watched
  DelHiManager(&himanageblocks,b,T_MC_StripeBlock,trigger);
  // Tell the manage thread to stripe beats on blocks
  StripeBlockManager *nw = (StripeBlockManager *) pre_stripeblock->
    RTNewWithWait();
  nw->pre_tm = app->getPRE_TIMEMARKER();
  nw->b = b;
  nw->i = i;
  nw->trigger = trigger;
  nw->Setup();
  AddHiManager(&himanageblocks,nw);
}

void BlockManager::StripeBlockOff (void *trigger, AudioBlock *b) {
  DelHiManager(&himanageblocks,b,T_MC_StripeBlock,trigger);
}

// Generic delete/add functions for managers (not hipri)
void BlockManager::DelManager (ManagedChain *m) {
  DelManager(&manageblocks,m);
};
void BlockManager::AddManager (ManagedChain *nw) {
  AddManager(&manageblocks,nw);
};

void BlockManager::DelManager (ManagedChain **first, ManagedChain *m) {
  ManagedChain *cur = *first;
  
  // Search for manager 'm' in our list
  while (cur != 0 && (cur != m || cur->status == T_MC_PendingDelete))
    cur = cur->next;
  
  if (cur != 0)
    // Flag for deletion
    cur->status = T_MC_PendingDelete;
};

void BlockManager::RefDeleted (ManagedChain **first, void *ref) {
  ManagedChain *cur = *first;
  while (cur != 0) {
    if (cur->status == T_MC_Running && cur->RefDeleted(ref))
      // Flag for deletion
      cur->status = T_MC_PendingDelete;

    // Next chain
    cur = cur->next;
  }
}

void BlockManager::HiRefDeleted (HiPriManagedChain **first, void *ref) {
  HiPriManagedChain *cur = *first;
  while (cur != 0) {
    if (cur->status == T_MC_Running && cur->RefDeleted(ref))
      // Flag for deletion
      cur->status = T_MC_PendingDelete;

    // Next chain
    cur = (HiPriManagedChain *) cur->next;
  }
}

// Notify all Managers that the object pointed to has been deleted-
// To avoid broken dependencies
void BlockManager::RefDeleted (void *ref) {
  RefDeleted(&manageblocks,ref);
  HiRefDeleted(&himanageblocks,ref);
}

// Activate a hipriority trigger- all hiprimanagedchains with
// specified trigger pointer will have manage() method called
// Safe to call in realtime!
void BlockManager::HiPriTrigger (void *trigger) {
  HiPriManagedChain *cur = himanageblocks;
  
  //printf("HIPRITRIG: %ld\n", mgrcnt);
  
  cur = himanageblocks;
  while (cur != 0) {
    if (cur->status == T_MC_Running &&
        (cur->trigger == trigger || cur->trigger == 0))
      // Ok, right trigger or no trigger specified, call 'em!
      if (cur->Manage()) 
        // Flag for delete
        cur->status = T_MC_PendingDelete;

    // Next chain
    cur = (HiPriManagedChain *) cur->next;
  }
}

// Returns the 1st chain manager associated with block b
// that has type t
ManagedChain *BlockManager::GetBlockManager(AudioBlock *o, 
                                            ManagedChainType t) {
  ManagedChain *cur = manageblocks;
  
  // Search for block 'o' && type t in our list
  while (cur != 0 && (cur->status == T_MC_PendingDelete ||
                      cur->b != o || cur->GetType() != t)) 
    cur = cur->next;
  
  return cur;
};

void BlockManager::AddManager (ManagedChain **first, ManagedChain *nw) {
  nw->status = T_MC_Running;

  // Possibility of priority inversion if AddManager is run in RT,
  // because non-RT manage thread also locks here. So far, AddManager shouldn't
  // be run in RT
  if (pthread_mutex_trylock (&manage_thread_lock) == 0) {
    ManagedChain *cur = *first;
    if (cur == 0)
      *first = nw; // That was easy, now we have 1 item
    else {
      while (cur->next != 0)
        cur = cur->next;
      cur->next = nw; // Link up the last item to new1
    }
    
    pthread_mutex_unlock (&manage_thread_lock);
  }
  else
    // Priority inversion
    printf("BLOCK: WARNING: Priority inversion during AddManager\n");
}

void BlockManager::AddHiManager (HiPriManagedChain **first, 
                                 HiPriManagedChain *nw) {
  nw->status = T_MC_Running;

  // Possibility of priority inversion if AddHiManager is run in RT,
  // because non-RT manage thread also locks here. 
  // So far, AddHiManager shouldn't be run in RT
  if (pthread_mutex_trylock (&manage_thread_lock) == 0) {
    HiPriManagedChain *cur = *first;
    if (cur == 0)
      *first = nw; // That was easy, now we have 1 item
    else {
      while (cur->next != 0)
        cur = (HiPriManagedChain *) cur->next;
      cur->next = nw; // Link up the last item to new1
    }
    
    pthread_mutex_unlock (&manage_thread_lock);
  } else
    // Priority inversion
    printf("BLOCK: WARNING: Priority inversion during AddHiManager\n");
}

// Delete a managed chain for block o and manager type t
// If t is T_MC_None, removes the first managed chain for 'o' 
// of any type
void BlockManager::DelManager (ManagedChain **first, AudioBlock *o,
                               ManagedChainType t) {
  ManagedChain *cur = *first;
  
  // Search for block 'o' in our list of type 't'
  while (cur != 0 && (cur->status == T_MC_PendingDelete ||
                      cur->b != o || (cur->GetType() != t &&
                                      t != T_MC_None))) 
    cur = cur->next;
  
  if (cur != 0)
    // Flag for deletion
    cur->status = T_MC_PendingDelete;
}

// Delete a hiprimanaged chain for block o and manager type t
// with specified trigger.
// If t is T_MC_None, removes the first managed chain for 'o' 
// with specified trigger, of any type
void BlockManager::DelHiManager (HiPriManagedChain **first, 
                                 AudioBlock *o,
                                 ManagedChainType t, void *trigger) {
  HiPriManagedChain *cur = *first;
  
  // Search for block 'o' in our list of type 't'
  while (cur != 0 && (cur->status == T_MC_PendingDelete ||
                      cur->b != o || cur->trigger != trigger ||
                      (cur->GetType() != t && t != T_MC_None)))
    cur = (HiPriManagedChain *) cur->next;
  
  if (cur != 0)
    // Flag for deletion
    cur->status = T_MC_PendingDelete;
}

void *BlockManager::run_manage_thread (void *ptr) {
  BlockManager *inst = static_cast<BlockManager *>(ptr);
  
  while (inst->threadgo) {
    // Manage the blocks we have
    ManagedChain *cur = inst->manageblocks;
    while (cur != 0) {
      if (cur->status == T_MC_Running)
        if (cur->Manage())
          // Flag for deletion
          cur->status = T_MC_PendingDelete;

      // Next chain
      cur = cur->next;
    }

    // Delete managers
    cur = inst->manageblocks;
    ManagedChain *prev = 0;
    while (cur != 0) {
      if (cur->status == T_MC_PendingDelete) {
        // printf("MGR %p DELETE\n",cur);

        // Remove chain
        pthread_mutex_lock (&inst->manage_thread_lock);
        ManagedChain *tmp = cur->next;
        if (prev != 0) 
          prev->next = tmp;
        else 
          inst->manageblocks = tmp;
        pthread_mutex_unlock (&inst->manage_thread_lock);
        //printf("end mgr\n");
        cur->RTDelete();

        cur = tmp;
      } else {
        // Next chain
        prev = cur;
        cur = cur->next;
      }
    }

    HiPriManagedChain *hcur = inst->himanageblocks,
      *hprev = 0;
    while (hcur != 0) {
      if (hcur->status == T_MC_PendingDelete) {
        // printf("HI-MGR %p DELETE\n",hcur);

        // Remove chain
        pthread_mutex_lock (&inst->manage_thread_lock);
        HiPriManagedChain *tmp = (HiPriManagedChain *) hcur->next;
        if (hprev != 0) 
          hprev->next = tmp;
        else 
          inst->himanageblocks = tmp;
        pthread_mutex_unlock (&inst->manage_thread_lock);
        //printf("end mgr\n");
        hcur->RTDelete();

        hcur = tmp;
      } else {
        // Next chain
        hprev = hcur;
        hcur = (HiPriManagedChain *) hcur->next;
      }
    }

    // Produce status report?
    FloConfig *fs = inst->app->getCFG();
    if (fs->status_report == FS_REPORT_BLOCKMANAGER) {
      fs->status_report++;

      printf("BLOCKMANAGER REPORT:\n");
      ManagedChain *cur = inst->manageblocks;
      while (cur != 0) {
        printf(" bmg mgr: type(%d) status(%d)\n",cur->GetType(),cur->status);
        cur = cur->next;
      }

      cur = inst->himanageblocks;
      while (cur != 0) {
        printf(" bmg HiPrimgr: type(%d) status(%d)\n",cur->GetType(),
               cur->status);
        cur = cur->next;
      }
    }

    // 10 ms delay between management tasks
    usleep(10000);
  }

  // Delete all blockmanagers now
  ManagedChain *cur = inst->manageblocks;
  while (cur != 0) {
    ManagedChain *tmp = cur->next;
    cur->RTDelete();
    cur = tmp;
  }
  cur = inst->himanageblocks;
  while (cur != 0) {
    ManagedChain *tmp = cur->next;
    cur->RTDelete();
    cur = tmp;
  }

  return 0;
}
