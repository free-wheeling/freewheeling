#ifndef __FWEELIN_DATATYPES_H
#define __FWEELIN_DATATYPES_H

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


#include <jack/ringbuffer.h>
#include <assert.h>


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
  UserVariable() : name(0), type(T_invalid), value(data), next(0) {};
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
    case T_char  : return Range(      *((char  *) value),       *((char  *) value   ));
    case T_int   : return Range(      *((int   *) value),       *((int   *) value   ));
    case T_long  : return Range(      *((long  *) value),       *((long  *) value   ));
    case T_float : return Range((int) *((float *) value), (int) *((float *) value   ));
    case T_range : return Range(      *((int   *) value),       *(((int  *) value)+1));
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
             "T_variableref!\n");
      return Range(0,0);      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return Range(0,0);
    default: assert(0);
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
  char data[CFG_VAR_SIZE] = {};

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

// Abstract class to allow updating RT data structures with a new # of reader and writer threads
class RTDataStruct_Updater {
  friend class RT_RWThreads;

protected:

  virtual void UpdateNumRWThreads(int new_num_writers) = 0;
};

// Certain RT data structures require info about which threads read or write to them
// This allows them to be lock-free while stile protecting the data from race conditions.
//
// This class holds data about all reader and writer threads for all RT data structures
// Note that this is global. RT structures may contain extra protections for threads that
// are not actually used for reading/writing to that structure.
class RT_RWThreads {
  template <typename T> friend class SRMWRingBuffer;
  friend class RT_RCU;

public:
  
  #define MAX_RW_THREADS 50      // Hard-wired maximum number of reader and writer threads
  #define MAX_RT_STRUCTS 20      // System-wide total number of RT data structures allowed

  // Global prep methods
  static void InitAll() {
     pthread_mutex_init(&register_rw_lock,0);
     pthread_mutex_init(&register_rtstruct_lock,0);
     num_rw_threads = 0;
     num_rt_structs = 0;
  };  
  static void CloseAll() {
     pthread_mutex_destroy(&register_rw_lock);
     pthread_mutex_destroy(&register_rtstruct_lock);
     num_rw_threads = 0;
     num_rt_structs = 0;
  };  

  // Registration of reader/ writer threads

  // Register a given thread as a writer
  static void RegisterReaderOrWriter (pthread_t id) {
    pthread_mutex_lock (&register_rw_lock);
    if (num_rw_threads >= MAX_RW_THREADS) {
      printf("CORE: ERROR: Too many writer threads for Ring Buffer!\n");
      exit(1);
    }
    printf("CORE: Register ringbuffer writer thread: %lu\n",id);
    ids[num_rw_threads] = id;
    num_rw_threads++;
    pthread_mutex_unlock (&register_rw_lock);

    // Update existing buffers with new writer thread
    UpdateRTStructs();
  };

  // Registers this thread as a writer
  static void RegisterReaderOrWriter () {
    pthread_mutex_lock (&register_rw_lock);
    if (num_rw_threads >= MAX_RW_THREADS) {
      printf("CORE: ERROR: Too many writer threads for Ring Buffer!\n");
      exit(1);
    }
    printf("CORE: Register ringbuffer writer thread: %lu\n",pthread_self());
    ids[num_rw_threads] = pthread_self();
    num_rw_threads++;
    pthread_mutex_unlock (&register_rw_lock);

    // Update existing buffers with new writer thread
    UpdateRTStructs();
  };
  
  // RT data structures are automatically registered and unregistered here.
  // This allows them to be notified of additional reader or writer threads that are starting later.
  // This is important during initialization if RT data structures need to be created before all
  // threads that read or write to them are initialized.
  static void RegisterRTDataStruct (RTDataStruct_Updater *r) {
    pthread_mutex_lock (&register_rtstruct_lock);
    if (num_rt_structs >= MAX_RT_STRUCTS) {
      printf("CORE: ERROR: Too many ring buffers!\n");
      exit(1);
    }
    printf("CORE: Register ringbuffer #%d: %p\n",num_rt_structs,r);
    rtsructs[num_rt_structs] = r;
    num_rt_structs++;
    pthread_mutex_unlock (&register_rtstruct_lock);
  };

  static void UnregisterRTDataStruct (RTDataStruct_Updater *r) {
    pthread_mutex_lock (&register_rtstruct_lock);
    char done = 0;
    for (int i = 0; i < num_rt_structs; i++)
      if (rtsructs[i] == r) {
        printf("CORE: Unregister ringbuffer #%d: %p\n",i,r);
        rtsructs[i] = 0;
        done = 1;
      }

    if (!done)
      printf("CORE: ERROR: Could not find ringbuffer %p to unregister.\n",r);

    pthread_mutex_unlock (&register_rtstruct_lock);
  };

private:

  static void UpdateRTStructs() {
    // Notify every RT data struct of a new reader / writer thread
    pthread_mutex_lock (&register_rtstruct_lock);
    for (int i = 0; i < num_rt_structs; i++)
      if (rtsructs[i] != 0)
        rtsructs[i]->UpdateNumRWThreads(num_rw_threads);
    pthread_mutex_unlock (&register_rtstruct_lock);
  };

  static int num_rw_threads;                   // Number of reader and writer threads registered
  static pthread_t ids[MAX_RW_THREADS];        // Thread ID for each reader / writer
  static pthread_mutex_t register_rw_lock;

  static int num_rt_structs;                             // Number of RT data structures in the system
  static RTDataStruct_Updater *rtsructs[MAX_RT_STRUCTS]; // Array of pointers to RT data structure updaters
  static pthread_mutex_t register_rtstruct_lock;
};

// Ringbuffer implementation for a Single Reader Thread & Multiple Writer Threads
//
// This expands to a set of ringbuffers- one for each writer thread. The order of elements between threads is not preserved.
// Elements are written and read using COPY operations.
//
// NOTE: Writer threads should not be added and removed once multithreaded operation has begun.
// New writer threads may be registered after the ringbuffer is created, but only while that RingBuffer
// is being written to by a single thread (ie during a single initialization thread).
//
// Class T must be able to be assigned to a null integer.
// If class T is a pointer, no problem.
// If class T is an instance, you must provide an initializer accepting an integer.

template <class T> class SRMWRingBuffer : public RTDataStruct_Updater {
  friend class RT_RWThreads;

public:

  // Create a ringbuffer with numel elements of class T
  SRMWRingBuffer (int numel) : numel(numel), num_writers(RT_RWThreads::num_rw_threads) {
    wbufs = new jack_ringbuffer_t *[MAX_RW_THREADS];
    for (int i = 0; i < num_writers; i++)
      wbufs[i] = jack_ringbuffer_create(sizeof(T) * numel);

    // Register this ringbuf
    RT_RWThreads::RegisterRTDataStruct(this);
  };
  virtual ~SRMWRingBuffer() {
    // Unregister this ringbuf
    RT_RWThreads::UnregisterRTDataStruct(this);

    for (int i = 0; i < num_writers; i++)
      jack_ringbuffer_free(wbufs[i]);
    delete[] wbufs;
  };
  
  // Instance methods
  
  int WriteElement (const T &el) {
    if (num_writers != RT_RWThreads::num_rw_threads) {
      pthread_mutex_lock(&RT_RWThreads::register_rtstruct_lock);
      pthread_mutex_unlock(&RT_RWThreads::register_rtstruct_lock);
      if (num_writers != RT_RWThreads::num_rw_threads) {
        printf("CORE: ERROR: SRMWRingBuffer thread count mismatch.\n");
        exit(1);
      }
    }

    // Determine which write thread we are
    pthread_t id = pthread_self();
    for (int i = 0; i < num_writers; i++)
      if (pthread_equal(id,RT_RWThreads::ids[i])) {
        // Write to the appropriate ringbuf
        if (jack_ringbuffer_write(wbufs[i],(const char *) &el,sizeof(T)) < sizeof(T)) {
          printf("CORE: No space in RingBuffer for element\n");
          return -1;
        } else
          return 0;
      }
      
    printf("CORE: RingBuffer write from unregistered write thread: %lu!\n",id);
    return -1;
  };
  
  const T ReadElement () {
    if (num_writers != RT_RWThreads::num_rw_threads) {
      pthread_mutex_lock(&RT_RWThreads::register_rtstruct_lock);
      pthread_mutex_unlock(&RT_RWThreads::register_rtstruct_lock);
      if (num_writers != RT_RWThreads::num_rw_threads) {
        printf("CORE: ERROR: SRMWRingBuffer thread count mismatch.\n");
        exit(1);
      }
    }

    // Check each ring buffer
    for (int i = 0; i < num_writers; i++) {
      size_t avail = jack_ringbuffer_read_space(wbufs[i]);
      if (avail >= sizeof(T)) {
        // Read element here
        if (jack_ringbuffer_read(wbufs[i],(char *) &tmpread,sizeof(T)) != sizeof(T)) {
          printf("CORE: Size mismatch during RingBuffer read\n");
          exit(1);
        } else {
          // printf("CORE: Ringbuf got item\n");
          return tmpread;
        }
      }
    }
    
    // No data available
    // printf("CORE: Ringbuf empty\n");
    return 0;
  };
  
private:

  virtual void UpdateNumRWThreads(int new_num_rw_threads) {
    printf("CORE: RingBuffer %p: Update reader and writer threads to %d\n",this,new_num_rw_threads);
    if (new_num_rw_threads <= num_writers) {
      printf("CORE: ERROR: Can't go from %d to %d threads during initialization.\n",num_writers,
          new_num_rw_threads);
      exit(1);
    } else {
      for (; num_writers < new_num_rw_threads; num_writers++)
        wbufs[num_writers] = jack_ringbuffer_create(sizeof(T) * numel);
    }
  };

  // Array of single-read single-write ring buffers: one for each writer thread
  jack_ringbuffer_t **wbufs;
  int numel,      // Number of elements of class T in the buffer
    num_writers;  // Local copy of RT_RWThreads::num_rw_threads
  T tmpread;      // Holding spot for read
};

// An RTStore is the concept of a store shelf stocked with cans of something (class T)
// The store shelf has a number of items. Each item has a state. The state can be compared and swapped
// atomically. This allows several threads to work together without locks, stocking and pulling items
// from the shelf.
//
// RTStore does not care about the content of class T. It does not care about the cans.
// It only cares about managing the state of each item on the shelf in a thread-safe, RT-safe way.
//
// This design pattern is used in the memory manager classes (fweelin_mem).
template <class T> class RTStore {
public:
  // 3 states of the transaction at a given index- waiting, busy and done.
  const static int ITEM_WAITING = 0,
    ITEM_BUSY = 1,
    ITEM_DONE = 2;

private:
  class RTStoreItem : public T {
  public:
    RTStoreItem() : item_status(ITEM_DONE) {};

    volatile int item_status;  // Current status of this item (update using careful atomic ops)
  };

public:
  RTStore (int num_items) : num_items(num_items) {
    items = new RTStoreItem[num_items];
  };
  ~RTStore () {
    delete [] items;
  };

  // Finds the first RTStoreItem with the given state 'find_state' and resets the state atomically to
  // 'replace_state'. Returns the index of the store item in the store (in idx), as well as a pointer to the
  // item object itself (return).
  //
  // Returns null if no item could be found with the given state.
  inline T *FindItemWithState (int find_state, int replace_state, int &idx) {
    for (int i = 0; i < num_items; i++) {
      // printf("RTStore Find (%p): %d: state is %d : find %d replace %d\n",this,i,items[i].item_status,find_state,replace_state);

      // Compare and swap, atomically. This guarantees that only one thread can modify an RTStoreItem
      // at a time, without locking.
      if (__sync_bool_compare_and_swap(&items[i].item_status,find_state,replace_state)) {
        // printf(" got it\n");

        // Found.
        idx = i;
        return &items[i];
      }
    }

    // None found
    return 0;
  };

  // Change the state of store item with given index to 'new_state', if the item has the expected state.
  // Returns nonzero if the state matched and was changed. Returns zero if the state did not match.
  inline char ChangeStateAtIdx (int idx, int expect_state, int new_state) {
    // printf("RTStore Change (%p): %d: state is %d : expect %d replace %d\n",this,idx,items[idx].item_status,expect_state,new_state);
    return __sync_bool_compare_and_swap(&items[idx].item_status,expect_state,new_state);
  };

  // Returns the item at the given index. To modify the item, change to the BUSY state first.
  inline T *GetItemAtIdx (int idx) { return &items[idx]; };

private:

  RTStoreItem *items;
  int num_items; // Number of items in the store
};

// Base class for single linked list
class SListItem {
  friend class SLinkList;

public:
  SListItem() : slist_next(0) {};

private:
  SListItem *slist_next;    // Next item pointer
};

// Single linked list. RT-safe but not threadsafe. Must be locked.
class SLinkList {
public:

  SLinkList() : first(0) {};

  inline void AddToHead(SListItem *i) {
    if (first == 0)
      first = i;
    else {
      i->slist_next = first;
      first = i;
    }
  };

  // Finds and removes item i from the list. Returns i if successful, otherwise null.
  inline SListItem *FindAndRemove(SListItem *i) {
    SListItem *cur = first,
      *prev = 0;

    // Search for 'i' in our list
    while (cur != 0 && cur != i) {
      prev = cur;
      cur = cur->slist_next;
    }

    if (cur != 0) {
      // Got one to delete, unlink!
      if (prev != 0)
        prev->slist_next = cur->slist_next;
      else
        first = cur->slist_next;
    }

    return cur;
  };

  inline SListItem *GetFirstItem() {
    return first;
  };

  inline SListItem *GetNextItem(SListItem *cur) {
    return cur->slist_next;
  };

private:

  SListItem *first;
};

class DListItem {
public:
  DListItem() : slist_prev(0), slist_next(0) {};

  SListItem *slist_prev, *slist_next;    // Previous and next item pointers
};

class DLinkList {
  DLinkList() : first(0) {};

  DListItem *first;
};

#endif
