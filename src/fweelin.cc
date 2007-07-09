/* ************
   FreeWheeling
   ************

   What is music,
   if it is not shared in community,
   held in friendship,
   alive and breathing,
   soil and soul?

   THANKS & PRAISE

   (c) Jan P Mercury 2000-2007
*/

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
  printf("Brought to you by a loan from Mother Earth.\n");
  printf("May we return many fold what we have taken.\n\n");

  if (!flo.setup())
    flo.go();
  else
    printf("Error starting FreeWheeling!");
  
  return 0;
}
#endif

// Improvisation is loving what is.
