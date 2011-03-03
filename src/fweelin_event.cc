/*
  They said we don't do kirtan during seva.
  Because singing is not mouna.
  but I differ--

  Rhythm
  is
  stillness in motion.

  And in the silence,
  there is music.
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

#include "fweelin_event.h"

#define EVENT_QUEUE_SIZE 100  // Number of events in event queue, per writer thread

EventTypeTable *Event::ett = 0;

// Events are allocated in blocks using the Memory Manager

// These macros help populate the event type table with names and managers
// They also determine which, if any, event parameters are indexed for speed
#define SET_ETYPE(etyp,nm,typ) \
  case etyp : \
    { \
      Event *proto = \
        ::new typ[PreallocatedType:: \
                  PREALLOC_DEFAULT_NUM_INSTANCES]; \
      ett[i].name = nm; \
      ett[i].mgr = \
        new PreallocatedType(mmgr,proto,sizeof(typ), \
                             PreallocatedType:: \
                             PREALLOC_DEFAULT_NUM_INSTANCES, \
                             1); \
      ett[i].slowdelivery = 0; \
      int paramidx = -1, j = 0; \
      for (; j < proto->GetNumParams() && \
           proto->GetParam(j).max_index == -1; j++); \
      if (j < proto->GetNumParams()) \
        paramidx = j; \
      ett[i].paramidx = paramidx; \
    } \
    break; 

#define SET_ETYPE_SLOW(etyp,nm,typ) \
  case etyp : \
    { \
      Event *proto = \
        ::new typ[PreallocatedType:: \
                  PREALLOC_DEFAULT_NUM_INSTANCES]; \
      ett[i].name = nm; \
      ett[i].mgr = \
        new PreallocatedType(mmgr,proto,sizeof(typ), \
                             PreallocatedType:: \
                             PREALLOC_DEFAULT_NUM_INSTANCES, \
                             1); \
      ett[i].slowdelivery = 1; \
      int paramidx = -1, j = 0; \
      for (; j < proto->GetNumParams() && \
           proto->GetParam(j).max_index == -1; j++); \
      if (j < proto->GetNumParams()) \
        paramidx = j; \
      ett[i].paramidx = paramidx; \
    } \
    break; 

#define SET_ETYPE_NO_BLOCK(etyp,nm,typ) \
  case etyp : \
    { \
      Event *proto = \
        ::new typ[PreallocatedType:: \
                PREALLOC_DEFAULT_NUM_INSTANCES]; \
      ett[i].name = nm; \
      ett[i].mgr = \
        new PreallocatedType(mmgr,proto,sizeof(typ), \
                             PreallocatedType:: \
                             PREALLOC_DEFAULT_NUM_INSTANCES, \
                             0); \
      int paramidx = -1, j = 0; \
      for (; j < proto->GetNumParams() && \
           proto->GetParam(j).max_index == -1; j++); \
      if (j < proto->GetNumParams()) \
        paramidx = j; \
      ett[i].paramidx = paramidx; \
    } \
    break; 

#define SET_ETYPE_NUMPREALLOC(etyp,nm,typ,numpre) \
  case etyp : \
    { \
      Event *proto = ::new typ[numpre]; \
      ett[i].name = nm; \
      ett[i].mgr = new PreallocatedType(mmgr,proto,sizeof(typ),numpre,1); \
      int paramidx = -1, j = 0; \
      for (; j < proto->GetNumParams() && \
           proto->GetParam(j).max_index == -1; j++); \
      if (j < proto->GetNumParams()) \
        paramidx = j; \
      ett[i].paramidx = paramidx; \
    } \
    break; 

void Event::SetupEventTypeTable(MemoryManager *mmgr) {
  int evnum = (int) EventType(T_EV_Last);
  ett = new EventTypeTable[evnum];
  for (int i = 0; i < evnum; i++) {
    switch (EventType(i)) {
      // *** Notice some events are marked SET_ETYPE_SLOW
      // This is critical because some code sections are not able to run in RT
      // Any event marked SET_ETYPE should be able to be run in RT,
      // because if it is bound to a MIDI trigger, that will happen.
      //
      // Events marked SET_ETYPE_SLOW will always run from the nonRT event
      // thread. This means they will be delivered after SET_ETYPE events if
      // a sequence of events is sent.

      SET_ETYPE(T_EV_Input_Key,"key",KeyInputEvent);
      SET_ETYPE(T_EV_Input_JoystickButton,"joybutton",
                JoystickButtonInputEvent);
      SET_ETYPE(T_EV_Input_MIDIKey,"midikey",MIDIKeyInputEvent);
      SET_ETYPE(T_EV_Input_MIDIController,"midicontroller",
                MIDIControllerInputEvent);
      SET_ETYPE(T_EV_Input_MIDIProgramChange,
                "midiprogramchange",
                MIDIProgramChangeInputEvent);
      SET_ETYPE(T_EV_Input_MIDIChannelPressure,
                "midichannelpressure",
                MIDIChannelPressureInputEvent);
      SET_ETYPE(T_EV_Input_MIDIPitchBend,"midipitchbend",
                MIDIPitchBendInputEvent);
      SET_ETYPE_NUMPREALLOC(T_EV_Input_MIDIClock,"midiclock",MIDIClockInputEvent,20);
      SET_ETYPE(T_EV_Input_MIDIStartStop,"midistartstop",MIDIStartStopInputEvent);

      SET_ETYPE_SLOW(T_EV_ALSAMixerControlSet,"alsa-mixer-control-set",ALSAMixerControlSetEvent);

      SET_ETYPE(T_EV_LoopClicked,"loop-clicked",LoopClickedEvent);

      SET_ETYPE(T_EV_GoSub,"go-sub",GoSubEvent);
      SET_ETYPE(T_EV_StartSession,"start-freewheeling",StartSessionEvent);
      SET_ETYPE(T_EV_StartInterface,"start-interface",StartInterfaceEvent);
      SET_ETYPE_SLOW(T_EV_ExitSession,"exit-freewheeling",ExitSessionEvent);

      SET_ETYPE(T_EV_SlideMasterInVolume,"slide-master-in-volume",
                SlideMasterInVolumeEvent);
      SET_ETYPE(T_EV_SlideMasterOutVolume,"slide-master-out-volume",
                SlideMasterOutVolumeEvent);
      SET_ETYPE(T_EV_SlideInVolume,"slide-in-volume",
                SlideInVolumeEvent);
      SET_ETYPE(T_EV_SetMasterInVolume,"set-master-in-volume",
                SetMasterInVolumeEvent);
      SET_ETYPE(T_EV_SetMasterOutVolume,"set-master-out-volume",
                SetMasterOutVolumeEvent);
      SET_ETYPE(T_EV_SetInVolume,"set-in-volume",
                SetInVolumeEvent);
      SET_ETYPE(T_EV_ToggleInputRecord,"toggle-input-record",
                ToggleInputRecordEvent);

      SET_ETYPE(T_EV_SetMidiEchoPort,"set-midi-echo-port",
                SetMidiEchoPortEvent);
      SET_ETYPE(T_EV_SetMidiEchoChannel,"set-midi-echo-channel",
                SetMidiEchoChannelEvent);
      SET_ETYPE(T_EV_AdjustMidiTranspose,"adjust-midi-transpose",
                AdjustMidiTransposeEvent);
      SET_ETYPE(T_EV_FluidSynthEnable,"fluidsynth-enable",
                FluidSynthEnableEvent);
      SET_ETYPE(T_EV_SetMidiTuning,"set-midi-tuning",
                SetMidiTuningEvent);

      SET_ETYPE(T_EV_SetTriggerVolume,"set-trigger-volume",
                SetTriggerVolumeEvent);
      SET_ETYPE(T_EV_SlideLoopAmp,"slide-loop-amplifier",SlideLoopAmpEvent);
      SET_ETYPE(T_EV_SetLoopAmp,"set-loop-amplifier",SetLoopAmpEvent);
      SET_ETYPE(T_EV_AdjustLoopAmp,"adjust-loop-amplifier",AdjustLoopAmpEvent);
      SET_ETYPE(T_EV_TriggerLoop,"trigger-loop",TriggerLoopEvent);
      SET_ETYPE_SLOW(T_EV_MoveLoop,"move-loop",MoveLoopEvent);
      SET_ETYPE_SLOW(T_EV_RenameLoop,"rename-loop",RenameLoopEvent);
      SET_ETYPE_SLOW(T_EV_EraseLoop,"erase-loop",EraseLoopEvent);
      SET_ETYPE_SLOW(T_EV_EraseAllLoops,"erase-all-loops",EraseAllLoopsEvent);
      SET_ETYPE_SLOW(T_EV_EraseSelectedLoops,"erase-selected-loops",
                     EraseSelectedLoopsEvent);
      SET_ETYPE(T_EV_SlideLoopAmpStopAll,"slide-loop-amplifier-stop-all",
                SlideLoopAmpStopAllEvent);

      SET_ETYPE_SLOW(T_EV_DeletePulse,"delete-pulse",DeletePulseEvent);
      SET_ETYPE_SLOW(T_EV_SelectPulse,"select-pulse",SelectPulseEvent);
      SET_ETYPE(T_EV_TapPulse,"tap-pulse",TapPulseEvent);
      SET_ETYPE(T_EV_SwitchMetronome,"switch-metronome",SwitchMetronomeEvent);
      SET_ETYPE(T_EV_SetSyncType,"set-sync-type",SetSyncTypeEvent);
      SET_ETYPE(T_EV_SetSyncSpeed,"set-sync-speed",SetSyncSpeedEvent);
      SET_ETYPE(T_EV_SetMidiSync,"set-midi-sync",SetMidiSyncEvent);
      
      SET_ETYPE(T_EV_SetVariable,"set-variable",SetVariableEvent);
      SET_ETYPE(T_EV_ToggleVariable,"toggle-variable",ToggleVariableEvent);
      SET_ETYPE(T_EV_SplitVariableMSBLSB, "split-variable-msb-lsb",SplitVariableMSBLSBEvent);

      SET_ETYPE(T_EV_ParamSetGetAbsoluteParamIdx,"paramset-get-absolute-param-index",
          ParamSetGetAbsoluteParamIdxEvent);
      SET_ETYPE(T_EV_ParamSetGetParam,"paramset-get-param",
          ParamSetGetParamEvent);
      SET_ETYPE(T_EV_ParamSetSetParam,"paramset-set-param",
          ParamSetSetParamEvent);

      SET_ETYPE(T_EV_LogFaderVolToLinear,"log-fader-to-linear",LogFaderVolToLinearEvent);

      SET_ETYPE(T_EV_VideoShowParamSetBank,"video-show-paramset-bank",
          VideoShowParamSetBankEvent);
      SET_ETYPE(T_EV_VideoShowParamSetPage,"video-show-paramset-page",
          VideoShowParamSetPageEvent);
      SET_ETYPE_SLOW(T_EV_VideoShowSnapshotPage,"video-show-snapshot-page",
                     VideoShowSnapshotPageEvent);
      SET_ETYPE_SLOW(T_EV_VideoShowLoop,"video-show-loop",VideoShowLoopEvent);
      SET_ETYPE_SLOW(T_EV_VideoShowLayout,"video-show-layout",
                     VideoShowLayoutEvent);
      SET_ETYPE_SLOW(T_EV_VideoSwitchInterface,"video-switch-interface",
                     VideoSwitchInterfaceEvent);
      SET_ETYPE_SLOW(T_EV_VideoShowDisplay,"video-show-display",
                     VideoShowDisplayEvent);
      SET_ETYPE_SLOW(T_EV_VideoShowHelp,"video-show-help",
                     VideoShowHelpEvent);
      SET_ETYPE_SLOW(T_EV_VideoFullScreen,"video-full-screen",
                     VideoFullScreenEvent);
      SET_ETYPE_SLOW(T_EV_ShowDebugInfo,"show-debug-info",
                     ShowDebugInfoEvent);

      SET_ETYPE_SLOW(T_EV_ToggleDiskOutput,"toggle-disk-output",
                     ToggleDiskOutputEvent);
      SET_ETYPE(T_EV_SetAutoLoopSaving,"set-auto-loop-saving",
                SetAutoLoopSavingEvent);
      SET_ETYPE_SLOW(T_EV_SaveLoop,"save-loop",SaveLoopEvent);
      SET_ETYPE_SLOW(T_EV_SaveNewScene,"save-new-scene",SaveNewSceneEvent);
      SET_ETYPE_SLOW(T_EV_SaveCurrentScene,"save-current-scene",
                     SaveCurrentSceneEvent);
      SET_ETYPE(T_EV_SetLoadLoopId,"set-load-loop-id",SetLoadLoopIdEvent);
      SET_ETYPE(T_EV_SetDefaultLoopPlacement,"set-default-loop-placement",
                SetDefaultLoopPlacementEvent);

      SET_ETYPE_SLOW(T_EV_ToggleSelectLoop,"toggle-select-loop",
                     ToggleSelectLoopEvent);
      SET_ETYPE_SLOW(T_EV_SelectOnlyPlayingLoops,"select-only-playing-loops",
                     SelectOnlyPlayingLoopsEvent);
      SET_ETYPE_SLOW(T_EV_SelectAllLoops,"select-all-loops",
                     SelectAllLoopsEvent);
      SET_ETYPE_SLOW(T_EV_TriggerSelectedLoops,"trigger-selected-loops",
                     TriggerSelectedLoopsEvent);
      SET_ETYPE(T_EV_SetSelectedLoopsTriggerVolume,
                "set-selected-loops-trigger-volume",
                SetSelectedLoopsTriggerVolumeEvent);
      SET_ETYPE(T_EV_AdjustSelectedLoopsAmp,
                "adjust-selected-loops-amp",
                AdjustSelectedLoopsAmpEvent);
      SET_ETYPE_SLOW(T_EV_InvertSelection,"invert-selection",
                     InvertSelectionEvent);

      SET_ETYPE_SLOW(T_EV_CreateSnapshot,"create-snapshot",
                     CreateSnapshotEvent);
      SET_ETYPE_SLOW(T_EV_RenameSnapshot,"rename-snapshot",
                     RenameSnapshotEvent);
      SET_ETYPE_SLOW(T_EV_TriggerSnapshot,"trigger-snapshot",
                     TriggerSnapshotEvent);
      SET_ETYPE_SLOW(T_EV_SwapSnapshots,"swap-snapshots",
                     SwapSnapshotsEvent);
      
      SET_ETYPE_SLOW(T_EV_BrowserMoveToItem,"browser-move-to-item",
                     BrowserMoveToItemEvent);
      SET_ETYPE_SLOW(T_EV_BrowserMoveToItemAbsolute,
                     "browser-move-to-item-absolute",
                     BrowserMoveToItemAbsoluteEvent);
      SET_ETYPE_SLOW(T_EV_BrowserSelectItem,"browser-select-item",
                     BrowserSelectItemEvent);
      SET_ETYPE_SLOW(T_EV_BrowserRenameItem,"browser-rename-item",
                     BrowserRenameItemEvent);
      SET_ETYPE(T_EV_BrowserItemBrowsed,"browser-item-browsed",
                BrowserItemBrowsedEvent);
      SET_ETYPE_SLOW(T_EV_PatchBrowserMoveToBank,"patchbrowser-move-to-bank",
                     PatchBrowserMoveToBankEvent);
      SET_ETYPE_SLOW(T_EV_PatchBrowserMoveToBankByIndex,
                     "patchbrowser-move-to-bank-by-index",
                     PatchBrowserMoveToBankByIndexEvent);

      SET_ETYPE_SLOW(T_EV_TransmitPlayingLoopsToDAW,
                     "transmit-playing-loops-to-daw",
                     TransmitPlayingLoopsToDAWEvent);

      // Internal events-- don't try to bind to these

      SET_ETYPE(T_EV_EndRecord,"__internal__",EndRecordEvent);
      SET_ETYPE(T_EV_LoopList,"__internal__",LoopListEvent);
      SET_ETYPE(T_EV_SceneMarker,"__internal__",SceneMarkerEvent);
      SET_ETYPE(T_EV_PulseSync,"__internal__",PulseSyncEvent);
      SET_ETYPE(T_EV_TriggerSet,"__internal__",TriggerSetEvent);
      SET_ETYPE_NUMPREALLOC(T_EV_AddProcessor,"__internal__",AddProcessorEvent,100);
      SET_ETYPE_NUMPREALLOC(T_EV_DelProcessor,"__internal__",DelProcessorEvent,100);
      SET_ETYPE_NUMPREALLOC(T_EV_CleanupProcessor,"__internal__",CleanupProcessorEvent,100);
      SET_ETYPE(T_EV_Input_MouseButton,"__internal__",MouseButtonInputEvent);
      SET_ETYPE(T_EV_Input_MouseMotion,"__internal__",MouseMotionInputEvent);
      
    default:
      break;
    }
  }
};

void Event::TakedownEventTypeTable() {
  int evnum = (int) EventType(T_EV_Last);
  for (int i = 0; i < evnum; i++) {
    // Deleting the manager will delete all instances
    // allocated thru it--
    if (ett[i].mgr != 0) 
      delete ett[i].mgr;
    // so the prototype base instance is already deleted
    ett[i].proto = 0;
  }

  delete[] ett;
};

Event *Event::GetEventByType(EventType typ, char wait) {
  if (ett == 0) {
    printf("EVENT: ERROR- no event type table!\n");
    exit(1);
  }

  int i = (int) typ;
  if (ett[i].mgr != 0) {
    Event *ret = (Event *) ett[i].mgr->RTNew();
    if (ret != 0)
      return ret;
    else {
      // No instance available
      if (wait) {
        // Wait
        printf("EVENT: Waiting for memory to be allocated.\n"); 
        do {
          usleep(10000);
          ret = (Event *) ett[i].mgr->RTNew();
        } while (ret == 0);
        return ret;
      } else {
        return 0; // Don't wait
      }
    }
  }
  else if (ett[i].proto != 0) {
    Event *ret = (Event *) ett[i].proto->RTNew();
    if (ret != 0)
      return ret;
    else {
      // No instance available
      if (wait) {
        // Wait
        printf("EVENT: Waiting for mem alloc...\n");    
        do {
          usleep(10000);
          ret = (Event *) ett[i].proto->RTNew();
        } while (ret == 0);
        return ret;
      } else {
        return 0; // Don't wait
      }
    }
  }
  else {
    printf("ERROR: no prototype or mgr for event type: '%s'\n",
           ett[i].name);
    return 0;
  }
};

Event *Event::GetEventByName(char *evtname, char wait) {
  if (ett == 0) {
    printf("EVENT: Error- no event type table!\n");
    exit(1);
  }

  int evnum = (int) EventType(T_EV_Last);
  for (int i = 0; i < evnum; i++) {
    if (ett[i].name != 0) 
      if (!strcmp(evtname, ett[i].name))
        return GetEventByType((EventType) i,wait);
  }

  return 0;
};

EventManager::EventManager () : eq(0), needs_wakeup(0), threadgo(1) {
  printf("Start event manager.\n");

  // Create listener structure..
  int evnum = (int) EventType(T_EV_Last);
  listeners = new EventListenerItem *[evnum];
  for (int i = 0; i < evnum; i++)
    listeners[i] = 0;

  pthread_mutex_init(&dispatch_thread_lock,0);
  pthread_mutex_init(&listener_list_lock,0);
  pthread_cond_init(&dispatch_ready,0);

  const static size_t STACKSIZE = 1024*128;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("EVENT: Stacksize: %d.\n",STACKSIZE);

  // Hold dispatch thread until ready
  pthread_mutex_lock(&dispatch_thread_lock);

  // Start an event dispatch thread
  int ret = pthread_create(&dispatch_thread,
                           &attr,
                           run_dispatch_thread,
                           static_cast<void *>(this));
  if (ret != 0) {
    printf("(eventmanager) pthread_create failed, exiting");
    exit(1);
  }
  SRMWRingBuffer_Writers::RegisterWriter(dispatch_thread);

  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));

  // Event dispatch thread is NOT RT
  schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
  if (pthread_setschedparam(dispatch_thread, SCHED_OTHER, &schp) != 0) {
    printf("EVENT: Can't set hi priority thread, will use regular!\n");
  }
};

EventManager::~EventManager() {
  //printf("Event Manager: cleanup... this: %p\n",this);   
  
  // Terminate the dispatch thread
  threadgo = 0;
  pthread_mutex_lock (&dispatch_thread_lock);
  pthread_cond_signal (&dispatch_ready);
  pthread_mutex_unlock (&dispatch_thread_lock);
  pthread_join(dispatch_thread,0);

  pthread_cond_destroy (&dispatch_ready);
  pthread_mutex_destroy (&dispatch_thread_lock);
  pthread_mutex_destroy (&listener_list_lock);
  
  int evnum = (int) EventType(T_EV_Last);
  for (int i = 0; i < evnum; i++) {
    // Erase listeners
    EventListenerItem *cur = listeners[i];
    while (cur != 0) {
      EventListenerItem *tmp = cur->next; 
      delete cur;
      cur = tmp;
    }
  }
  
  delete[] listeners;
  if (eq != 0)
    delete eq;
  
  // Takedown event type table
  // printf(" .. ETT takedown (this: %p)\n",this);   
  Event::TakedownEventTypeTable();
  // printf(" .. ETT takedown done (this: %p)\n",this);   
  
  printf("EVENT: manager end.\n");
};

// Create ring buffers after ALL writer threads are created
void EventManager::FinalPrep() {
  printf("EVENT: Create ringbuffers and begin.\n");

  eq = new SRMWRingBuffer<Event *>(EVENT_QUEUE_SIZE);

  // Start processing
  pthread_mutex_unlock(&dispatch_thread_lock);
};

// Event queue functions ** NOT THREADSAFE **

Event *EventManager::DeleteQueue(Event *first) {
  Event *cur = first;
  while (cur != 0) {
    Event *tmp = cur->next;
    cur->RTDelete();
    cur = tmp;
  }

  return 0;
}

void EventManager::QueueEvent(Event **first, Event *nw) {
  Event *cur = *first;
  if (cur == 0)
    *first = nw;
  else {
    while (cur->next != 0)
      cur = cur->next;

    cur->next = nw;
  }
};

void EventManager::RemoveEvent(Event **first, Event *prev, Event **cur) {
  Event *tmp = (*cur)->next;
  if (prev != 0)
    prev->next = tmp;
  else
    *first = tmp;
  (*cur)->RTDelete();
  *cur = tmp;
};

// ** End event queue functions

// Broadcast through dispatch thread!
// RT and threadsafe, so long as you allocate your event with RTNew()
void EventManager::BroadcastEvent(Event *ev,
                    EventProducer *source) {
  // printf("*** THREAD (BROADCAST): %li\n",pthread_self());

  ev->from = source;
  //ev->time = mygettime();

  // Write to queue
  eq->WriteElement(ev);

  // Wakeup dispatch thread
  WakeupIfNeeded(1);

  // printf("EVENT: SENT: %s!\n",Event::ett[(int) ev->GetType()].name);
};

void *EventManager::run_dispatch_thread (void *ptr) {
  EventManager *inst = static_cast<EventManager *>(ptr);

  pthread_mutex_lock(&inst->dispatch_thread_lock);

  while (inst->threadgo) {
    // Scan through all events
    Event *cur = inst->eq->ReadElement();
    while (cur != 0) {
      //printf("broadcast thread\n");
      // Print time elapsed since broadcast
      //double dt = (mygettime()-cur->time) * 1000;
      //printf("Evt dispatch- dt: %2.2f ms\n",dt);

      if (cur->GetMgr() == 0)
        printf("EVENT: WARNING: Broadcast from RT nonRT event!!\n");

      // printf("EVENT: DISPATCH: %s!\n",Event::ett[(int) cur->GetType()].name);
      inst->BroadcastEventNow(cur,cur->from,0,0); // Force delivery now,
                                                  // don't erase til we
                                                  // advance

      cur->RTDelete();
      cur = inst->eq->ReadElement();
    }

    // No more events in queue

    // Wait for wakeup
    pthread_cond_wait (&inst->dispatch_ready, &inst->dispatch_thread_lock);

    // printf("EVENT: WAKEUP!\n");
    inst->needs_wakeup = 0; // Woken!
  }

  printf("Event Manager: end dispatch thread\n");

  pthread_mutex_unlock(&inst->dispatch_thread_lock);

  return 0;
};

// Not RT safe, but threadsafe!
// Listen for the given event (optionally from the given producer) and callme
// when it occurs-- optionally, block calls from myself
void EventManager::ListenEvent(EventListener *callme,
                 EventProducer *from, EventType type,
                 char block_self_calls) {
  EventListenerItem *nw = new EventListenerItem(callme,from,type,
                                                block_self_calls);

  pthread_mutex_lock(&listener_list_lock);

  // Add to the listeners list
  int evnum = (int) type;
  EventListenerItem *cur = listeners[evnum];
  if (cur == 0)
    listeners[evnum] = nw; // That was easy, now we have 1 item
  else {
    while (cur->next != 0)
      cur = cur->next;
    cur->next = nw; // Link up the last item to new1
  }

  pthread_mutex_unlock(&listener_list_lock);
};

// Not RT safe!
void EventManager::UnlistenEvent(EventListener *callme,
                   EventProducer *from, EventType type) {
  pthread_mutex_lock(&listener_list_lock);

  // Remove from the listeners list
  int evnum = (int) type;
  EventListenerItem *cur = listeners[evnum],
    *prev = 0;

  // Search for those listening to 'from' & 'type'
  while (cur != 0 && (cur->callwhom != callme ||
                      cur->eventsfrom != from)) {
    prev = cur;
    cur = cur->next;
  }

  if (cur != 0) {
    // Got it, unlink!
    if (prev != 0)
      prev->next = cur->next;
    else
      listeners[evnum] = cur->next;
    delete cur;
  }

  pthread_mutex_unlock(&listener_list_lock);
};

