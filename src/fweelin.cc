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

#include <signal.h>
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

#include "stacktrace.h"

#include "fweelin_midiio.h"
#include "fweelin_videoio.h"
#include "fweelin_sdlio.h"
#include "fweelin_audioio.h"

#include "fweelin_core.h"
#include "fweelin_core_dsp.h"

pid_t main_pid;

void signal_handler (int iSignal) {
  switch (iSignal) {
    case SIGINT:
      return;
    #if defined(WIN32)
    #else
    case SIGSEGV:
      printf(">>> FATAL ERROR: Segmentation fault (SIGSEGV) occured! <<<\n");
      break;
    case SIGBUS:
      printf(">>> FATAL ERROR: Access to undefined portion of a memory object (SIGBUS) occured! <<<\n");
      break;
    case SIGILL:
      printf(">>> FATAL ERROR: Illegal instruction (SIGILL) occured! <<<\n");
      break;
    case SIGFPE:
      printf(">>> FATAL ERROR: Erroneous arithmetic operation (SIGFPE) occured! <<<\n");
      break;
    case SIGUSR1:
      printf(">>> User defined signal 1 (SIGUSR1) received <<<\n");
      break;
    case SIGUSR2:
      printf(">>> User defined signal 2 (SIGUSR2) received <<<\n");
      break;
    #endif
    default: { // this should never happen, as we register for the signals we want
      printf(">>> FATAL ERROR: Unknown signal received! <<<\n");
      break;
    }
  }
  signal(iSignal, SIG_DFL); // Reinstall default handler to prevent race conditions
  printf("Saving stack trace to file 'fweelin-stackdump'...\n");
  
  char buf[256];
  snprintf(buf,255,"%s%s",FWEELIN_DATADIR,"/gdb-stackdump-cmds");
  StackTrace(buf);
  
  sleep(10);
  printf("Exit Freewheeling...\n");
  // Use abort() if we want to generate a core dump.
  kill(main_pid, SIGKILL);
}


#ifndef NO_COMPILE_MAIN
int main (int /*argc*/, char *argv[]) {
#if !defined(WIN32)
  main_pid = getpid();
#endif // WIN32

  // Initialize the stack trace mechanism 
  StackTraceInit(argv[0], -1);

  signal(SIGINT, signal_handler);

#if !defined(WIN32)
  // Register signal handlers
  struct sigaction sact;
  sigemptyset(&sact.sa_mask);
  sact.sa_flags   = 0;
  sact.sa_handler = signal_handler;
  sigaction(SIGSEGV, &sact, NULL);
  sigaction(SIGBUS,  &sact, NULL);
  sigaction(SIGILL,  &sact, NULL);
  sigaction(SIGFPE,  &sact, NULL);
  sigaction(SIGUSR1, &sact, NULL);
  sigaction(SIGUSR2, &sact, NULL);
#endif // WIN32

  Fweelin flo;
  
  printf("FreeWheeling %s\n",VERSION);
  printf("May we return to the circle.\n\n");

  if (!flo.setup())
    flo.go();
  else
    printf("Error starting FreeWheeling!\n");
  
  return 0;
}
#endif // NO_COMPILE_MAIN


// Improvisation is loving what is.
