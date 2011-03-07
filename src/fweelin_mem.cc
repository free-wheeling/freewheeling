/*
   I surrender,
   and God comes singing.
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

#include "fweelin_mem.h"
#include "fweelin_datatypes.h"

// Room for this many instances created / deleted between manager thread passes
#define MEMMGR_UPDATE_QUEUE_SIZE 1000

MemoryManager::MemoryManager() : update_queue(0) {
  // Init mutex/conditions
  pthread_mutex_init(&mgr_thread_lock,0);
  pthread_cond_init(&mgr_go,0);

  // Create update queue
  update_queue = new SRMWRingBuffer<MemoryManagerUpdate>(MEMMGR_UPDATE_QUEUE_SIZE);

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
  SRMWRingBuffer_Writers::RegisterWriter(mgr_thread);

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

  delete update_queue;

  printf("MEM: End manager thread.\n");
};

void MemoryManager::WakeUp(MemoryManagerUpdate &upd) {
  if (update_queue->WriteElement(upd) != 0) {
    printf("MEM: ERROR: No space in memory manager update queue!\nMust increase MEMMGR_UPDATE_QUEUE_SIZE.\n");
    exit(1);
  }

  // Wake up the manager thread
  WakeupIfNeeded(1);
};

void MemoryManager::AddType(PreallocatedType *t) {
  pthread_mutex_lock (&mgr_thread_lock);
  pts.AddToHead(t);
  pthread_mutex_unlock (&mgr_thread_lock);
};

void MemoryManager::DelType(PreallocatedType *t) {
  pthread_mutex_lock (&mgr_thread_lock);
  if (pts.FindAndRemove(t) != 0)
    t->Cleanup();
  pthread_mutex_unlock (&mgr_thread_lock);
};

// Look through update queue and perform all updates needed
void MemoryManager::ProcessQueue() {
  // printf("MEM: start process queue\n");

  MemoryManagerUpdate mmu = 0;
  do {
    // printf("MEM: ...Processing queue...\n");
    mmu = update_queue->ReadElement();
    if (mmu.IsValid()) {
      // printf("MEM: Valid item: updatetype: %d typeptr: %p restockidx: %d freeptr: %p\n",mmu.update_type,mmu.which_pt,mmu.updatedat.update_idx,mmu.updatedat.tofree);

      if (mmu.update_type == T_MU_FreeInstance) {
        // Free item in free list *take out trash*
        mmu.which_pt->GoPostdelete(mmu.updatedat.tofree);
      } else if (mmu.update_type == T_MU_RestockInstance) {
        // Allocate new item for ready list *restock*
        mmu.which_pt->GoPreallocate(mmu.updatedat.update_idx);
      } else {
        printf("MEM: ERROR: Invalid update type\n");
        exit(1);
      }
    }
    //else
    //  printf("MEM: No more items received from queue.\n");
  } while (mmu.IsValid());

  // printf("MEM: end process queue\n");
};

void *MemoryManager::run_mgr_thread (void *ptr) {
  MemoryManager *inst = static_cast<MemoryManager *>(ptr);

  // printf("*** THREAD: %p\n",pthread_self());

  pthread_mutex_lock(&inst->mgr_thread_lock);
  while (inst->threadgo) {
    inst->ProcessQueue();
    
    // Wait for wakeup
    pthread_cond_wait (&inst->mgr_go, &inst->mgr_thread_lock);

    inst->needs_wakeup = 0; // Woken!
  }

  printf("MEM: Begin cleanup.\n"); 
  inst->ProcessQueue();

  PreallocatedType *cur = (PreallocatedType *) inst->pts.GetFirstItem();
  while (cur != 0) {
    cur->Cleanup();
    cur = (PreallocatedType *) inst->pts.GetNextItem(cur);
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
  prealloc_base(prealloc_base), instance_size(instance_size),
  prealloc_num_instances(prealloc_num_instances), block_mode(block_mode), blocks_list(0),
  mmgr(mmgr) {
  // Set up ready and free lists
  ready_list = new RTStore<PreallocatedInstance>(prealloc_num_instances);

  // Setup base instance
  prealloc_base->
    SetupPreallocated(this,Preallocated::PREALLOC_BASE_INSTANCE);

  if (block_mode) {
    // Setup one base block of instances
    if (prealloc_num_instances < 3) {
      printf("MEM: ERROR- Types using block mode must have at least 3 instances.");
      exit(1);
    }

    // Mark first instance in block as base
    Preallocated *cur = prealloc_base;

    // Store the first block of instances
    blocks_list = prealloc_base;

    /* printf("MEM: NEW BLOCK (STARTUP): %p, size: %d\n",cur,
       instance_size); */

    // The rest are going right into the 'ready list'.. mark them and set them up
    cur = (Preallocated *) (((char *) cur) + instance_size);
    for (int i = 1; i < prealloc_num_instances; i++,
           cur = (Preallocated *) (((char *) cur) + instance_size)) {
      cur->SetupPreallocated(this,Preallocated::PREALLOC_IN_USE);
      if (i == 1) // Second instance contains # of free instances
        cur->predata.prealloc_num_free = 0; // None are free. We have the base instance and then the
                                            // rest go straight to the ready list.

      // Add to ready list...
      PreallocatedInstance *inst = 0;
      int inst_idx; // Index in ready list
      if ((inst = ready_list->FindItemWithState(RTStore<PreallocatedInstance>::ITEM_DONE,
          RTStore<PreallocatedInstance>::ITEM_BUSY,inst_idx)) == 0) {
        printf("MEM: ERROR: Ready_list size mismatch.\n");
        exit(1);
      } else
        inst->ptr = cur;
      if (!ready_list->ChangeStateAtIdx(inst_idx,RTStore<PreallocatedInstance>::ITEM_BUSY,
          RTStore<PreallocatedInstance>::ITEM_WAITING)) {
        printf("MEM: ERROR: State mismatch in ready_list\n");
        exit(1);
      }
    }
  } else {
    // Instance mode

    // Create full ready list
    PreallocatedInstance *i = 0;
    do {
      int idx;
      if ((i = ready_list->FindItemWithState(RTStore<PreallocatedInstance>::ITEM_DONE,
          RTStore<PreallocatedInstance>::ITEM_BUSY,idx)) != 0) {
        // Prepare an instance and store in the list
        i->ptr = prealloc_base->NewInstance();
        i->ptr->SetupPreallocated(this,Preallocated::PREALLOC_IN_USE);

        // Update state to waiting (for consumption)
        if (!ready_list->ChangeStateAtIdx(idx,RTStore<PreallocatedInstance>::ITEM_BUSY,
            RTStore<PreallocatedInstance>::ITEM_WAITING)) {
          printf("MEM: ERROR: State mismatch in ready_list\n");
          exit(1);
        }
      }
    } while (i != 0);
  }

  // Add ourselves to list of managed types
  mmgr->AddType(this);
};

PreallocatedType::~PreallocatedType() {
  // Remove ourselves from list of managed types
  mmgr->DelType(this);

  delete ready_list;
}; 

// Realtime and thread-safe function to get a new instance of this class
Preallocated *PreallocatedType::RTNew() {
  // Simply scan through the ready list and get an item marked 'waiting' (for consumption)
  int idx;
  PreallocatedInstance *i = ready_list->FindItemWithState(RTStore<PreallocatedInstance>::ITEM_WAITING,
      RTStore<PreallocatedInstance>::ITEM_BUSY,idx);
  if (i != 0) {
    // Now, grab the pointer
    Preallocated *ptr = i->ptr;

    // And mark it as 'done' (in use)
    if (!ready_list->ChangeStateAtIdx(idx,RTStore<PreallocatedInstance>::ITEM_BUSY,
        RTStore<PreallocatedInstance>::ITEM_DONE)) {
      printf("MEM: ERROR: State mismatch in ready_list\n");
      exit(1);
    }

    // Wake up manager thread to replace the consumed instance
    MemoryManagerUpdate mmu(this,T_MU_RestockInstance,idx,0);
    mmgr->WakeUp(mmu);

    return ptr;
  } else {
    // No instances ready for consumption
    printf("\nMEM: RTNew- No instances available.\n");
    // mmgr->WakeupIfNeeded(1);
    return 0;
  }

#if 0
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
#endif
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

// Realtime and thread-safe function to delete this instance of this class
void PreallocatedType::RTDelete(Preallocated *inst) {
  // Wake up manager thread to free the instance
  MemoryManagerUpdate mmu(this,T_MU_FreeInstance,0,inst);
  mmgr->WakeUp(mmu);

  #if 0
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
#endif
};

#if 0
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

      // Insert at beginning of list
      //
      // *** Note that this will move the base instance so it is 
      // *** no longer at the beginning of the in use list
      // *** For this reason, a separate 'prealloc_base' pointer has been
      // *** created
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
        // Insert at beginning of list
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
#endif

void PreallocatedType::GoPostdelete(Preallocated *tofree) {
  // *** NOTE: Currently no support for deleting additional blocks
  // of instances- so if we are bursting to many instances
  // the memory overhead will remain ***
  //
  // This should not use significant memory because, in Freewheeling, the large data structures
  // such as AudioBlock are allocated in instance mode, not block mode.

  if (block_mode) {
    // Recycle the instance (instead of deleting)
    tofree->Recycle();

    // Mark as free
    tofree->prealloc_status = Preallocated::PREALLOC_FREE;

    // Check pointer range to see which block this instance is in
    Preallocated *cur = blocks_list;
    int blocksize = instance_size*prealloc_num_instances;
    while (cur != 0 && (tofree < cur || (char *) tofree >= ((char *) cur) + blocksize))
      cur = cur->predata.prealloc_next;
    if (cur == 0) {
      printf("MEM: ERROR: Postdelete: Instance not found in any blocks\n");
      exit(1);
    } else {
      // Add to free count
      Preallocated *second = (Preallocated *) (((char *) cur) + instance_size);
      second->predata.prealloc_num_free++;
    }
  } else {
    // Single instance, simply delete it - no internal data structures to modify
    ::delete tofree;
  }
};

void PreallocatedType::GoPreallocate(int ready_list_idx) {
  Preallocated *nw = 0;

  if (block_mode) {
    // In block mode, we get a new index from our blocks list.
    // Find a block with a free instance.
    Preallocated *curblk = blocks_list;
    char found = 0;
    while (!found && curblk != 0) {
      // Second instance in block contains # of free instances
      Preallocated *second = (Preallocated *) (((char *) curblk) +
                                               instance_size);
      if (second->predata.prealloc_num_free > 0) {
        // This block has a free instance. Use it.
        found = 1;
      } else
        curblk = curblk->predata.prealloc_next;
    }

    if (found) {
      // Find a free instance in this block
      Preallocated *curinst = curblk;
      int i = 0;
      for (; i < prealloc_num_instances && curinst->prealloc_status != Preallocated::PREALLOC_FREE;
          i++, curinst = (Preallocated *) (((char *) curinst) + instance_size));
      if (i >= prealloc_num_instances) {
        // Failed, even though this block says it has free instances
        printf("MEM: ERROR: Block marked with free instances but has none.\n");
        exit(1);
      } else {
        // Use this instance
        nw = curinst;
        nw->prealloc_status = Preallocated::PREALLOC_IN_USE;

        // Reduce free count
        Preallocated *second = (Preallocated *) (((char *) curblk) +
                                                 instance_size);
        second->predata.prealloc_num_free--;
      }
    } else {
      // No blocks found with free instances. We need a new block
      nw = prealloc_base->NewInstance();

      // Setup new block
      Preallocated *cur = nw;
      for (int i = 0; i < prealloc_num_instances; i++,
             cur = (Preallocated *) (((char *) cur) + instance_size)) {
        if (i == 0)
          cur->SetupPreallocated(this,Preallocated::PREALLOC_IN_USE); // Use first instance
        else
          cur->SetupPreallocated(this,Preallocated::PREALLOC_FREE);

        if (i == 1) // Second instance contains # of free instances
          // All but 1 (which we'll use) will be free
          cur->predata.prealloc_num_free = prealloc_num_instances-1;
      }

      // Link it in
      nw->predata.prealloc_next = blocks_list;
      blocks_list = nw;
    }
  } else {
    // Single instance mode. Easy. Since one instance was consumed, we must allocate one more and
    // put it in the ready list.
    nw = prealloc_base->NewInstance();
    nw->SetupPreallocated(this,Preallocated::PREALLOC_IN_USE);
  }

  // Now place this instance in the ready list
  if (nw != 0) {
    if (!ready_list->ChangeStateAtIdx(ready_list_idx,RTStore<PreallocatedInstance>::ITEM_DONE,
        RTStore<PreallocatedInstance>::ITEM_BUSY)) {
      printf("MEM: ERROR: State mismatch in ready_list\n");
      exit(1);
    }

    ready_list->GetItemAtIdx(ready_list_idx)->ptr = nw;

    // Mark as waiting-for-consumption
    if (!ready_list->ChangeStateAtIdx(ready_list_idx,RTStore<PreallocatedInstance>::ITEM_BUSY,
        RTStore<PreallocatedInstance>::ITEM_WAITING)) {
      printf("MEM: ERROR: State mismatch in ready_list\n");
      exit(1);
    }
  } else {
    printf("MEM: ERROR: Can't allocate more instances.\n");
    exit(1);
  }
};

void PreallocatedType::Cleanup() {
  // Ok, we've been told to stop
  // So we have to delete all preallocated instances/blocks!

  if (block_mode) {
    // Delete all blocks from blocks list
    Preallocated *curblk = blocks_list;
    while (curblk != 0) {
      Preallocated *curinst = curblk;

      int numfree = 0, numfree_verify = 0;
      for (int i = 0; i < prealloc_num_instances; i++,
        curinst = (Preallocated *) (((char *) curinst) + instance_size)) {
        if (i == 1)
          numfree = curinst->predata.prealloc_num_free;

        // The check below does not work because instances marked 'in use' could still be in the ready list
        // if (curinst->prealloc_status == Preallocated::PREALLOC_IN_USE)
          // printf("MEM: WARNING: Freeing block with instance #%d marked in use.\n",i);

        if (curinst->prealloc_status == Preallocated::PREALLOC_FREE)
          numfree_verify++;
      }

      if (numfree != numfree_verify)
        printf("MEM: WARNING: Number of free blocks listed and actual number of free blocks don't match.\n");

      Preallocated *tmp = curblk->predata.prealloc_next;
      curblk->DelBlock();
      curblk = tmp;
    }

    blocks_list = 0;
  } else {
    // No internal data structures to free
  }
};
