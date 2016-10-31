#ifndef __FWEELIN_AMIXER_H
#define __FWEELIN_AMIXER_H

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

/* This file contains code from amixer.c, ALSA's command line mixer utility:
 *
 *   ALSA command line mixer utility
 *   Copyright (c) 1999-2000 by Jaroslav Kysela <perex@perex.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifndef __MACOSX__
#include <alsa/asoundlib.h>
#endif

#include "fweelin_core.h"

class Fweelin;

/**
 * Interface class for directly talking to the audio hardware (ALSA) mixer
 *
 * Taken almost verbatim from amixer.c in alsa-utils, with minor changes to fit
 * Freewheeling.
 */
class HardwareMixerInterface {
public:
  HardwareMixerInterface(Fweelin *app) : app(app), prev_hwid(-1) {};

#ifndef __MACOSX__
  /*
   * Set an ALSA Mixer control
   *
   * Parameters:
   * the HW id (ie hw:0, hw:1, etc)
   * the control numid (ie numid=5)
   * and between 1 and 4 values.
   *
   * Returns zero on success.
   */
  int ALSAMixerControlSet(int hwid, int numid, int val1, int val2 = -1, int val3 = -1, int val4 = -1);

private:
  const char *control_type(snd_ctl_elem_info_t *info);
  const char *control_access(snd_ctl_elem_info_t *info);
  long get_integer(char **ptr, long min, long max);
  long get_integer64(char **ptr, long long min, long long max);
  int parse_control_id(const char *str, snd_ctl_elem_id_t *id);
  const char *control_iface(snd_ctl_elem_id_t *id);
  void show_control_id(snd_ctl_elem_id_t *id);
  void print_spaces(unsigned int spaces);
  void print_dB(long dB);
  void decode_tlv(unsigned int spaces, unsigned int *tlv, unsigned int tlv_size);
  int show_control(char *card, const char *space, snd_hctl_elem_t *elem,
      int level);
  int get_ctl_enum_item_index(snd_ctl_t *handle, snd_ctl_elem_info_t *info,
      char **ptrp);
  int cset(char *card, int argc, char *argv[], int roflag, int keep_handle, int debugflag);
#endif

  Fweelin *app;
  int prev_hwid;  // Previous cset call, what was the hwid?
};

#endif
