/*
   To change the world,
   I aspire to perceive it differently.
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

#include "fweelin_datatypes.h"

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

