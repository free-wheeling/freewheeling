/* 
   I surrender,
   and God comes singing.
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

#include "fweelin_mem.h"

MemoryManager::MemoryManager() : pts(0), apts(0) {
  // Init mutex/conditions
  pthread_mutex_init(&mgr_thread_lock,0);
  pthread_cond_init(&mgr_go,0);

  const static size_t STACKSIZE = 1024*128;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("MEMMGR: Stacksize: %d.\n",STACKSIZE);

  // Setup manager thread
  threadgo = 1;
  int ret = pthread_create(&mgr_thread,
			   &attr,
			   run_mgr_thread,
			   static_cast<void *>(this));
  if (ret != 0) {
    printf("MEM: (memorymanager) pthread_create failed, exiting");
    exit(1);
  }

  // Setup high priority threads
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
  printf("MEM: Memory manager thread (p%d)\n",schp.sched_priority);
  if (pthread_setschedparam(mgr_thread, SCHED_OTHER, &schp) != 0) {    
    printf("MEM: Can't set thread priority, will use regular!\n");
  }
};

MemoryManager::~MemoryManager() {
  // Terminate the manager thread
  threadgo = 0;
  pthread_mutex_lock (&mgr_thread_lock);
  pthread_cond_signal (&mgr_go);
  pthread_mutex_unlock (&mgr_thread_lock);
  pthread_join(mgr_thread,0);

  pthread_cond_destroy (&mgr_go);
  pthread_mutex_destroy (&mgr_thread_lock);

  printf("MEM: End manager thread.\n");
};

void MemoryManager::WakeUp(PreallocatedType *t) {
  // First, add the specified type to the list of active types
  if (apts == 0) {
    t->anext = 0;
    apts = t;
  }
  else {
    // Only add to active type list if it isn't already there!
    // This should be low overhead since the active types list will usually
    // be small
    PreallocatedType *cur = apts;
    while (cur->anext != 0 && cur != t)
      cur = cur->anext;
    if (cur != t) {
      t->anext = 0;
      cur->anext = t;
    }
  }

  // Wake up the manager thread
  if (pthread_mutex_trylock (&mgr_thread_lock) == 0) {
    pthread_cond_signal (&mgr_go);
    pthread_mutex_unlock (&mgr_thread_lock);
  }

  // *** Redesign active types list to be circular ring buffer--
  // this would avoid potential loss of update on race condition to reset/read
  // apts

  /* else
     printf("MEM: Warning: Mgr thread active during mutex trylock.\n"); */
};

void MemoryManager::AddType(PreallocatedType *t) {
  pthread_mutex_lock (&mgr_thread_lock);
  if (pts == 0) 
    pts = t;
  else {
    t->next = pts;
    pts = t;
  }
  pthread_mutex_unlock (&mgr_thread_lock);
};

void MemoryManager::DelType(PreallocatedType *t) {
  pthread_mutex_lock (&mgr_thread_lock);

  if (apts != 0)
    printf("MEM: Warning: Active pre types not zero during DelType\n");

  PreallocatedType *cur = pts,
    *prev = 0;
  
  // Search for type 't' in our list
  while (cur != 0 && cur != t) {
    prev = cur;
    cur = cur->next;
  }

  if (cur != 0) {
    // Got one to delete, unlink!
    if (prev != 0)
      prev->next = cur->next;
    else
      pts = cur->next;

    // Look in the active list
    {
      PreallocatedType *cur = apts;
      while (cur != 0 && cur != t)
	cur = cur->anext;
      if (cur != 0) {
	printf("MEM: ERROR: Deleted type found in active list!\n");
	exit(1);
      }
    }

    // Necessary?
    cur->Cleanup();
  }

  if (apts != 0)
    printf("MEM: Warning: Active pre types not zero during DelType\n");

  pthread_mutex_unlock (&mgr_thread_lock);
};

void *MemoryManager::run_mgr_thread (void *ptr) {
  MemoryManager *inst = static_cast<MemoryManager *>(ptr);

  printf("*** THREAD: %p\n",pthread_self());
  
  pthread_mutex_lock(&inst->mgr_thread_lock);
  while (inst->threadgo) {
    // Go through all PreallocatedTypes in our active list
    PreallocatedType *cur = inst->apts;
    while (cur != 0) {
      if (cur->GetUpdateLists())
	// Update free/inuse lists for this type
	cur->UpdateLists();

      // Make sure there are enough free instances of this type
      cur->GoPreallocate();

      // Next, look for instances pending delete!
      cur->GoPostdelete();

      cur = cur->anext;
    }
    
    // Clear active type list until we are woken up again
    inst->apts = 0;

    // Wait for wakeup
    pthread_cond_wait (&inst->mgr_go, &inst->mgr_thread_lock);
  }

  printf("MEM: Begin cleanup.\n"); 
  PreallocatedType *cur = inst->pts;
  while (cur != 0) {
    cur->Cleanup();
    cur = cur->next;
  }
  printf("MEM: End cleanup.\n");

  pthread_mutex_unlock(&inst->mgr_thread_lock);

  return 0;
}

PreallocatedType::PreallocatedType(MemoryManager *mmgr,
				   Preallocated *prealloc_base,
				   int instance_size,
				   int prealloc_num_instances,
				   char block_mode) : 
  update_lists(0), instance_size(instance_size), 
  prealloc_num_instances(prealloc_num_instances), block_mode(block_mode),
  block_last_idx(0), mmgr(mmgr), next(0), anext(0) {
  if (block_mode) {
    // Setup base block of instances
    if (prealloc_num_instances < 3) {
      printf("MEM: ERROR- Block mode must have at least 3 instances.");
      exit(1);
    }

    // Mark first instance in block as base
    Preallocated *cur = prealloc_base;
    cur->SetupPreallocated(this,Preallocated::PREALLOC_BASE_INSTANCE);

    /* printf("MEM: NEW BLOCK (STARTUP): %p, size: %d\n",cur,
       instance_size); */

    // Rest are free
    cur = (Preallocated *) (((char *) cur) + instance_size);
    for (int i = 1; i < prealloc_num_instances; i++, 
	   cur = (Preallocated *) (((char *) cur) + instance_size)) {
      cur->SetupPreallocated(this,
			     Preallocated::PREALLOC_PENDING_USE);
      if (i == 1) // Second instance contains # of free instances
	cur->predata.prealloc_num_free = prealloc_num_instances-1;
    }

    // Put base block in free (doesn't go in use until all are used)
    prealloc_free = prealloc_base;
    prealloc_inuse = 0;
  } else {
    // Setup the base instance
    prealloc_base->
      SetupPreallocated(this,Preallocated::PREALLOC_BASE_INSTANCE);

    // Put base instance in use
    prealloc_free = 0;
    prealloc_inuse = prealloc_base;
  }

  // Make sure there are enough free instances 
  GoPreallocate();

  // Add ourselves to list of managed types
  mmgr->AddType(this);
};

PreallocatedType::~PreallocatedType() {
  // Remove ourselves from list of managed types
  mmgr->DelType(this);
}; 

// Realtime-safe function to get a new instance of this class
Preallocated *PreallocatedType::RTNew() {
  Preallocated *nw = 0;
  if (block_mode) {
    Preallocated *cur = prealloc_free;
    char gogo = 1;
    while (gogo && cur != 0) {
      char go = 1;
      int idx = block_last_idx;
      nw = (Preallocated *) (((char *) cur) + idx*instance_size);
      while (go && nw->prealloc_status != Preallocated::PREALLOC_PENDING_USE) {
	idx++;
	nw = (Preallocated *) (((char *) nw) + instance_size);
	
	if (idx >= prealloc_num_instances) {
	  idx = 0;
	  nw = cur;
	}
	
	if (idx == block_last_idx) 
	  // We've checked the whole block, none free
	  go = 0;
      }
      
      if (go) {
	// Found a free instance in this block, use it
	nw->prealloc_status = Preallocated::PREALLOC_IN_USE;    

	block_last_idx = idx+1;
	if (block_last_idx >= prealloc_num_instances)
	  block_last_idx = 0;

	Preallocated *second = (Preallocated *) 
	  (((char *) cur) + instance_size);
	second->predata.prealloc_num_free--;

	/* printf("MEM: RTNew (block %p, instance %p)- Num free: %d\n",cur,
 	   nw,second->predata.prealloc_num_free); */
	
	// Less than half the block is free?
	if (second->predata.prealloc_num_free < prealloc_num_instances/2) {
	  if (cur->predata.prealloc_next == 0) {
	    // No other free blocks, so wakeup to allocate more
	    // printf("MEM: RTNew (block)- Wakeup!\n");
	    mmgr->WakeUp(this);
	  }

	  if (second->predata.prealloc_num_free <= 0) {
	    // No more free instances in this block-- 
	    // Update lists
	    update_lists = 1;
	  }
	}

	// Stop searching
	gogo = 0;
      } else {
	// No free instance in this block-
	// This can happen if memmgr hasn't yet updated its lists
	nw = 0;
	// printf("MEM: RTNew (block)- WARNING: Full block in free list.\n");

	// Update lists
	update_lists = 1;

	// Search through next free block
	cur = cur->predata.prealloc_next;
      }
    }

    if (cur == 0) {
      // No free instances!
      printf("\nMEM: RTNew (block)- No instances available.\n");
      mmgr->WakeUp(this);
      return 0; // Problem! No free instances!
    }    
  } else {
    // Get the first free instance
    nw = prealloc_free;
    while (nw != 0 && 
	   nw->prealloc_status != Preallocated::PREALLOC_PENDING_USE)
      nw = nw->predata.prealloc_next;

    if (nw == 0) {
      printf("\nMEM: RTNew- No instances available:\n");
      
      // DEBUG- Print status for all instances
      Preallocated *p = prealloc_free;
      printf("FREE LIST:\n");
      while (p != 0) {
	printf("STATUS: %d\n",p->prealloc_status);
	p = p->predata.prealloc_next;
      }

      p = prealloc_inuse;
      printf("IN USE LIST:\n");
      while (p != 0) {
	printf("STATUS: %d\n",p->prealloc_status);
	p = p->predata.prealloc_next;
      }
      
      mmgr->WakeUp(this);
      
      return 0; // Problem! No free instances!
    }
    nw->prealloc_status = Preallocated::PREALLOC_IN_USE;

    // Update lists
    update_lists = 1;

    // Wake up the memory manager to allocate new
    mmgr->WakeUp(this);
  }

  return nw;
};

Preallocated *PreallocatedType::RTNewWithWait() {
  Preallocated *ret = 0;
  do {
    ret = RTNew();
    if (ret == 0) {
      printf("MEM: Waiting for memory to be allocated.\n");	
      usleep(10000);
    }
  } while (ret == 0);

  return ret;
};

// Realtime-safe function to delete this instance of this class
// It might get slow if you have many blocks of instances
void PreallocatedType::RTDelete(Preallocated *inst) {
  if (block_mode) {
    // Recycle the instance
    inst->Recycle();

    // Mark as free
    inst->prealloc_status = Preallocated::PREALLOC_PENDING_USE;

    // Check pointer range to see which block this instance is in
    Preallocated *cur = prealloc_free;
    int blocksize = instance_size*prealloc_num_instances;
    while (cur != 0 && (inst < cur || (char *) inst >= ((char *) cur) + 
			blocksize))
      cur = cur->predata.prealloc_next;
    if (cur == 0) {
      cur = prealloc_inuse;
      while (cur != 0 && (inst < cur || (char *) inst >= ((char *) cur) + 
			  blocksize))
	cur = cur->predata.prealloc_next;
    }

    if (cur == 0) {
      printf("MEM: RTDelete (block): WARNING: Instance %p not found "
	     "in any block!\n",inst);
      return;
    }

    //printf("MEM: RTDelete (block %p, instance %p)\n",cur,inst);

    // Add to free count
    Preallocated *second = (Preallocated *) (((char *) cur) + instance_size);
    second->predata.prealloc_num_free++;

    // If we are going from having no free instances to having 1 free instance
    // then we probably need to move this block to free list
    if (second->predata.prealloc_num_free == 1) {
      // Update lists
      update_lists = 1;
    }
  } else {
    // Mark the instance pending delete!-- the rest happens in the thread
    inst->prealloc_status = Preallocated::PREALLOC_PENDING_DELETE;
    //printf("RTdel\n");
    
    // Wake up the memory manager to delete the instance
    mmgr->WakeUp(this);
  }
};

void PreallocatedType::UpdateLists() {
  // Flag that we have updated our lists
  update_lists = 0;

  // printf("MEM: UPDATE LISTS!\n");

  // First, check for instances/blocks to move from free to in use list
  Preallocated *cur = prealloc_free,
    *prev = 0;
  while (cur != 0) {
    char movecur = 0;
    if (block_mode) {
      Preallocated *second = (Preallocated *) (((char *) cur) + instance_size);
      if (second->predata.prealloc_num_free <= 0) {
	// printf("MEM: Move block %p (free->inuse)\n",cur);
	movecur = 1;
      }
    } else if (!block_mode && 
	       (cur->prealloc_status == Preallocated::PREALLOC_IN_USE ||
		cur->prealloc_status == 
		Preallocated::PREALLOC_PENDING_DELETE)) {
      // printf("MEM: Move instance %p (free->inuse)\n",cur);
      movecur = 1;
    }

    if (movecur) {
      // This block/instance needs to move to the in use list
      Preallocated *tmp = cur->predata.prealloc_next;
      cur->predata.prealloc_next = prealloc_inuse;
      prealloc_inuse = cur;
      if (prev != 0) 
	prev->predata.prealloc_next = tmp;
      else 
	prealloc_free = tmp;	
      cur = tmp;
    } else {
      prev = cur;
      cur = cur->predata.prealloc_next;
    }
  }

  // In block mode, we also need to check for blocks to move back
  // from the in use to the free list
  if (block_mode) {
    cur = prealloc_inuse;
    prev = 0;
    while (cur != 0) {
      Preallocated *second = (Preallocated *) (((char *) cur) + instance_size);
      if (second->predata.prealloc_num_free > 0) {
	// printf("MEM: Move block %p (inuse->free)\n",cur);

	// This block needs to move to the free list
	Preallocated *tmp = cur->predata.prealloc_next;
	cur->predata.prealloc_next = prealloc_free;  
	prealloc_free = cur;
	if (prev != 0)
	  prev->predata.prealloc_next = tmp;
	else
	  prealloc_inuse = tmp;
	cur = tmp;
      } else {
	prev = cur;
	cur = cur->predata.prealloc_next;
      }
    }
  }
};

void PreallocatedType::GoPostdelete() {
  // *** NOTE: Currently no support for deleting additional blocks
  // of instances- so if we are bursting to many instances
  // the memory overhead will remain ***
  
  if (!block_mode) {
    // *** This could be optimized with a separate list for instances
    // *** pending delete!
    Preallocated *cur = prealloc_inuse,
      *prev = 0;
    while (cur != 0) {
      if (cur->prealloc_status == Preallocated::PREALLOC_PENDING_DELETE) {
	// Remove instance from list
	Preallocated *tmp = cur->predata.prealloc_next;
	if (prev != 0) 
	  prev->predata.prealloc_next = tmp;
	else 
	  prealloc_inuse = tmp;
	// printf("del ptr: %p sz: %d!!\n",cur,sizeof(*cur));
	::delete cur;
	cur = tmp;
      } else {
	// Next instance
	prev = cur;
	cur = cur->predata.prealloc_next;
      }
    }
  }
};

void PreallocatedType::GoPreallocate() {
  if (block_mode) {
    Preallocated *second = (Preallocated *) (((char *) prealloc_free) + 
					     instance_size);

    // Less than half the block is free?
    if (prealloc_free == 0 || 
	second->predata.prealloc_num_free < prealloc_num_instances/2) {
      // OK, allocate a new block
      // * NewInstance must create an array of the right size for block
      //   mode to work *
      Preallocated *nw_b;
      if (prealloc_inuse == 0)
	nw_b = prealloc_free->NewInstance();
      else
	nw_b = prealloc_inuse->NewInstance();

      // printf("MEM: NEW BLOCK: %p!\n",nw_b);

      // Setup new block
      Preallocated *cur = nw_b;
      for (int i = 0; i < prealloc_num_instances; i++, 
	     cur = (Preallocated *) (((char *) cur) + instance_size)) {
	cur->SetupPreallocated(this,Preallocated::PREALLOC_PENDING_USE);
	if (i == 1) // Second instance contains # of free instances
	  cur->predata.prealloc_num_free = prealloc_num_instances;
      }

      // Link it in
      nw_b->predata.prealloc_next = prealloc_free;
      prealloc_free = nw_b;
    }
  } else {
    // Make sure there are enough free instances 
    int cnt = prealloc_num_instances;
    Preallocated *cur = prealloc_free,
      *prev = 0;
    
    while (cnt>0 && cur!=0) {
      cnt--;
      prev = cur;
      cur = cur->predata.prealloc_next;
    }
    
    if (cnt>0) {
      // We need more free instances, so let's make em
      if (prev==0) {
	// First new block
	prealloc_free = prev = prealloc_inuse->NewInstance();
	prev->SetupPreallocated(this,Preallocated::PREALLOC_BASE_INSTANCE);
	//printf("firstnew\n");
	cnt--;
      }
      
      while (cnt>0) {
	// More new blocks
	//printf("new instance- typ: %p inuse: %p\n",this,prealloc_inuse);
	prev->predata.prealloc_next = cur = prealloc_inuse->NewInstance();
	cur->SetupPreallocated(this,Preallocated::PREALLOC_PENDING_USE);
	prev = cur;
	cnt--;
      }
    }
  }
};

void PreallocatedType::Cleanup() {
  // Ok, we've been told to stop
  // So we have to delete all preallocated instances/blocks!
  Preallocated *cur = prealloc_free;
  prealloc_free = 0;
  while (cur != 0) {
    // Remove instance from list
    Preallocated *tmp = cur->predata.prealloc_next;
    if (block_mode) {
      // printf("MEM: block delete from prealloc_free list: %p\n",cur);
      cur->DelBlock();
    }
    else 
      ::delete cur;
    cur = tmp;
  }
  cur = prealloc_inuse;
  prealloc_inuse = 0;
  while (cur != 0) {
    // Remove instance from list
    Preallocated *tmp = cur->predata.prealloc_next;
    if (block_mode) {
      // printf("MEM: block delete from prealloc_inuse list: %p\n",cur);
      cur->DelBlock();
    }
    else 
      ::delete cur;
    cur = tmp;
  }
};
