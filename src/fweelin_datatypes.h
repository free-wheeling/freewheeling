#ifndef __FWEELIN_DATATYPES_H
#define __FWEELIN_DATATYPES_H

#include <jack/ringbuffer.h>

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

enum CoreDataType {
  T_char,
  T_int,
  T_long,
  T_float,
  T_range,
  T_variable,
  T_variableref,
  T_invalid
};

CoreDataType GetCoreDataType(char *name);

class Range {
 public:
  Range(int lo, int hi) : lo(lo), hi(hi) {};

  int lo,
    hi;
};

// Flexible data type configuration variable-
// Used in parsing and evaluating expressions from config file

#define CFG_VAR_SIZE 16 // Number of data bytes in one variable
class UserVariable {

 public:
  UserVariable() : name(0), value(data), next(0) {};
  ~UserVariable() { 
    if (name != 0) 
      delete[] name;
  };

  // Ensures that the precision of this variable is at least that of src
  // If not, reconfigures this variable to match src..
  // For ex, if this is T_char and src is T_float, this becomes T_float
  void RaisePrecision (UserVariable &src) {
    switch (src.type) {
    case T_char :
      break;
    case T_int :
      if (type == T_char) {
        int tmp = (int) *this;
        type = T_int;
        *this = tmp;
      }
      break;
    case T_long :
      if (type == T_char || type == T_int) {
        long tmp = (long) *this;
        type = T_long;
        *this = tmp;
      }
      break;
    case T_float : 
      if (type == T_char || type == T_int || type == T_long) {
        float tmp = (float) *this;
        type = T_float;
        *this = tmp;
      }
      break;
    default :
      break;
    }
  };

  char operator > (UserVariable &cmp) {
    RaisePrecision(cmp);
    // Comparing ranges yields undefined results
    if (type == T_range || cmp.type == T_range)
      return 0;
    switch (type) {
      case T_char : 
        return (*((char *) value) > (char) cmp);
      case T_int : 
        return (*((int *) value) > (int) cmp);
      case T_long : 
        return (*((long *) value) > (long) cmp);
      case T_float : 
        return (*((float *) value) > (float) cmp);
      case T_variable :
      case T_variableref :
        printf(" UserVariable: WARNING: Compare T_variable or T_variableref "
               " not implemented!\n");
        return 0;      
      case T_range : 
        printf(" UserVariable: WARNING: Can't compare range variable!\n");
        return 0;
      case T_invalid : 
        printf(" UserVariable: WARNING: Can't compare invalid variable!\n");
        return 0;
    }
    
    return 0;
  };
  
  char operator == (UserVariable &cmp) {
    RaisePrecision(cmp);
    // Special case if one variable is range and one is scalar-- then
    // we check if the scalar is within the range
    if (type == T_range && cmp.type != T_range) {
      int v = (int) cmp;
      Range r(*((int *) value),*(((int *) value)+1));
      return (v >= r.lo && v <= r.hi);
    }
    if (cmp.type == T_range && type != T_range) {
      int v = (int) *this;
      Range r(*((int *) cmp.value),*(((int *) cmp.value)+1));
      return (v >= r.lo && v <= r.hi);
    }
    switch (type) {
    case T_char : 
      return (*((char *) value) == (char) cmp);
    case T_int : 
      return (*((int *) value) == (int) cmp);
    case T_long : 
      return (*((long *) value) == (long) cmp);
    case T_float : 
      return (*((float *) value) == (float) cmp);
    case T_range : 
      {
        Range r = (Range) cmp;
        return (*((int *) value) == r.lo && 
                *(((int *) value)+1) == r.hi);
      }
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Compare T_variable or T_variableref "
             " not implemented!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't compare invalid variable!\n");
      return 0;
    }

    return 0;
  };

  char operator != (UserVariable &cmp) {
    return !(operator == (cmp));
  };

  void operator += (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) += (char) src;
      break;
    case T_int : 
      *((int *) value) += (int) src;
      break;
    case T_long : 
      *((long *) value) += (long) src;
      break;
    case T_float : 
      *((float *) value) += (float) src;
      break;
    case T_range : 
      {
        Range r = (Range) src;
        *((int *) value) += r.lo; 
        *(((int *) value)+1) += r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
             " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  // Return the absolute value of the difference (delta) between this variable and arg
  UserVariable GetDelta (UserVariable &arg) {
    UserVariable ret;
    ret.type = T_char;
    ret.RaisePrecision(*this);
    ret.RaisePrecision(arg);
    
    switch (ret.type) {
      case T_char :
        ret = (char) abs((char) arg - (char) *this);
        break;
      case T_int :
        ret = (int) abs((int) arg - (int) *this);
        break;
      case T_long :
        ret = (long) labs((long) arg - (long) *this);
        break;
      case T_float :
        ret = (float) fabsf((float) arg - (float) *this);
        break;
      default :
        printf(" UserVariable: WARNING: GetDelta() doesn't work on this type of variable!\n");
        break;
    }
    
    return ret;
  };
  
  void operator -= (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) -= (char) src;
      break;
    case T_int : 
      *((int *) value) -= (int) src;
      break;
    case T_long : 
      *((long *) value) -= (long) src;
      break;
    case T_float : 
      *((float *) value) -= (float) src;
      break;
    case T_range : 
      {
        Range r = (Range) src;
        *((int *) value) -= r.lo; 
        *(((int *) value)+1) -= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
             " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  void operator *= (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) *= (char) src;
      break;
    case T_int : 
      *((int *) value) *= (int) src;
      break;
    case T_long : 
      *((long *) value) *= (long) src;
      break;
    case T_float : 
      *((float *) value) *= (float) src;
      break;
    case T_range : 
      {
        Range r = (Range) src;
        *((int *) value) *= r.lo; 
        *(((int *) value)+1) *= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
             " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  void operator /= (UserVariable &src) { 
    switch (type) {
    case T_char : 
    case T_int :
    case T_long :
    case T_float :
      {
        // Special case- when dividing a scalar by another scalar, the 
        // result is always evaluated to a float!!
        float t = (float) src;
        
        // Convert this variable to a float
        if (t != 0) {
          *((float *) value) = (float) *this / t;
          type = T_float;
        }
      }
      break;
    case T_range : 
      {
        Range r = (Range) src;
        if (r.lo != 0)
          *((int *) value) /= r.lo; 
        if (r.hi != 0)
          *(((int *) value)+1) /= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
             " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  UserVariable & operator = (char src) { 
    *((char *) value) = src; 
    return *this;
  };
  UserVariable & operator = (int src) { 
    *((int *) value) = src; 
    return *this;
  };
  UserVariable & operator = (long src) { 
    *((long *) value) = src; 
    return *this;
  };
  UserVariable & operator = (float src) { 
    *((float *) value) = src; 
    return *this;
  };
  UserVariable & operator = (Range src) { 
    *((int *) value) = src.lo; 
    *(((int *) value)+1) = src.hi;
    return *this;
  };

  operator char () {
    switch (type) {
    case T_char : return *((char *) value);
    case T_int : return (char) *((int *) value);
    case T_long : return (char) *((long *) value);
    case T_float : return (char) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator int () {
    switch (type) {
    case T_char : return (int) *((char *) value);
    case T_int : return *((int *) value);
    case T_long : return (int) *((long *) value);
    case T_float : return (int) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator long () {
    switch (type) {
    case T_char : return (long) *((char *) value);
    case T_int : return (long) *((int *) value);
    case T_long : return *((long *) value);
    case T_float : return (long) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator float () {
    switch (type) {
    case T_char : return (float) *((char *) value);
    case T_int : return (float) *((int *) value);
    case T_long : return (float) *((long *) value);
    case T_float : return *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator Range () {
    switch (type) {
    case T_char : return Range(*((char *) value), *((char *) value));
    case T_int : return Range(*((int *) value), *((int *) value));
    case T_long : return Range(*((long *) value), *((long *) value));
    case T_float : return Range((int) *((float *) value), 
                                (int) *((float *) value));
    case T_range : return Range(*((int *) value),*(((int *) value)+1));
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return Range(0,0);      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return Range(0,0);
    }

    return Range(0,0);
  };

  void operator = (UserVariable &src) {
    // Assignment operator does not copy name to avoid memory alloc 
    // problems
    type = src.type;
    memcpy(data,src.data,CFG_VAR_SIZE);
    if (src.value == src.data) 
      value = data;
    else
      value = src.value; // System variable, copy data ptr directly
  };

  // Sets this UserVariable from src, converting from src type to this
  void SetFrom(UserVariable &src) {
    switch (type) {
    case T_char :
      *this = (char) src;
      break;
    case T_int : 
      *this = (int) src;
      break;
    case T_long : 
      *this = (long) src;
      break;
    case T_float :
      *this = (float) src;
      break;
    case T_range :
      *this = (Range) src;
      break;
    case T_variable :
    case T_variableref :
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't set from invalid variable!\n");
      break;
    }
  }

  // Dump UserVariable to string str (maxlen is maximum length) 
  // or stdout if str = 0
  void Print(char *str = 0, int maxlen = 0);

  inline char IsSystemVariable() { return (value != data); };
  inline char *GetValue() { return value; }; // Returns the raw data bytes for the value of this variable
  inline CoreDataType GetType() { return type; };
  inline char *GetName() { return name; };
  
  char *name;
  CoreDataType type;
  char data[CFG_VAR_SIZE];

  // System variables are a special type of variable that is created by the 
  // core FreeWheeling system and not the user.
  //
  // A system variable is essentially a pointer to an internal data value
  // inside FreeWheeling. It can be read by the configuration system like
  // any other user variable, but it accesses directly into FreeWheeling's
  // internal memory and so it has a mind of its own
  //
  // If value points to the data array, this is a user variable
  // If value does not point to data, this is a system variable
  char *value;

  UserVariable *next;
};

// Data for writer threads (common to all instances of SRMWRingBuffer)
class SRMWRingBuffer_Writers {
public:
  
  #define MAX_WRITER_THREADS 50  // Hard-wired maximum number of writer threads

  // Global prep methods
  static void InitAll() {
     pthread_mutex_init(&register_lock,0);
     num_writers = 0;
  };  
  static void CloseAll() {
     pthread_mutex_destroy(&register_lock);
     num_writers = 0;
  };  
  static void RegisterWriter (pthread_t id) {
    pthread_mutex_lock (&register_lock);
    if (num_writers >= MAX_WRITER_THREADS) {
      printf("CORE: ERROR: Too many writer threads for Ring Buffer!\n");
      exit(1);
    }
    ids[num_writers] = id;
    num_writers++;
    pthread_mutex_unlock (&register_lock);    
  };
  static void RegisterWriter () {
    pthread_mutex_lock (&register_lock);
    if (num_writers >= MAX_WRITER_THREADS) {
      printf("CORE: ERROR: Too many writer threads for Ring Buffer!\n");
      exit(1);
    }
    ids[num_writers] = pthread_self();
    num_writers++;
    pthread_mutex_unlock (&register_lock);    
  };
  
  static int num_writers;                   // Number of writer threads registered
  static pthread_t ids[MAX_WRITER_THREADS]; // Thread ID for each writer
  static pthread_mutex_t register_lock;
};

// Ringbuffer implementation for a Single Reader Thread & Multiple Writer Threads
//
// This expands to a set of ringbuffers- one for each writer thread. The order of elements between threads is not preserved.
// Elements are written and read using COPY operations.
template <class T> class SRMWRingBuffer {
  // Create a ringbuffer with numel elements of class T
  SRMWRingBuffer (int numel) : numel(numel) {
    wbufs = new jack_ringbuffer_t *[SRMWRingBuffer_Writers::num_writers];
    for (int i = 0; i < SRMWRingBuffer_Writers::num_writers; i++)
      wbufs[i] = jack_ringbuffer_create(sizeof(T) * numel);
  };
  ~SRMWRingBuffer() {
    for (int i = 0; i < SRMWRingBuffer_Writers::num_writers; i++)
      jack_ringbuffer_free(wbufs[i]);
    delete[] wbufs;
  };
  
  // Instance methods
  
  int WriteElement (T *el) {
    // Determine which write thread we are
    pthread_t id = pthread_self();
    for (int i = 0; i < SRMWRingBuffer_Writers::num_writers; i++)
      if (pthread_equal(id,SRMWRingBuffer_Writers::ids[i])) {
        // Write to the appropriate ringbuf
        if (jack_ringbuffer_write(wbufs[i],el,sizeof(T)) < sizeof(T)) {
          printf("CORE: No space in RingBuffer for element\n");
          return -1;
        } else
          return 0;
      }
      
    printf("CORE: RingBuffer write from unregistered write thread!\n");
    return -1;
  };
  
  T *ReadElement () {
    // Check each ring buffer
    for (int i = 0; i < SRMWRingBuffer_Writers::num_writers; i++) {
      size_t avail = jack_ringbuffer_read_space(wbufs[i]);
      if (avail >= sizeof(T)) {
        // Read element here
        if (jack_ringbuffer_read(wbufs[i],tmpread,sizeof(T)) != sizeof(T)) {
          printf("CORE: Size mismatch during RingBuffer read\n");
          return 0;
        } else
          return tmpread;
      }
    }
    
    // No data available
    return 0;
  };
  
protected:

  // Array of single-read single-write ring buffers: one for each writer thread
  jack_ringbuffer_t **wbufs;
  int numel;  // Number of elements of class T in the buffer
  char tmpread[sizeof(T)];  // Holding spot for read
};

#endif
