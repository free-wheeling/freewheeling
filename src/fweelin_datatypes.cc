/*
   To change the world,
   I aspire to perceive it differently.
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

#include "fweelin_datatypes.h"

int RT_RWThreads::num_rw_threads = 0;
pthread_t RT_RWThreads::ids[MAX_RW_THREADS];
pthread_mutex_t RT_RWThreads::register_rw_lock;

int RT_RWThreads::num_rt_structs = 0;
RTDataStruct_Updater *RT_RWThreads::rtsructs[MAX_RT_STRUCTS];
pthread_mutex_t RT_RWThreads::register_rtstruct_lock;

CoreDataType GetCoreDataType(char *name) {
  if (!strcmp(name, "char")) 
    return T_char;
  else if (!strcmp(name, "int")) 
    return T_int;
  else if (!strcmp(name, "long")) 
    return T_long;
  else if (!strcmp(name, "float")) 
    return T_float;
  else if (!strcmp(name, "range"))
    return T_range;
  else
    return T_invalid;
};

