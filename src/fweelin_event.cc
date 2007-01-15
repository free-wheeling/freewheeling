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
      SET_ETYPE(T_EV_Input_MIDIPitchBend,"midipitchbend",
		MIDIPitchBendInputEvent);

      SET_ETYPE(T_EV_LoopClicked,"loop-clicked",LoopClickedEvent);

      SET_ETYPE(T_EV_GoSub,"go-sub",GoSubEvent);
      SET_ETYPE(T_EV_StartSession,"start-freewheeling",StartSessionEvent);
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
      SET_ETYPE(T_EV_MoveLoop,"move-loop",MoveLoopEvent);
      SET_ETYPE_SLOW(T_EV_RenameLoop,"rename-loop",RenameLoopEvent);
      SET_ETYPE_SLOW(T_EV_EraseLoop,"erase-loop",EraseLoopEvent);
      SET_ETYPE_SLOW(T_EV_EraseAllLoops,"erase-all-loops",EraseAllLoopsEvent);
      SET_ETYPE(T_EV_SlideLoopAmpStopAll,"slide-loop-amplifier-stop-all",
		SlideLoopAmpStopAllEvent);

      SET_ETYPE_SLOW(T_EV_DeletePulse,"delete-pulse",DeletePulseEvent);
      SET_ETYPE_SLOW(T_EV_SelectPulse,"select-pulse",SelectPulseEvent);
      SET_ETYPE(T_EV_TapPulse,"tap-pulse",TapPulseEvent);
      SET_ETYPE(T_EV_SwitchMetronome,"switch-metronome",SwitchMetronomeEvent);
      SET_ETYPE(T_EV_SetSyncType,"set-sync-type",SetSyncTypeEvent);
      SET_ETYPE(T_EV_SetSyncSpeed,"set-sync-speed",SetSyncSpeedEvent);

      SET_ETYPE(T_EV_SetVariable,"set-variable",SetVariableEvent);
      SET_ETYPE(T_EV_ToggleVariable,"toggle-variable",ToggleVariableEvent);

      SET_ETYPE(T_EV_VideoShowLoop,"video-show-loop",VideoShowLoopEvent);
      SET_ETYPE(T_EV_VideoShowLayout,"video-show-layout",
		VideoShowLayoutEvent);
      SET_ETYPE(T_EV_VideoShowDisplay,"video-show-display",
		VideoShowDisplayEvent);
      SET_ETYPE(T_EV_VideoShowHelp,"video-show-help",
		VideoShowHelpEvent);
      SET_ETYPE_SLOW(T_EV_VideoFullScreen,"video-full-screen",
		     VideoFullScreenEvent);
      SET_ETYPE(T_EV_ShowDebugInfo,"show-debug-info",
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
      SET_ETYPE_SLOW(T_EV_InvertSelection,"invert-selection",
		     InvertSelectionEvent);
      
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

      // Internal events-- don't try to bind to these

      SET_ETYPE(T_EV_EndRecord,"__internal__",EndRecordEvent);
      SET_ETYPE(T_EV_LoopList,"__internal__",LoopListEvent);
      SET_ETYPE(T_EV_SceneMarker,"__internal__",SceneMarkerEvent);
      SET_ETYPE(T_EV_PulseSync,"__internal__",PulseSyncEvent);
      SET_ETYPE(T_EV_TriggerSet,"__internal__",TriggerSetEvent);
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
  
  // Takedown event type table
  // printf(" .. ETT takedown (this: %p)\n",this);   
  Event::TakedownEventTypeTable();
  // printf(" .. ETT takedown done (this: %p)\n",this);   
  
  printf("EVENT: manager end.\n");
};
