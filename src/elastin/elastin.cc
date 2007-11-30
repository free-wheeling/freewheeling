// 500 2500 500 500 2.0 2.0
// 1000 2500 1500 128 5.0 2.0 (better freq stability) (*best*)
// 1000 2500 1500 1000 5.0 2.0 (smoother?) 

// These numbers break it: seems to happen for many ratescales below 1.0,
// with -odd- numbers:
// 1000 2500 1500 128 1000 5.0 1.0 0.64 0.0 0.0

// *** For FW:
//
// *** Add functions to RT modify parameters
// *** Compensate for shift in loopstartpt/loopendpt (adjust timedelta)

// OK Implement counter that tells us how far into the original sample we've
// synthesized at any point (ie generated samples / stretch factor for period)
// 
// OK Implement continuous looping of sample (calc loop points and synthesize
// in loop mode) (*accomodate 'tail' when calculating loop points*)
//
// OK Implement RATE shift and "pitch" shift (using combined stretch/rate)
//
// *** Analyze with 'shorts'
// *** Variable crossfade length (longer crossfades seem to produce more
// consistent harmonics, but add 'subtle echos' on transients-- balance)
// *** Optimize analysis (most melodic passages have consistent chunk lengths)
// *** Bug on 1.0 stretch-- extra samples written (crossfadelen samples)
// *** Interpolate chunks 

// Split harmonic / inharmonic portion
//  .. use IFFT phasevoc for harmonic (or large fragmentsize)
//  .. use timedomain for noise p
//  .. or use separate timedomain SOLA methods on each portion

// TEST RESULTS using women2/women2b.
// show that it's OK to encode/decode sample and keep markers
// (as would happen in saving/loading loops)
// hard to detect quality loss

// OLD NOTES
// Idea: Within each chunk we have subchunks
// We also subdivide by frequency bands. 
//
// OK* Move beginning of chunk instead of j2 pos (alternate)
//     (on seven.wav, sounds better with alternate movepoints)
// *** Anticipatory repeat-- if we know some poorly scored areas are coming up
// **** Recursive analysis-- we go into poorly scored areas with subresolution
//     to extract smaller repeating segments
// *** Problems seem to occur around transients... subresolution around 
//     areas with sharp peaks?
// Use peak profile to pick spots to reduce chunksize... zoom in on those
// areas
// -or-
// use really small chunks and do correlation between chunks
// then string together many successive chunks that are very similar
// (correlate well to each other) and repeat the whole set, instead of 
// just one at a time (better frequency resolution)

#include <stdio.h>
#include <string.h>

#include "elastin.h"

// Favor next chunk threshhold-
// A higher FAVOR_THRESH means the next chunk has to be -significantly-
// better than this chunk for crossfading to be chosen.
#define FAVOR_THRESH 0.5 

// How much better (reduced ratio) does a static chunk insertion have to be
// before we go ahead and do it
#define MOVETHRESH 0.9

Elastin_Data::~Elastin_Data() {
  // *** Free analysis data!
  Elastin_Jump *cur = firstj;
  while (cur != 0) {
    Elastin_Jump *tmp = cur->next;
    delete cur;
    cur = tmp;
  }

  if (loopj != 0)
    delete loopj;
};

Elastin::~Elastin() {
  if (sbufs != 0)
    Takedown_SampleBufs();
};

void Elastin::Takedown_SampleBufs() {
  // *** Free sample buffers
  for (int i = 0; i < d->numch; i++) 
    delete[] sbufs[i];
  delete[] sbufs;
};

void Elastin::Setup_SampleBufs() {
  // *** Allocate sample buffers
  sbufs = new el_sample_t *[d->numch];
  for (int i = 0; i < d->numch; i++) 
    sbufs[i] = new el_sample_t[(d->maxchunk+d->cmpchunk)*2];
};

Elastin_Data *Elastin::Analyze_Start (Elastin_SampleFeed *feed, 
				      el_nframes_t maxcrossfadelen,
				      el_nframes_t minchunk, 
				      el_nframes_t maxchunk,
				      el_nframes_t cmpchunk) {
  if (cmpchunk > maxchunk)
    return 0;
  if (d != 0 && d->firstj != 0)
    return 0;
  if (d != 0)
    fprintf(stderr,"ELASTIN: WARNING: Starting analyze when already attached "
	    "to an Elastin_Data\n");

  d = new Elastin_Data();  
  d->feed = feed;
  d->minchunk = minchunk;
  d->maxchunk = maxchunk;
  d->cmpchunk = cmpchunk;

  d->slen = feed->GetLength();
  d->numch = feed->GetNumChannels();
  if (d->numch <= 0)
    return 0;

  if (sbufs != 0)
    Takedown_SampleBufs();
  Setup_SampleBufs();

  if (maxcrossfadelen != 0)
    // Add static chunk for beginning
    d->AddJump(new Elastin_Jump(0,maxcrossfadelen,
				maxcrossfadelen,-1,JUMPFLAG_STATIC));

#if 0
  // DEPRECATED- mono

  // Now scan the whole sample and extract amplitude envelope
#define AMPENV_CHUNK 512
  el_sample_t *ampbuf = new el_sample_t[AMPENV_CHUNK];
  el_nframes_t apos = 0;

  el_nframes_t ampenvlen = slen/AMPENV_CHUNK;
  el_sample_t *ampenv = 0;
  if (ampenvlen > 0)
    ampenv = new el_sample_t[ampenvlen];

  int ampenvidx = 0;
  while (ampenvidx < ampenvlen) {
    el_nframes_t aremaining = slen-apos;
    if (aremaining > AMPENV_CHUNK)
      aremaining = AMPENV_CHUNK;

    // Get samples
    feed->GetSamples(apos,aremaining,ampbuf);

    // Compute amplitude for this chunk
    if (aremaining > 0) {
      register el_sample_t amp = 0.0;
      for (register el_nframes_t n = 0; n < aremaining; n++)
	amp += fabs(ampbuf[n]);
      ampenv[ampenvidx] = amp/aremaining;
    } else
      ampenv[ampenvidx] = 0;

    if (ampenvidx > 0 && ampenvidx+1 < ampenvlen)
      DEBUGPRINT("d: %f\n",ampenv[ampenvidx]-ampenv[ampenvidx-1]);

    // Advance
    ampenvidx++;
    apos += aremaining;
  }

  delete[] ampbuf;
#endif

  spos = maxcrossfadelen;
  return d;
};

int Elastin::Analyze_Stop (el_nframes_t loopstartpt, el_nframes_t loopendpt) {
  if (d == 0)
    return -1;

  if (d->analysisdone) {
    // Add last chunks (static)
    if (d->lastj != 0) {
      if (d->slen - d->lastj->j2 > d->maxchunk*2)
	fprintf(stderr,"ELASTIN: ERROR: End static chunk too large!\n");

      d->AddJump(new Elastin_Jump(d->lastj->j2, d->slen,
				  d->slen - d->lastj->j2, -1,
				  JUMPFLAG_STATIC));
    }

#ifdef COMPUTE_LOOP_POINTS
    // Compute loop points from end<>begin

    // Use begin/end points specified
    DEBUGPRINT("LOOP: %d>%d rough endpoints given\n",loopstartpt,loopendpt);

    // Read in samples
    // We will use sbufs split evenly in two. The first half of the buffer
    // will store samples around the loop start point. The second half
    // of the buffer will contain samples around the loop end point. 
    // We need to grab an extra 'cmpchunk' samples at the end to give the
    // correlation something to chew on.
    el_nframes_t halfpt = d->maxchunk + d->cmpchunk;
    
    // Check range start1->start2 against end1->end2
    el_nframes_t start1 = loopstartpt - d->maxchunk/2, // Feed indices for 
                                                       // start range
      start2 = loopstartpt + d->maxchunk/2 + d->cmpchunk,
      s_orig = d->maxchunk/2, // Index in sbufs of loopstartpt sample
      end1 = loopendpt - d->maxchunk/2, // Feed indices for end range
      end2 = loopendpt + d->maxchunk/2 + d->cmpchunk,
      e_orig = halfpt + d->maxchunk/2; // Index in sbufs of loopendpt sample

    // Ensure comparison ranges are within sample bounds
    if (start1 < 0) {
      start2 -= start1; // Extend end of range (to accomodate clipped begin)
      s_orig += start1; // Therefore, center moves forward
      start1 = 0;
    }
    if (end2 >= d->slen) {
      end1 -= (end2 - d->slen + 1);   // Bring forward beginning of range 
                                      // (accomodate clipped end)
      e_orig += (end2 - d->slen + 1); // Therefore, center moves towards end
      end2 = d->slen-1; 
    }
  
    // Ensure default start and end points give enough space for correlate
    if (s_orig < 0) {
      DEBUGPRINT("LOOP: Default loop begin too early- "
		 "not enough space for correlate- fixed!\n");
      loopstartpt -= s_orig;
      s_orig = 0;
    } 

    if (e_orig + d->cmpchunk >= 2*halfpt) {
      DEBUGPRINT("LOOP: Default loop end too late- "
		 "not enough space for correlate- fixed!\n");
      loopendpt += (2*halfpt - d->cmpchunk - 1 - e_orig);
      e_orig = 2*halfpt - d->cmpchunk - 1;
    }

    DEBUGPRINT("LOOP: StartChunkLen: %d EndChunkLen: %d S_orig: %d E_orig: %d\n",
	       start2-start1,
	       end2-end1, s_orig, e_orig);

    if (start2 >= d->slen || end1 < 0) {
      // Really short sample- no loop points possible
      fprintf(stderr,"ELASTIN: *** Short sample! No loop points possible!\n");
    } else {
      // Get start and end parts to compare

      // Get start
      d->feed->GetSamples(start1,start2-start1,sbufs);
      // Get end
      el_sample_t *sbufs_tmp[d->numch];
      for (int ch = 0; ch < d->numch; ch++)
	sbufs_tmp[ch] = &(sbufs[ch][halfpt]);
      d->feed->GetSamples(end1,end2-end1,sbufs_tmp);

      // Now, find best correlate position
      char first = 1;
      float bestcor = 0.0;
      el_nframes_t bestcoridx = 0;

      // Moving loopstartpt with loopendpt pinned
      el_nframes_t boundary = halfpt-d->cmpchunk; // Can't correlate past halfpt
      for (el_nframes_t i = 0; i < boundary; i++) {
	float cor = Correlate(i,e_orig,d->cmpchunk);
	// DEBUGPRINT("BEGIN: %d cor: %f\n",i,cor);
	if (cor < bestcor) {
	  bestcor = cor;
	  bestcoridx = i;
	} else if (first) {
	  first = 0;
	  bestcor = cor;
	  bestcoridx = i;
	}
      }

      // Moving loopendpt with loopstartpt pinned
      boundary = 2*halfpt-d->cmpchunk; // Can't correlate past end of buffer
      for (el_nframes_t i = halfpt; i < boundary; i++) {
	float cor = Correlate(s_orig,i,d->cmpchunk);
	// DEBUGPRINT("END: %d cor: %f\n",i-halfpt,cor);
	if (cor < bestcor) {
	  bestcor = cor;
	  bestcoridx = i;
	} else if (first) {
	  first = 0;
	  bestcor = cor;
	  bestcoridx = i;
	}
      }
      
      // Best?
      if (bestcoridx < halfpt) {
	// Moving loopstartpt with loopendpt pinned
	d->loopj = new Elastin_Jump(start1+bestcoridx,loopendpt,0,bestcor,
				    JUMPFLAG_LOOPPT);
	DEBUGPRINT("LOOP: Move start: %d>%d\n",d->loopj->j1,d->loopj->j2);
      } else {
	// Moving loopendpt with loopstartpt pinned
	d->loopj = new Elastin_Jump(loopstartpt,end1+bestcoridx-halfpt,0,bestcor,
				    JUMPFLAG_LOOPPT);
	DEBUGPRINT("LOOP: Move end: %d>%d\n",d->loopj->j1,d->loopj->j2);
      }

      // Determine which chunk the loop start point is in
      // Store that in loopj->next
      Elastin_Jump *cur = d->firstj;
      d->loopj->next = 0;
      while (cur != 0 && d->loopj->next == 0) {
	if (d->loopj->j1 >= cur->j1 &&
	    d->loopj->j1 < cur->j2)
	  d->loopj->next = cur;
	cur = cur->next;
      }
      if (d->loopj->next == 0)
	fprintf(stderr,"ELASTIN: ERROR: Loop start point not within any chunk!\n");

      DEBUGPRINT("DONE!\n");
    }
#endif

#ifdef USE_SYNTHMAX_SCORE
    // Recalibrate scores

    // Get mean score
    float meanscore = 0.0;
    int scorecnt = 0;
    Elastin_Jump *cur = firstj;
    while (cur != 0) {
      if (!cur->IsStatic()) {
	meanscore += cur->score;
	scorecnt++;
      }
      cur = cur->next;
    }
    if (scorecnt > 1) {
      meanscore /= (float) scorecnt;
      DEBUGPRINT("Mean score: %f\n",meanscore);

      // Now compute standard deviation
      float stddev = 0;
      cur = firstj;
      while (cur != 0) {
	if (!cur->IsStatic()) {
	  float dev = cur->score - meanscore;
	  stddev += dev*dev;
	}
	cur = cur->next;
      }
      stddev = sqrt(stddev/(scorecnt-1));
      DEBUGPRINT("Std dev: %f\n",stddev);

      // Now recalibrate scores in terms of deviation from mean
      cur = firstj;
      while (cur != 0) {
	if (!cur->IsStatic()) {
	  cur->score = (cur->score - meanscore)/stddev;
	  DEBUGPRINT("jump: len(%d) %d>%d score: %f\n",cur->jlen,cur->j1,cur->j2,
		     cur->score);
	}
	cur = cur->next;
      }
    }
#endif
  }

  return 0;
};

// Get amplitude envelope from spos[idx1]->spos[idx2]
el_sample_t Elastin::Analyze_GetAmpEnv (el_nframes_t idx1, 
					el_nframes_t idx2) {
  el_sample_t alen = (idx2-idx1)*d->numch;
  if (alen == 0)
    return 0.0;

  register el_sample_t amp = 0.0;
  for (int i = 0; i < d->numch; i++) {
    register el_sample_t *sbuf = sbufs[i];
    for (register el_nframes_t n = idx1; n < idx2; n++)
      amp += fabs(sbuf[n]);
  }

  return amp/alen;
};

#define DO_COR(i1,i2) \
  (Correlate(i1,i2,cmpchunk) * pow((i2)-(i1),0.5))

int Elastin::Analyze (int numchunks) {
  // static el_sample_t prevamp = 0.0;

  if (d == 0 || d->feed == 0)
    return -1;

  char go = 1;
  for (int i = 0; go && i < numchunks; i++) {
    el_nframes_t runlen = d->maxchunk*2,
      remaining = d->slen-spos,
      passmaxchunk = d->maxchunk,
      passminchunk = d->minchunk;
    char gopass = 1;

    if (remaining < runlen) {
      // Approaching end of sample-- shorten pass
      go = 0;
      runlen = remaining;
      passmaxchunk = runlen/2;
      if (passminchunk > passmaxchunk)
	gopass = 0;
    }
    
    if (gopass) {
      // Read in samples
      d->feed->GetSamples(spos,runlen,sbufs);

      // Now, find best correlate position
      char first = 1;
      float bestcor = 0.0;
      el_nframes_t bestcoridx = 0,
	minchunk = d->minchunk,
	cmpchunk = d->cmpchunk;
      for (el_nframes_t i = minchunk; i < passmaxchunk; i++) {
	float cor = DO_COR(0,i); 
	if (cor < bestcor) {
	  bestcor = cor;
	  bestcoridx = i;
	} else if (first) {
	  first = 0;
	  bestcor = cor;
	  bestcoridx = i;
	}
      }

      float bestcor1 = bestcor;

      // Try move begin
      el_nframes_t moverange = bestcoridx - d->minchunk,
	bestcoridx_b = 0;
      for (el_nframes_t i = 1; i < moverange; i++) {
	float cor = DO_COR(i,bestcoridx);
	if (cor < bestcor) {
	  bestcor = cor;
	  bestcoridx_b = i;
	}
      }

      if (bestcoridx_b != 0 && bestcor/bestcor1 < MOVETHRESH) {
	DEBUGPRINT("bestcor_b: %f->%f (%d>%d)\n",bestcor1,bestcor,
		   bestcoridx_b,bestcoridx);

	// Add static portion from 0>bestcoridx_b
#if 0
	el_sample_t amp = Analyze_GetAmpEnv(0,bestcoridx_b);
	DEBUGPRINT("d: %f [%f]\n",amp-prevamp,-1.0);
	prevamp = amp;
#endif
	d->AddJump(new Elastin_Jump(spos,spos+bestcoridx_b,bestcoridx_b,-1,
				    JUMPFLAG_STATIC));
      } else {
	// Try move begin 2
	moverange = passmaxchunk - d->minchunk;
	// if too many large static chunks causing excessive reps---
	// if (moverange > MAX_STATIC_LEN) moverange = MAX_STATIC_LEN

	bestcoridx_b = 0;
	el_nframes_t cmpchunk = d->cmpchunk;
	for (el_nframes_t i = 1; i < moverange; i++) {
	  float cor = DO_COR(i,passmaxchunk);
	  if (cor < bestcor) {
	    bestcor = cor;
	    bestcoridx_b = i;
	  }
	}

	if (bestcoridx_b != 0 && bestcor/bestcor1 < MOVETHRESH) {
	  DEBUGPRINT("bestcor_b2: %f->%f (%d>%d)\n",bestcor1,bestcor,
		     bestcoridx_b,passmaxchunk);

	  // Add static portion from 0>bestcoridx_b
#if 0
	  el_sample_t amp = Analyze_GetAmpEnv(0,bestcoridx_b);
	  DEBUGPRINT("d: %f [%f]\n",amp-prevamp,-1.0);
	  prevamp = amp;
#endif
	  d->AddJump(new Elastin_Jump(spos,spos+bestcoridx_b,bestcoridx_b,-1,
				      JUMPFLAG_STATIC));

	  // Use bestcoridx_b > passmaxchunk jump
	  bestcoridx = passmaxchunk;
	} else {
	  // Use original jump
	  bestcoridx_b = 0;
	  bestcor = bestcor1;
	}
      }
      
      // Store bestcoridx/bestcor as a jump, and continue there
#if 0
      el_sample_t amp = Analyze_GetAmpEnv(bestcoridx_b,bestcoridx);
      DEBUGPRINT("d: %f [%f]\n",amp-prevamp,bestcor);
      prevamp = amp;
#endif

      Elastin_Jump *newj;
      d->AddJump(newj = new Elastin_Jump(spos+bestcoridx_b,spos+bestcoridx,
					 bestcoridx-bestcoridx_b,bestcor));
      DEBUGPRINT("jump: len(%d) %d>%d score: %f\n",newj->jlen,newj->j1,newj->j2,
		 newj->score);
      spos += bestcoridx;
    }
  }

  if (!go) 
    d->analysisdone = 1;

  return (go ? 0 : -1);
};

int Elastin::Synthesize_Start (el_nframes_t startpos, 
			       el_nframes_t crossfadelen, 
			       char loopmode,
			       el_nframes_t loop_crossfadelen, 
			       float synthmaxscore,
			       el_sample_t **tempbufs,
			       el_nframes_t tempbufslen,
			       float timestretch,
			       float rateshift,
			       float ts_pitchshift,
			       float rt_pitchshift) {
  // Check if we've analyzed audio
  if (d == 0 || d->feed == 0 || !d->analysisdone || d->firstj == 0 ||
      sbufs == 0)
    return -1;

  // Check crossfade length
  // *** Loop cross fade length can not exceed 'cmpchunk' because
  // it's possible that loopj->j2 is as close as cmpchunk samples
  // to the end of the sample!
  if ((loopmode && loop_crossfadelen > d->cmpchunk) || 
      crossfadelen > d->maxchunk || 
      crossfadelen > d->firstj->j2)
    return -2;

  this->tempbufs = tempbufs;
  this->tempbufslen = tempbufslen;
  this->tempbuf_pos = 0.0;
  this->tempbuf_filled = 0;

  this->crossfadelen = crossfadelen;
  this->loopmode = loopmode;
  this->loop_crossfadelen = loop_crossfadelen;

  this->timestretch = timestretch;
  this->rateshift = rateshift;
  this->ts_pitchshift = ts_pitchshift;
  this->rt_pitchshift = rt_pitchshift;
  Synth_ComputeParams();

  this->synthmaxscore = synthmaxscore;
  
  cfcnt = 0;
  forcedstatic = 0;
  timedelta = 0;
  synthcount = 0.0;
  // *** Startpos not yet implemented
  curj = d->firstj;
  Synth_SetupChunk(0);

  return 0;
};

int Elastin::Synthesize_Stop () {
  return 0;
};

el_nframes_t Elastin::Synthesize (el_sample_t **bufs, el_nframes_t len) {
  if (bufs == 0)
    return 0; // Zero buffer!

  if (combirateshift != 1.0) {
    // Timestretch + Rateshift (TS+RT)
    // DEBUGPRINT("tempbuf_pos: %.5lf\n",tempbuf_pos);

    // Check if intermediate buffers are big enough
    // Endpos is the end position 
    // (ie the new starting position in tempbuf after this pass, unwrapped)
    double endpos = tempbuf_pos + combirateshift * len;

    // Ensure there's always an extra sample for interpolation
    el_nframes_t endpos_withpad = (el_nframes_t) endpos + 1; 
    if (tempbufs == 0 || endpos_withpad >= tempbufslen) {
      fprintf(stderr,"ELASTIN: ERROR: Tempbufs not allocated or not big enough"
	      " for synthesize: combirateshift: %.2f tempbufslen: %d "
	      "tempbufs: %p!\n",
	      combirateshift,tempbufslen,tempbufs);

      // Shunt rate shift
      return Synthesize_TS(bufs,len);
    }

    // First, pull and timestretch audio (accounting for filled spot in 
    // tempbufs from previous pass)
    el_sample_t *tempbufs_tmp[d->numch];
    for (int ch = 0; ch < d->numch; ch++)
      tempbufs_tmp[ch] = &(tempbufs[ch][tempbuf_filled]);
    el_nframes_t tofill = endpos_withpad - tempbuf_filled + 1;
    // DEBUGPRINT("Filled: %d Tofill: %d\n",tempbuf_filled,tofill);
    if (tofill < 0)
      tofill = 0;

    el_nframes_t ts_outlen = Synthesize_TS(tempbufs_tmp,tofill);
    
    // Check if TS actually synthesized 'tofill' samples
    char end = 0;
    el_nframes_t ret;
    if (ts_outlen != tofill) {
      // No, must be end of sample-- adjust length we produce

      // Use bounds checking Synthesize_RT pass to produce -just enough-
      // output given the reduced size in tempbufs (ts_outlen + tempbuf_filled)
      end = 1;
      ret = Synthesize_RT(tempbufs,bufs,len,1,ts_outlen + tempbuf_filled);
    } else 
      // Second, rate shift audio in tempbufs
      ret = Synthesize_RT(tempbufs,bufs,len);

    if (end) {
      tempbuf_pos = 0.0;
      tempbuf_filled = 0;
    } else {
      // Third, advance tempbuf_pos, and wrap based on fractional part of 
      // endpos- this gives us the new starting position relative to
      // the start of tempbufs
      el_nframes_t endpos_d1 = (el_nframes_t) endpos;
      tempbuf_pos = endpos - endpos_d1;
      
      // Move sample(s) at end of buffer to beginning for next pass
      tempbuf_filled = endpos_withpad - endpos_d1 + 1;
      for (el_nframes_t n = 0; n < tempbuf_filled; n++)
	for (int ch = 0; ch < d->numch; ch++)
	  tempbufs[ch][n] = tempbufs[ch][endpos_d1 + n];

      // DEBUGPRINT("endpos: %.5lf\n",endpos);
      // DEBUGPRINT("copied %d positions @ %d > head ",tempbuf_filled,endpos_d1);
      // DEBUGPRINT("(sample: %f)\n",tempbufs[0][0]);
    }

    return ret;
  } else {
    if (tempbufs != 0 && tempbuf_filled != 0) {
      // *** TEST THIS

      // We have some data already generated from a TS+RT pass (above)
      // Use it first
      el_sample_t *bufs_tmp[d->numch];
      for (int ch = 0; ch < d->numch; ch++) {
	memcpy(&(bufs[ch][0]),&(tempbufs[ch][0]),
	       tempbuf_filled*sizeof(el_sample_t));
	bufs_tmp[ch] = &(bufs[ch][tempbuf_filled]);
      }

      // Now synthesize remaining samples
      el_nframes_t ret = Synthesize_TS(bufs_tmp,len - tempbuf_filled) 
	+ tempbuf_filled;  

      // Reset
      tempbuf_filled = 0;

      return ret;
    } else
      // Timestretch only!
      return Synthesize_TS(bufs,len);  
  }
};

el_nframes_t Elastin::Synthesize_RT (el_sample_t **tempbufs, 
				     el_sample_t **bufs,
				     el_nframes_t outlen,
				     char checkinbounds,
				     el_nframes_t inlen_max) {
  register double tb_pos = tempbuf_pos; // *** Can use float?

  // Compute fractional and integer part of position, and linear interpolate
  // using this sample and adjacent across all channels
#define DO_RT_SCALE \
      el_nframes_t intpos = (el_nframes_t) tb_pos; \
      float fracpos = tb_pos - intpos, \
	oneminus_fracpos = 1.0 - fracpos; \
      \
      for (int ch = 0; ch < d->numch; ch++) \
	bufs[ch][n] = \
	  oneminus_fracpos * tempbufs[ch][intpos] + \
	  fracpos * tempbufs[ch][intpos+1];

  if (checkinbounds) {
    // Slow (in bounds checking)
    DEBUGPRINT("Slow RT scale w/ bounds check!\n");

    if ((el_nframes_t) tb_pos + 1 >= inlen_max) {
      // No samples can be produced- need at least 2 samples in tempbufs
      return 0;
    }
      
    for (el_nframes_t n = 0; n < outlen; n++) {
      DO_RT_SCALE;

      tb_pos += combirateshift;
      if ((el_nframes_t) tb_pos + 1 >= inlen_max) {
	// Reached boundary! Return # produced so far
	return n+1;
      }
    }
  } else {
    // Fast (no in bounds checking)
    //
    // We assume there are enough samples in
    // tempbufs for rate scale)
    for (el_nframes_t n = 0; n < outlen; n++, tb_pos += combirateshift) {
      DO_RT_SCALE;
    }
  }

  // DEBUGPRINT("RTshift: float pos AT,AFTER end of pass: %.5lf, %.5lf (combirateshift: %.5f)\n",tb_pos - combirateshift,tb_pos,combirateshift);
  // DEBUGPRINT("RTshift: int pos AT end of pass (+1 interpolate): %d ",(el_nframes_t) (tb_pos - combirateshift) + 1);
  // DEBUGPRINT("(sample: %f)\n",tempbufs[0][(el_nframes_t) (tb_pos - combirateshift) + 1]);

  return outlen;
};

el_nframes_t Elastin::Synthesize_TS (el_sample_t **bufs, el_nframes_t len) {
  el_nframes_t len_remaining = len,
    bufidx = 0;
  char run_again;
  do {
    run_again = 0;

    // Compute run length
    el_nframes_t run_len = runremaining;
    if (run_len > len_remaining)
      run_len = len_remaining;
    
    // Run 
    if (run_len > 0) {
      switch (ss) {
      case EL_StaticRun :
      case EL_StaticRun_EarlyFade :
      case EL_StaticRun_LoopPt :
      case EL_Run :
      case EL_Run_LoopPt :
	{
	  // Copy
	  for (int ch = 0; ch < d->numch; ch++)
	    memcpy(&(bufs[ch][bufidx]),&(sbufs[ch][chunkidx]),
		   run_len*sizeof(el_sample_t));
	}
	break;
	
      case EL_Fade :
	{
	  // Fade	  
	  float fv1 = 0.0, 
	    fv2 = 0.0;
	  for (int ch = 0; ch < d->numch; ch++) {
	    register el_sample_t *bb = &(bufs[ch][bufidx]),
	      *sb1 = &(sbufs[ch][fadepos1]),
	      *sb2 = &(sbufs[ch][fadepos2]);
	    fv1 = fadevol1;
	    fv2 = fadevol2;
	    for (el_nframes_t i = 0; i < run_len; i++, bb++, sb1++, sb2++) {
	      *bb = fv1 * *sb1 + fv2 * *sb2;
	      fv1 -= fadeinc;
	      fv2 += fadeinc;
	      
	      // Bounds check on fadevols?
	    }
	  } 

	  fadepos1 += run_len;
	  fadepos2 += run_len;
	  fadevol1 = fv1;
	  fadevol2 = fv2;
	}
	break;
	
      default:
	break;
      }
      
      // Advance
      runremaining -= run_len;
      bufidx += run_len;
      chunkidx += run_len;
    }

    // Next pass
    if (runremaining <= 0) {
      // Prepare for next pass
      len_remaining -= run_len;
      if (len_remaining > 0)
	run_again = 1;
      
      // Chunk boundary
      switch (ss) {
      case EL_StaticRun_LoopPt :
      case EL_Run_LoopPt :
	{
	  // Crossfade at loop point
	  cft = CF_LoopPt;

	  DEBUGPRINT("Setup fade (length %d)\n",loop_crossfadelen);

	  // Get first part of crossfade (around loopj->j2)
	  spos = d->loopj->j2;
	  chunkidx = 0;
	  d->feed->GetSamples(spos,loop_crossfadelen,sbufs);

	  // Get second part of crossfade, store immediately after first in
	  // sbufs
	  spos = d->loopj->j1;
	  el_sample_t *sbufs_tmp[d->numch];
	  for (int ch = 0; ch < d->numch; ch++)
	    sbufs_tmp[ch] = &(sbufs[ch][loop_crossfadelen]);
	  d->feed->GetSamples(spos,loop_crossfadelen,sbufs_tmp);

	  // Relocate jump 
	  curj = d->loopj->next;

	  // Setup crossfade
	  fadepos1 = 0;
	  fadepos2 = loop_crossfadelen;
	  fadevol1 = 1.0;
	  fadevol2 = 0.0;
	  fadeinc = 1.0/loop_crossfadelen;
	  runremaining = loop_crossfadelen;
	  // *** What to do about timedelta?
	  // I think this is right. Compensate for crossfade time.
	  timedelta += (combitimestretch-1.0)*runremaining;

	  ss = EL_Fade;
	}
	break;

      case EL_StaticRun :
      case EL_StaticRun_EarlyFade :
      case EL_Run :
	{
	  // Check-- should we crossfade back to repeat, crossfade forward
	  // to skip, or continue straight thru?
	  
	  if (Synth_CheckCFBack()) {
	    // Crossfade back
	    cft = CF_Back;
	    cfcnt++;

	    DEBUGPRINT("TD: %lf [cfback (cfcnt %d)]\n",timedelta,cfcnt);

	    fadepos1 = chunkidx;
	    fadepos2 = 0;
	    fadevol1 = 1.0;
	    fadevol2 = 0.0;
	    fadeinc = 1.0/crossfadelen;
	    runremaining = crossfadelen;
	    // Compensate for samples added
	    // DEBUGPRINT("compensate timedelta: %lf -> ",timedelta);
	    timedelta -= curj->jlen;
	    // DEBUGPRINT("%lf\n",timedelta);

	    ss = EL_Fade;
	  } else if (ss == EL_StaticRun_EarlyFade ||
		     (ss == EL_Run && (cfcnt = Synth_CheckCFForward()) != 0)) {
	    // Crossfade forward 'cfcnt' chunks
	    cft = CF_Forward;

	    // Get first part of crossfade
	    spos = curj->j2 - crossfadelen; // Current spot
	    chunkidx = 0;
	    d->feed->GetSamples(spos,crossfadelen,sbufs);

	    // Skip chunks
	    int cnt = 0;
	    el_nframes_t skiplen = 0;
	    for (; cnt < cfcnt && curj->next != 0; cnt++) {
	      curj = curj->next;
	      skiplen += curj->jlen;
	    }
	    if (cnt < cfcnt) {
	      // Skipping stopped early- no more chunks!- stop!
	      synthcount += bufidx / combitimestretch;
	      return bufidx;
	    }

	    DEBUGPRINT("TD: %lf [cffwd %d chunks (%d len)]\n",
		       timedelta,cfcnt,skiplen);
	       
	    // Get second part of crossfade, store immediately after first in
	    // sbufs
	    spos = curj->j2 - crossfadelen; // Current spot
	    el_sample_t *sbufs_tmp[d->numch];
	    for (int ch = 0; ch < d->numch; ch++)
	      sbufs_tmp[ch] = &(sbufs[ch][crossfadelen]);
	    d->feed->GetSamples(spos,crossfadelen,sbufs_tmp);

	    // Setup crossfade
	    fadepos1 = 0;
	    fadepos2 = crossfadelen;
	    fadevol1 = 1.0;
	    fadevol2 = 0.0;
	    fadeinc = 1.0/crossfadelen;
	    runremaining = crossfadelen;
	    // Compensate for samples removed-- include crossfade as 
	    // part of last chunk
	    timedelta += (combitimestretch-1.0)*runremaining + 
	      combitimestretch*skiplen;

	    ss = EL_Fade;
	  } else {
	    // No crossfade--
	    // Continue to next chunk
	    cfcnt = 0;

	    DEBUGPRINT("TD: %lf [NO CF]\n",timedelta);

	    if (curj->next != 0) {
	      Elastin_Jump *prevj = curj;
	      curj = curj->next;
	      Synth_SetupChunk(prevj);
	    } else {
	      // No more chunks-- stop!
	      synthcount += bufidx / combitimestretch;
	      return bufidx;
	    }
	  }
	}
	break;

      case EL_Fade :
	{
	  // Switch from fade to run!
	  // DEBUGPRINT("fadedone\n");

	  if (cft == CF_Forward) {
	    // Finished crossfade forward- load samples for next chunk
	    if (curj->next != 0) {
	      curj = curj->next;
	      Synth_SetupChunk(0);
	    } else {
	      // No more chunks-- stop!
	      synthcount += bufidx / combitimestretch;
	      return bufidx;
	    }	    
	  } else if (cft == CF_Back) {
	    // Finished crossfade backward- run remaining samples
	    chunkidx = fadepos2; // Start at end-of-fade position
	    runremaining = curj->jlen - crossfadelen;
	    ss = EL_Run;
	  } else if (cft == CF_LoopPt) {
	    // Finished crossfade at loop point- run starting at new curj-
	    // tell SetupChunk we've looped by passing loopj, which is flagged
	    // for IsLoopPt()
	    DEBUGPRINT("Done looppt fade. Setup start point.\n");
	    Synth_SetupChunk(d->loopj);
	  }
	}
	break;

      default:
	break;
      }
    }
  } while (run_again);

  synthcount += bufidx / combitimestretch;
  return bufidx; 
};

// Returns nonzero if, given current conditions, we should crossfade back
char Elastin::Synth_CheckCFBack () {  
  // Don't crossfade back static chunks or if we are speeding up audio
  if (curj->IsStatic(synthmaxscore) || forcedstatic || combitimestretch <= 1.0) {
    forcedstatic = 0;
    return 0;
  }

  DEBUGPRINT("timedelta: %lf\n",timedelta);

  int average_repeats = (int) combitimestretch;
  if (timedelta > 0.0) {
    // Next chunk is static? Must repeat then, to avoid getting
    // way out of time
    if (curj->next == 0 || curj->next->IsStatic(synthmaxscore)) 
      return 1;

    // Spread to next chunk?
    if (cfcnt >= average_repeats) {
      DEBUGPRINT("spread to next [%d]\n",cfcnt);
      return 0;
    }
    
    // Favor next chunk?
    if (curj->score - curj->next->score >= FAVOR_THRESH) {
      DEBUGPRINT("favor next [%d] [%f/%f]\n",cfcnt,
		 curj->next->score,curj->score);
      return 0;
    }
    
    // Otherwise, repeat now
    return 1;	
  } else if (cfcnt < average_repeats-1 && curj->next != 0 && 
	     curj->next->IsStatic(synthmaxscore) &&
	     timedelta + (combitimestretch-1.0)*curj->next->jlen > 0.0) {
    // Anticipate that we will need to repeat a chunk--
    // if the next chunk is static and that chunk will cause us
    // to be behind, repeat this earlier chunk instead of many
    // repeats after the static chunk
    DEBUGPRINT("anticipate cfback [%d]\n",cfcnt);
    return 1;
  } else
    // No crossfade back
    return 0;
};

// Check whether we should crossfade forward from current position-
// Returns the number of jumps to skip, or zero if no crossfade forward
// should be done
int Elastin::Synth_CheckCFForward () {
  Elastin_Jump *cjn = curj->next;
  int cnt = 0;
  double td = timedelta;

  while (1) {
    // DEBUGPRINT("timedelta: %lf skipfwdcnt: %d\n",td,cnt);

    // Don't crossfade forward across static chunks 
    // or if we are slowing down audio
    if (cjn == 0 || cjn->IsStatic(synthmaxscore) ||
	combitimestretch >= 1.0) 
      return cnt;

    int average_skips = (int) (combitimestretch <= 0.0 ? -1 : (1.0/combitimestretch));
    if (td < 0.0) {
      // Next next chunk is static? Must skip then, to avoid getting
      // way out of time
      if (cjn->next == 0 || cjn->next->IsStatic(synthmaxscore)) 
	return cnt+1;

      // Spread to next chunk?
      if (average_skips > 0 && cnt >= average_skips) {
	DEBUGPRINT("spread to next [%d]\n",cnt);
	return cnt;
      }
    
      // Favor next chunk?
      if (cjn->score - cjn->next->score >= FAVOR_THRESH) {
	DEBUGPRINT("favor next [%d] [%f/%f]\n",cnt,
		   cjn->next->score,cjn->score);
	return cnt;
      }
    
      // Otherwise, skip
      cnt++;
    } else if (average_skips > 0 && cnt < average_skips-1 &&
	       cjn->next != 0 && cjn->next->IsStatic(synthmaxscore) &&
	       td + (combitimestretch-1.0)*cjn->jlen < 0.0) {
      // Anticipate that we will need to skip a chunk--
      // if the next next chunk is static and that chunk will cause us
      // to be ahead, skip this earlier coming chunk instead of many
      // skips after the static chunk
      DEBUGPRINT("anticipate cffwd [%d]\n",cnt);
      cnt++;
    } else
      // No more crossfade forward
      return cnt;

    // Advance to next skip
    td += (combitimestretch-1.0)*cjn->jlen;
    cjn = cjn->next;
  }
};

void Elastin::Synth_SetupChunk (Elastin_Jump *prevj) {
  if (prevj != 0 && prevj->IsLoopPt()) {
    DEBUGPRINT("After loop fade: Reposition!\n");

    // We just looped. Loop fade may have moved us to a new chunk.
    // Reposition
    spos += loop_crossfadelen; 
    
    // If necessary, jump to next chunk
    while (spos >= curj->j2 && curj != 0) {
      DEBUGPRINT("next chunk!\n");
      curj = curj->next;
    }
  }

  if (curj->IsStatic()) {
    // Static run
    if (prevj != 0 && prevj->IsLoopPt()) {
      // Following loop fade, new position...

      // We will load the sample from spos to j2
      DEBUGPRINT("After loop fade: New startpt- STATIC run!\n");
      chunkidx = 0;
      runremaining = curj->j2 - spos;
    } else if (prevj == 0 || prevj->IsStatic() || forcedstatic) {
      forcedstatic = 0;
      DEBUGPRINT("static-- j1\n");
      
      // Previous chunk was static/crossfade forward--
      // so we are at j1 position.
      spos = curj->j1;
      chunkidx = 0;
      runremaining = curj->jlen;
    } else {
      DEBUGPRINT("static-- j1 crossfp\n");
      
      // Previous regular chunk, so we are crossfadelen samples
      // before j1
      spos = curj->j1 - crossfadelen;
      chunkidx = 0;
      runremaining = curj->jlen + crossfadelen;
    }

    char islooppt = 0;
    if (loopmode && d->loopj != 0 && curj->j2 >= d->loopj->j2) {
      if (d->loopj->j2 < curj->j1) {
	// Loop point actually happened already- how did we miss it?
	fprintf(stderr,"ELASTIN: ERROR: Loop point missed!\n");
      } else {
	// Loop point happens during this chunk. Make note and reduce length.
	runremaining -= (curj->j2 - d->loopj->j2);
	islooppt = 1;
      }
    }
      
    // Bring timedelta up-to-date with the end of the new chunk
    double otimedelta = timedelta;
    timedelta += (combitimestretch-1.0)*runremaining;
    
    // Advance check for crossfade forward- we need to prepare before 
    // the end of the static chunk in this case
    // Technically, there is a small (crossfadelen) discrepancy, where
    // we will tend to favor a crossfadeforward- because normally we check
    // crossfade using timedelta @ the position before j1, here we are checking
    // fully at the j1 position of the next chunk. However, this is not
    // cumulative.
    if (!islooppt && runremaining >= crossfadelen && 
	(cfcnt = Synth_CheckCFForward()) != 0) {
      // Crossfade forward, so shorten run to provide space for crossfade
      DEBUGPRINT("Static Run Early Fade!\n");
      runremaining -= crossfadelen;
      timedelta = otimedelta + (combitimestretch-1.0)*runremaining;
      ss = EL_StaticRun_EarlyFade;
    } else if (islooppt) {
      DEBUGPRINT("StaticRun @ LoopPt\n");
      ss = EL_StaticRun_LoopPt;
    } else
      ss = EL_StaticRun;

    // Get new samples
    if (runremaining > 0)
      d->feed->GetSamples(spos,runremaining,sbufs);
  } else {
    // Regular run
    el_nframes_t chunklen = curj->jlen + crossfadelen;
    
    if (prevj != 0 && prevj->IsLoopPt()) {
      // Following loop fade, new position...

      DEBUGPRINT("After loop fade: New startpt- REGULAR run!\n");

      chunkidx = spos - curj->j1 + crossfadelen;
      runremaining = curj->j2 - crossfadelen - spos;
      
      if (runremaining < 0) {
	// If runremaining < 0, we are beyond the cross fade position 
	// (crossfadelen samples before j2). There is no way to start
	// a crossfade now, so we will have to play out this block in a static
	// way

	// Play to j2, load chunk from spos to j2
	chunkidx = 0;
	runremaining = curj->j2 - spos;
	chunklen = runremaining;

	// Force static run
	DEBUGPRINT("Forced static after looppt fade!\n");
	forcedstatic = 1;
      } else
	// Load samples starting from before j1, in case we need to 
	// crossfade back right away
	spos = curj->j1 - crossfadelen;
    } else {
      spos = curj->j1 - crossfadelen;
      
      if (prevj == 0 || prevj->IsStatic() || forcedstatic) {
	forcedstatic = 0;
	DEBUGPRINT("reg-- j1\n");
	
	// Previous chunk was static/crossfade forward-- 
	// so we are at j1 position.
	runremaining = curj->jlen - crossfadelen;
	chunkidx = crossfadelen;
	
	if (spos < 0) {
	  fprintf(stderr,"ELASTIN: ERROR: Crossfade- not enough space!\n");
	  
	  // Not enough space for crossfade, disable 
	  chunklen = curj->jlen;
	  spos = 0;	  
	  chunkidx = 0;
	  runremaining += crossfadelen;
	}
      } else {
	DEBUGPRINT("reg-- j1 crossfp\n");
	
	// Previous regular chunk, so we are crossfadelen samples
	// before j1
	runremaining = curj->jlen;
	chunkidx = 0;
	
	if (spos < 0) {
	  fprintf(stderr,
		  "ELASTIN: ERROR: Not first chunk but no data for crossfade?\n");
	  spos = 0;
	} 
      }
    }

    if (forcedstatic) {
      // Finished with loop point crossfade, but finished late in chunk-
      // force static play
      ss = EL_StaticRun;
    } else if (loopmode && d->loopj != 0 && curj->j2 >= d->loopj->j2) {
      if (d->loopj->j2 < curj->j1) {
	// Loop point actually happened already- how did we miss it?
	fprintf(stderr,"ELASTIN: ERROR: Loop point missed!\n");
      } else {
	// Loop point happens during this chunk. Recalculate runremaining-
	// Runremaining now takes us crossfadelen samples before j2.
	// Runremaining should take us right to the loop point
	DEBUGPRINT("Regular Run @ LoopPt\n");
	runremaining += crossfadelen + d->loopj->j2 - curj->j2;
      }
      
      // Regular run cut off by loop point
      ss = EL_Run_LoopPt;
    } else
      // Regular run
      ss = EL_Run;
    
    // Get new samples
    if (runremaining > 0)
      d->feed->GetSamples(spos,chunklen,sbufs);

    // Bring timedelta up-to-date with the end of the new chunk
    // DEBUGPRINT("advance td: %lf -> ",timedelta);
    timedelta += (combitimestretch-1.0)*runremaining;
    // DEBUGPRINT("%lf\n",timedelta);
  }
};
