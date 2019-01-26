#ifndef __FWEELIN_MEM_H
#define __FWEELIN_MEM_H

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

class MemoryManager;
class Preallocated;

#include "fweelin_datatypes.h"

/*
 * This source implements memory manager classes that offer very fast, lightweight, realtime and
 * thread-safe allocation and deallocation of arbitrary class instances. Using this approach, instances
 * are preallocated and postdeleted and stored in lock-free real-time data structures.
 *
 * This allows real-time and time critical threads to get new instances on demand, and to free them without
 * pausing. The actual memory allocation and deletion are managed in a manager thread.
 */

// RTStore uses classes, so here's a single preallocated instance:
class PreallocatedInstance {
public:
  PreallocatedInstance() : ptr(0) {};
  Preallocated *ptr;
};

class PreallocatedType : public SListItem {
  friend class MemoryManager;

 public:
  // Default number of instances to keep preallocated
  const static int PREALLOC_DEFAULT_NUM_INSTANCES = 10;

  // Tells memory manager to start preallocating blocks of this type
  // Note for each type of preallocated data we can specify
  // a different number of instances to keep ready for RT consumption
  //
  // We must provide one instance of the Preallocated class as a reference
  // This is prealloc_base
  //
  // If block_mode is 1, a continuous array of instances is used, and unused instances
  // are recycled instead of new ones constructed. Pass an initial BLOCK of prealloc_num_instances
  // instances as 'prealloc_base'.
  PreallocatedType(MemoryManager *mmgr, Preallocated *prealloc_base,
                   int instance_size,
                   int prealloc_num_instances = 
                   PREALLOC_DEFAULT_NUM_INSTANCES,
                   char block_mode = 0);
  // Stops preallocating this type
  ~PreallocatedType();

  // Realtime-safe function to get a new instance of this class
  Preallocated *RTNew();
  // RTNewWithWait() returns a new instance-- whereas RTNew returns 0
  // if none is available, RTNewWithWait -waits- until one becomes available
  // -- not realtime safe --
  Preallocated *RTNewWithWait();
  // Realtime-safe function to delete this instance of this class
  void RTDelete(Preallocated *inst);
  
  // Perform any preallocations and pending deletes that are needed!
  // Called by memory manager

  void GoPreallocate(int ready_list_idx);   // Allocate/assign an instance into the ready-to-consume list
  void GoPostdelete(Preallocated *tofree);  // Free/unassign an instance

  // Cleanup- delete all preallocated instances of this type- called
  // on program exit
  void Cleanup();

  inline int GetBlockSize() { return prealloc_num_instances; };
  
 private:
  
  Preallocated *prealloc_base;                // Base instance from which others are spawned

  RTStore<PreallocatedInstance> *ready_list;  // List of all instances ready (preallocated)

  // Actual size of one instance (we can't get it with RTTI, so you have to
  // pass it!)
  int instance_size;

  // Number of instances to keep preallocated
  int prealloc_num_instances;

  // *** BLOCK MODE ***
  // Block mode or single instance mode?
  char block_mode;
  // List of blocks (block mode only)
  Preallocated *blocks_list;
  // Last block index scanned for free instances
  int block_last_idx;
  // ***

  MemoryManager *mmgr;    // Memory manager
};

// This class is a base for classes that want to be preallocated
// and postdeleted-- classes in which new instances are needed
// in realtime-- the memory allocation is done in a nonrealtime thread
//
// One such allocate&delete thread exists for all preallocated types
// MemoryManager handles that thread. 
//
// Using this class as a base adds some bytes of size to each instance
// which might be a concern if you are allocating many instances!
class Preallocated {
  friend class PreallocatedType;

 public:
  // Default constructor calls recycle to init this instance
  Preallocated() : prealloc_mgr(0) {};
  virtual ~Preallocated() {};

  void *operator new(size_t) {
    printf("ERROR: Preallocated type can not be allocated directly\n");
    exit(1);
  };
  void operator delete(void*) {
    // cannot pass to RTDelete as this would end with two delete executed
    // and destructor called twice
    printf("ERROR: Preallocated type can not be deleted directly\n");
    exit(1);
  }

  // Realtime-safe function to get a new instance of this class
  Preallocated *RTNew() { 
    if (prealloc_mgr == 0) { // No mgr, so allocate nonRT way!
      //printf("WARNING: nonRT Prealloc in RTNew\n");
      return NewInstance();
    }
    else 
      return prealloc_mgr->RTNew();
  };

  Preallocated *RTNewWithWait() {
    if (prealloc_mgr == 0) { // No mgr, so allocate nonRT way!
      //printf("WARNING: nonRT Prealloc in RTNew\n");
      return NewInstance();
    }
    else 
      return prealloc_mgr->RTNewWithWait();
  };


  // Realtime-safe function to delete this instance of this class
  void RTDelete() {
    if (prealloc_mgr == 0) { // No mgr, nonRT delete!
      // printf("WARNING: nonRT delete in RTDelete: %p\n",this);
      ::delete this;
    }
    else 
      prealloc_mgr->RTDelete(this); 
  };

  // Returns the PreallocatedType manager associated with this type
  inline PreallocatedType *GetMgr() { return prealloc_mgr; };

  // Values for status of preallocated instances
  const static char PREALLOC_BASE_INSTANCE = 0, // Base instance, never deleted
    PREALLOC_IN_USE = 1,                        // In use means 'in the ready list, or already consumed
                                                // for use'
    PREALLOC_FREE = 2;                          // Free is only applicable for block mode, and means
                                                // 'ready to be added to the ready list- not available for
                                                // consumption yet'

  // This setup function is called from PreallocatedType
  // after a new instance of Preallocated is created.
  // It sets up the internal variables in this instance.
  void SetupPreallocated(PreallocatedType *mgr,
                         char status) {
    prealloc_mgr = mgr;
    prealloc_status = status;
    predata.prealloc_next = 0;
  };

 private:

  // Code to destroy a block of instances- only called in block mode
  // This is a workaround because operator delete[] is not virtual
  // and can not handle our arrays of derived instances
  //
  // Any derived classes that use block mode must re-implement this method (ie use the macro below).
#define FWMEM_DEFINE_DELBLOCK  virtual void DelBlock() { ::delete[] this; };
  FWMEM_DEFINE_DELBLOCK;
  
  // Code to call new operator for the derived class (nonRT)
  // All allocation and lengthy initialization can be done here
  // If this type is allocated in block mode, NewInstance must allocate
  // not one instance, but an array of instances with prealloc_num_instances
  // size
  virtual Preallocated *NewInstance() = 0;
  
  // For managed types in block mode only:
  //
  // This method should recycle an old instance to be used again.
  // It is used instead of the constructor/destructor.
  //
  // In block mode, the default constructor is called by NewInstance (which you implement)
  // ONCE when allocating a block of instances. When an instance is freed, it is not destructed
  // but simply Recycled.
  // 
  // Initialization code can go here.
  //
  // This method is called from the memory manager thread, so it needn't be RT safe.
  virtual void Recycle() {};

  // Status of this instance
  char prealloc_status;

  // PreallocatedType that manages the allocation of new instances
  // ** With a table, this could be optimized out of each instance **
  PreallocatedType *prealloc_mgr;

  union {
    // In block mode,
    // Each instance is part of an array. And each array is part of a linked
    // list. So we have a list of blocks of instances. 
    // The 1st instance in a block points to the next block.
    // The 2nd instance in a block holds the # of free instances in that block:
    int prealloc_num_free;

    // Next instance- or in block mode, next block of instances
    Preallocated *prealloc_next;
  } predata;
};

enum MemoryManagerUpdateType {
  T_MU_RestockInstance,
  T_MU_FreeInstance,
};

/**
 * This class communicates a single change from RTNew() / RTDelete() to nonRT manager thread.
 * The manager thread must either "re-stock the shelves" in the ready list, or
 * "Take out the trash" by freeing an instance.
 */
class MemoryManagerUpdate {
public:
  // Zero (invalid) update
  MemoryManagerUpdate(int zero = 0) : which_pt(0) { if (zero) { /*zero is for what now?*/ } };

  // Valid update
  MemoryManagerUpdate(PreallocatedType *which_pt, MemoryManagerUpdateType update_type,
      int update_idx, Preallocated *tofree) : which_pt(which_pt), update_type(update_type) {
    if (update_type == T_MU_RestockInstance)
      updatedat.update_idx = update_idx;
    else if (update_type == T_MU_FreeInstance)
      updatedat.tofree = tofree;
  };

  inline char IsValid() { return which_pt != 0; };

  PreallocatedType *which_pt;           // Which preallocated type needs updating
  MemoryManagerUpdateType update_type;  // Is it a change to the ready list or an item to be freed
  union {
    int update_idx;                     // If it's an item to restock, which index to update
    Preallocated *tofree;               // If it's an instance to be freed, pointer to the instance
  } updatedat;
};

class MemoryManager {
 public:

  MemoryManager();
  ~MemoryManager();
  
  // Starts managing the specified type
  void AddType(PreallocatedType *t);
  // Stops managing the specified type
  void DelType(PreallocatedType *t);

  // Wake up the manager thread-
  // queue the update-
  // either we need to re-stock the shelves or take out the trash
  void WakeUp(MemoryManagerUpdate &upd);

  // Wakeup the memory manager thread. Non blocking, RT safe.
  inline void WakeupIfNeeded(char always_wakeup = 0) {
    if (always_wakeup || needs_wakeup) {
      /* if (!always_wakeup)
        printf("MEM: Woken because of priority inversion\n"); */

      // Wake up the memory manager thread
      if (pthread_mutex_trylock (&mgr_thread_lock) == 0) {
        pthread_cond_signal (&mgr_go);
        pthread_mutex_unlock (&mgr_thread_lock);
      } else {
        // Priority inversion - we are interrupting the memory manager thread while it's processing
        // update_queue. This is not an issue, because update_queue uses SRMWRingBuffer.
        // However, the memory manager thread may go to sleep, missing the new updates
        // until it's woken again. So, set a flag and the RT audio
        // thread will wake it up next process cycle.

        if (always_wakeup) {
          // printf("MEM: WARNING: Priority inversion while filling update queue!\n");
          needs_wakeup = 1;
        }
      }
    }
  };

 private:

  // Look through update queue and perform all updates needed
  void ProcessQueue();

  // Thread function
  static void *run_mgr_thread (void *ptr);

  // List of all PreallocatedTypes we are managing
  SLinkList pts;

  // Queue of all changes needed to ready_lists and items to be freed
  SRMWRingBuffer<MemoryManagerUpdate> *update_queue;

  volatile char needs_wakeup; // Memory manager thread needs wakeup? (priority inversion)

  // Thread to preallocate and postdelete instances
  pthread_t mgr_thread;
  pthread_mutex_t mgr_thread_lock;
  pthread_cond_t mgr_go;
  int threadgo;
};

#endif
