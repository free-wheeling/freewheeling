#ifndef __FWEELIN_MEM_H
#define __FWEELIN_MEM_H

class MemoryManager;
class Preallocated;

class PreallocatedType {
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
  // If block_mode is 1, an array of instances is used, and unused instances
  // are recycled instead of new ones constructed.
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
  void GoPreallocate();
  void GoPostdelete();

  // Update free and inuse lists so that free blocks and in use blocks
  // appear in the right lists- this can't be done in RT as it can break
  // the lists
  void UpdateLists();

  // Cleanup- delete all preallocated instances of this type- called
  // on program exit
  void Cleanup();

  inline int GetBlockSize() { return prealloc_num_instances; };
  inline char GetUpdateLists() { return update_lists; };
  
 private:
  
  char update_lists; // Flag- we need to update our free and inuse lists
                     // b/c one free block is now inuse / vice versa

  // Actual size of one instance (we can't get it with RTTI, so you have to
  // pass it!)
  int instance_size;

  // Number of instances to keep preallocated
  int prealloc_num_instances;

  // Block mode or single instance mode?
  char block_mode;
  // Last block index scanned for free instances
  int block_last_idx;

  // List of free instances
  Preallocated *prealloc_free; 
  // List of instances in use
  Preallocated *prealloc_inuse;
  // Base instance-- always available to use to create more instances
  Preallocated *prealloc_base;

  MemoryManager *mmgr;
  // MemoryManager maintains 2 lists of PreallocatedTypes
  PreallocatedType *next,
    *anext; // Next in active list
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

  void *operator new(size_t s) {
    printf("ERROR: Preallocated type can not be allocated directly\n");
    exit(1);
  };
  void operator delete(void *d) {
    //printf("ERROR: Preallocated type can not be deleted directly\n");
    //exit(1);

    // We used to give an error message-
    // now we pass this delete on to RTDelete
    ((Preallocated *) d)->RTDelete();
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
  PreallocatedType *GetMgr() { return prealloc_mgr; };

  // Status values
  const static char PREALLOC_BASE_INSTANCE = 0, // Base instance, never deleted
    PREALLOC_PENDING_USE = 1,
    PREALLOC_IN_USE = 2,
    PREALLOC_PENDING_DELETE = 3;

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
#define FWMEM_DEFINE_DELBLOCK  virtual void DelBlock() { ::delete[] this; };
  FWMEM_DEFINE_DELBLOCK;
  
  // Code to call new operator for the derived class (nonRT)
  // All allocation and lengthy initialization can be done here
  // If this type is allocated in block mode, NewInstance must allocate
  // not one instance, but an array of instances with prealloc_num_instances
  // size
  virtual Preallocated *NewInstance() = 0;
  
  // Code to recycle an old instance to be used again
  // This is called *only* in block mode when an instance is RTDeleted.
  // Instead of deleting and constructing instances, we recycle
  // instances and pass them as new instances.
  // 
  // So initialization code can go here, but this function
  // must be RT safe.
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

class MemoryManager {
 public:

  MemoryManager();
  ~MemoryManager();
  
  // Starts managing the specified type
  void AddType(PreallocatedType *t);
  // Stops managing the specified type
  void DelType(PreallocatedType *t);

  // Wake up the manager thread- something to be done on the specified type
  void WakeUp(PreallocatedType *t);

 private:

  // Thread function
  static void *run_mgr_thread (void *ptr);

  // List of all PreallocatedTypes we are managing
  PreallocatedType *pts;
  // List of all PreallocatedTypes with activity pending
  PreallocatedType *apts;

  // Thread to preallocate and postdelete instances
  pthread_t mgr_thread;
  pthread_mutex_t mgr_thread_lock;
  pthread_cond_t  mgr_go;
  int threadgo;
};

#endif
