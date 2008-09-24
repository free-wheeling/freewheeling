/* ************
   FreeWheeling
   ************

   What is music,
   if it is not shared in community,
   held in friendship,
   alive and breathing,
   soil and soul?

   THANKS & PRAISE
*/

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

#include "fweelin_midiio.h"
#include "fweelin_videoio.h"
#include "fweelin_sdlio.h"
#include "fweelin_audioio.h"

#include "fweelin_core.h"
#include "fweelin_core_dsp.h"

#if 1
int main (int argc, char *argv[]) {
  Fweelin flo;
  
  printf("FreeWheeling %s\n",VERSION);
  printf("May we return to the circle.\n\n");

  if (!flo.setup())
    flo.go();
  else
    printf("Error starting FreeWheeling!");
  
  return 0;
}
#endif

// Improvisation is loving what is.
