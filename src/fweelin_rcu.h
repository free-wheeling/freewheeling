#ifndef __FWEELIN_RCU_H
#define __FWEELIN_RCU_H

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

#include <stdint.h>

#include "fweelin_datatypes.h"
#include "fweelin_mem.h"

// Realtime RCU (Read-Copy-Update) implementation
// That allows light-weight read access to a data structure without locking.
// And more heavy-weight write access
//
// RT_RCU protects one or more pointers to Preallocated objects.
// (More than one pointer can be protected only if they are independent pointers (update can only happen atomically to one pointer)).
class RT_RCU : public RTDataStruct_Updater {
  friend class RT_RWThreads;

public:

  // Create an RCU helper class T
  RT_RCU () : global_time_count(1), last_update_time(0), num_readers(RT_RWThreads::num_rw_threads) {
    reader_lock_times = new uint32_t[MAX_RW_THREADS];
    for (int i = 0; i < MAX_RW_THREADS; i++)
      reader_lock_times[i] = 0;

    // Register this RCU
    RT_RWThreads::RegisterRTDataStruct(this);
  };
  ~RT_RCU() {
    // Unregister this RCU
    RT_RWThreads::UnregisterRTDataStruct(this);

    delete[] reader_lock_times;
  };
  
  // Instance methods
  
  // Begin read-side critical section - RT and thread-safe
  inline int ReadLock () {
    if (num_readers != RT_RWThreads::num_rw_threads) {
      printf("CORE: ERROR: RT_RCU thread count mismatch.\n");
      exit(1);
    }

    // Determine which read thread we are
    pthread_t id = pthread_self();
    for (int i = 0; i < num_readers; i++)
      if (pthread_equal(id,RT_RWThreads::ids[i])) {
        // Increment global time count atomically, storing old value
        // (then, no two threads will ever lock with the same time count)
        uint32_t cnt = __sync_fetch_and_add(&global_time_count,1);

        // Store global count in reader-specific time count
        reader_lock_times[i] = cnt;

        // Must ensure that reader_locktimes[i] has actually been updated before we return, because that's all that protects the
        // reader from having the data freed from under their feet
        __sync_synchronize();

        return 0;
      }

    printf("CORE: RT_RCU ReadLock from unregistered read thread: %lu!\n",id);
    return -1;
  };

  // End read-side critical section - RT and thread-safe
  inline int ReadUnlock () {
    if (num_readers != RT_RWThreads::num_rw_threads) {
      printf("CORE: ERROR: RT_RCU thread count mismatch.\n");
      exit(1);
    }

    // Determine which read thread we are
    pthread_t id = pthread_self();
    for (int i = 0; i < num_readers; i++)
      if (pthread_equal(id,RT_RWThreads::ids[i])) {
        // Reset state of this reader thread to unlocked
        reader_lock_times[i] = 0;

        __sync_synchronize();
        return 0;
      }

    printf("CORE: RT_RCU ReadUnlock from unregistered read thread: %lu!\n",id);
    return -1;
  };
  
  // Update your reference from old_ptr to new_ptr.
  // *old_ptr = new_ptr;
  // Does this atomically, remembering the time when it was done
  inline void Update (volatile Preallocated **old_ptr, Preallocated *new_ptr) {
    // Update pointer atomically
    *old_ptr = new_ptr;

    // Ensure it has happened in memory by this point
    __sync_synchronize();

    // Increment global time count atomically and store as the last update time
    // (guarantees that *old_ptr == new_ptr by last_update_time)
    last_update_time = __sync_fetch_and_add(&global_time_count,1);
  }

  // Synchronize() is called by the reclaiming thread.
  // It waits (using sleep_time microsecond granules), until all read-side critical sections that started BEFORE the last Update() have completed-
  // This ensures that no readers are holding a pointer to a structure. After this call returns, the old_ptr (before update) may be freed.
  inline void Synchronize (int sleep_time) {
    // A warning is issued after waiting for this many microseconds
    const static int WARNING_WAIT_TIME = 1000000;
    int wait_time = 0;

    char recheck;
    do {
      recheck = 0;
      for (int i = 0; !recheck && i < num_readers; i++)
        if (reader_lock_times[i] != 0 && reader_lock_times[i] < last_update_time)
          // Pointer was updated during grace period (read-side critical section). Therefore, we must wait til this section unlocks.
          recheck = 1;

      if (recheck) {
        if ((wait_time + sleep_time) % WARNING_WAIT_TIME < wait_time % WARNING_WAIT_TIME)
          // Warn that we are waiting too long
          printf("CORE: WARNING: RCU Synchronize() is still waiting (%d secs)...\n",(wait_time + sleep_time) / 1000000);

        // Wait
        wait_time += sleep_time;
        usleep(sleep_time);
      }
    } while (recheck);
  };

private:

  virtual void UpdateNumRWThreads(int new_num_rw_threads) {
    printf("CORE: RT_RCU %p: Update reader and writer threads to %d\n",this,new_num_rw_threads);
    if (new_num_rw_threads <= num_readers) {
      printf("CORE: ERROR: Can't go from %d to %d threads during initialization.\n",num_readers,
          new_num_rw_threads);
      exit(1);
    } else {
      // Fixed size structure used, no problem.
    }
  };

  volatile uint32_t global_time_count,   // Every time a lock, or update occurs, this count is incremented. This allows us to easily tell
                                         // whether a read-side critical section is underway before we reclaim.
    last_update_time;                    // Time when the ptr this RT_RCU protects
  volatile uint32_t *reader_lock_times;  // Every reader thread has a value that says what was the global_time_count when it was locked, or 0
                                         // if that reader thread is unlocked.
  int num_readers;                       // Local copy of RT_RWThreads::num_rw_threads
};

#endif
