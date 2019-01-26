#ifndef __FWEELIN_EVENT_H
#define __FWEELIN_EVENT_H

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

#include <linux/limits.h>
#include <pthread.h>

#include <SDL/SDL.h>
extern "C"
{
#include <jack/ringbuffer.h>
}

// Event parameter name for interface ID
#define INTERFACEID "interfaceid"

class Loop;
class Event;
class ProcessorItem;

class EventProducer {
};

class EventListener {
 public:
  
  virtual void ReceiveEvent(Event *ev, EventProducer *from) = 0;
};

// An event hook gets first dibs on incoming events
// We use this to override the usual functions of, say, the keyboard,
// and redirect them elsewhere, like typing text.
class EventHook {
 public:

  // HookEvent works just like EventListener::ReceiveEvent,
  // except it returns nonzero if it has swallowed the event
  // and zero if it has disregarded the event.
  // 
  // If HookEvent disregards the event, it is processed as defined
  // through the configuration system
  virtual char HookEvent(Event *ev, EventProducer *from) = 0;
};

double mygettime(void);

// Max filename length
#define FWEELIN_OUTNAME_LEN PATH_MAX

#define MAX_MIDI_CHANNELS 16
#define MAX_MIDI_CONTROLLERS 127
#define MAX_MIDI_NOTES 127
#define MAX_MIDI_PORTS 4
#define MIDI_CC_SUSTAIN 64  // Sustain pedal cc

// Get a new block of events of the given type
#define EVT_NEW_BLOCK(typ,etyp) ::new typ[Event::GetMemMgrByType(etyp)-> \
                                          GetBlockSize()]

// Basic defines for within an Event
#define EVT_DEFINE(typ,etyp) \
  typ() { Recycle(); }; \
  \
  virtual Preallocated *NewInstance() { \
    return EVT_NEW_BLOCK(typ,etyp); \
  }; \
  virtual EventType GetType() { return etyp; }; \
  FWMEM_DEFINE_DELBLOCK

#define EVT_DEFINE_NO_CONSTR(typ,etyp) \
  virtual Preallocated *NewInstance() { \
    return EVT_NEW_BLOCK(typ,etyp); \
  }; \
  virtual EventType GetType() { return etyp; }; \
  FWMEM_DEFINE_DELBLOCK

#define EVT_DEFINE_NO_BLOCK(typ,etyp) \
  typ() { Recycle(); }; \
  \
  virtual Preallocated *NewInstance() { \
    return ::new typ(); \
  }; \
  virtual EventType GetType() { return etyp; };

// List of all types of events
enum EventType {
  T_EV_None,

  T_EV_Input_Key,
  T_EV_Input_JoystickButton,
  T_EV_Input_MIDIKey,
  T_EV_Input_MIDIController,
  T_EV_Input_MIDIProgramChange,
  T_EV_Input_MIDIChannelPressure,
  T_EV_Input_MIDIPitchBend,
  T_EV_StartSession,
  T_EV_StartInterface,
  T_EV_GoSub,

  T_EV_LoopClicked,
  T_EV_BrowserItemBrowsed,

  // End of bindable events-
  // Events after this can not trigger config bindings
  T_EV_Last_Bindable,

  T_EV_Input_MIDIClock,
  T_EV_Input_MIDIStartStop,

  T_EV_Input_MouseButton,
  T_EV_Input_MouseMotion,

  T_EV_ALSAMixerControlSet,

  T_EV_EndRecord,
  T_EV_LoopList,
  T_EV_SceneMarker,
  T_EV_PulseSync,
  T_EV_TriggerSet,
  T_EV_AddProcessor,
  T_EV_DelProcessor,
  T_EV_CleanupProcessor,

  T_EV_SetVariable,
  T_EV_ToggleVariable,
  T_EV_SplitVariableMSBLSB,

  T_EV_ParamSetGetAbsoluteParamIdx,
  T_EV_ParamSetGetParam,
  T_EV_ParamSetSetParam,

  T_EV_LogFaderVolToLinear,

  T_EV_VideoShowParamSetBank,
  T_EV_VideoShowParamSetPage,
  T_EV_VideoShowSnapshotPage,
  T_EV_VideoShowLoop,
  T_EV_VideoShowLayout,
  T_EV_VideoSwitchInterface,
  T_EV_VideoShowDisplay,
  T_EV_VideoShowHelp,
  T_EV_VideoFullScreen,
  T_EV_ShowDebugInfo,

  T_EV_ExitSession,

  T_EV_SlideMasterInVolume,
  T_EV_SlideMasterOutVolume,
  T_EV_SlideInVolume,
  T_EV_SetMasterInVolume,
  T_EV_SetMasterOutVolume,
  T_EV_SetInVolume,
  T_EV_ToggleInputRecord,

  T_EV_SetMidiEchoPort,
  T_EV_SetMidiEchoChannel,
  T_EV_AdjustMidiTranspose,
  T_EV_FluidSynthEnable,
  T_EV_SetMidiTuning,
  
  T_EV_DeletePulse,
  T_EV_SelectPulse,
  T_EV_TapPulse,
  T_EV_SwitchMetronome,
  T_EV_SetSyncType,
  T_EV_SetSyncSpeed,
  T_EV_SetMidiSync,
  
  T_EV_ToggleSelectLoop,
  T_EV_SelectOnlyPlayingLoops,
  T_EV_SelectAllLoops,
  T_EV_TriggerSelectedLoops,
  T_EV_SetSelectedLoopsTriggerVolume,
  T_EV_AdjustSelectedLoopsAmp,
  T_EV_InvertSelection,
  
  T_EV_CreateSnapshot,
  T_EV_RenameSnapshot,
  T_EV_TriggerSnapshot,
  T_EV_SwapSnapshots,

  T_EV_SetTriggerVolume,
  T_EV_SlideLoopAmp,
  T_EV_SetLoopAmp,
  T_EV_AdjustLoopAmp,
  T_EV_TriggerLoop,
  T_EV_MoveLoop,
  T_EV_RenameLoop,
  T_EV_EraseLoop,
  T_EV_EraseAllLoops,
  T_EV_EraseSelectedLoops,
  T_EV_SlideLoopAmpStopAll,

  T_EV_ToggleDiskOutput,
  T_EV_SetAutoLoopSaving,
  T_EV_SaveLoop,
  T_EV_SaveNewScene,
  T_EV_SaveCurrentScene,
  T_EV_SetLoadLoopId,
  T_EV_SetDefaultLoopPlacement,

  T_EV_BrowserMoveToItem,
  T_EV_BrowserMoveToItemAbsolute,
  T_EV_BrowserSelectItem,
  T_EV_BrowserRenameItem,
  T_EV_PatchBrowserMoveToBank,
  T_EV_PatchBrowserMoveToBankByIndex,

  T_EV_TransmitPlayingLoopsToDAW,

  T_EV_Last
};

#include "fweelin_datatypes.h"
#include "fweelin_block.h"

// Gets the offset of given variable into the current class
#define FWEELIN_GETOFS(a) ((long)&a - (long)this)

class SDLKeyList;
class PreallocatedType;
class Event;

// EventParameters are parameters in events that can be controlled
// from inputs such as MIDI and Keyboard (see InputMatrix)
class EventParameter {
 public:
  // Specify an event parameter with given name, offset into event,
  // size of data type, and max index (-1 by default if parameter
  // is not indexed)
  EventParameter (char *name = 0, long ofs = 0, 
                  CoreDataType dtype = T_char,
                  int max_index = -1) :
    name(name), ofs(ofs), dtype(dtype), max_index(max_index) {};

  char *name; // Name of event parameter
  long ofs; // Offset into event class of data
  CoreDataType dtype; // Type of data
  int max_index; // Config stores a hashtable for input events for
                 // quick triggering-- this is the # of hash indexes 
                 // for an indexed parameter, or -1 for an unindexed param
};

// Table of all event types and memory managers for them
class EventTypeTable {
 public:
  EventTypeTable (char *name = 0, PreallocatedType *mgr = 0,
                  Event *proto = 0, int paramidx = -1, char slowdelivery = 0) :
    name(name), pretype(mgr), proto(proto), paramidx(paramidx), 
    slowdelivery(slowdelivery) {};

  char *name;
  PreallocatedType *pretype;
  Event *proto;
  int paramidx; // Index of event parameter that is used for hash index
                // For example, in the keyboard input event, the keysym 
                // is indexed for quick triggering of keyboard events by key
  char slowdelivery; // Nonzero if this event should be delivered slow, in
                     // a nonRT thread. This is useful for events that cause
                     // nonRT-safe operations to be executed.
};

// Events can be allocated in realtime using class Preallocated
class Event : public Preallocated {
 public:
  Event() { Recycle(); };
  virtual ~Event() {};

  virtual void Recycle() {
    from = 0;
    to = 0;
    next = 0;
    time.tv_sec = 0;
    time.tv_nsec = 0;
    echo = 0;
  };

  virtual void operator = (const Event &/*evt*/) {};
  virtual EventType GetType() { return T_EV_None; };  
  // Returns the number of parameters this event has
  virtual int GetNumParams() { return 0; };
  // Returns the nth parameter
  virtual EventParameter GetParam(int /*param_n*/) {
    return EventParameter();
  };

  // Get the memory manager for the given type
  static inline PreallocatedType *GetMemMgrByType(EventType typ) {
    return ett[(int) typ].pretype;
  };
  // Returns an instance of the event named 'evtname'
  // If wait is nonzero and there are no free instances through RTNew,
  // we wait until one becomes available
  static Event *GetEventByName(char *evtname, char wait = 0); 
  // Returns an instance of the event with given type
  static Event *GetEventByType(EventType typ, char wait = 0);
  // Returns the index of the indexed parameter for the event with given
  // type
  static int GetParamIdxByType(EventType typ) { return ett[typ].paramidx; };
  // Returns the string name of the given event type
  static char *GetEventName(EventType typ) { return ett[typ].name; };

  static EventTypeTable *ett;
  static void SetupEventTypeTable(MemoryManager *mmgr);
  static void TakedownEventTypeTable();

  // In an event queue, stores who this event is from
  EventProducer *from;
  // In an event queue, stores who this event is to
  EventListener *to;
  // In an event queue, this stores pointer to next event
  Event *next;
  // Time to send event
  struct timespec time;
  // Event is being echoed to MIDI outputs?
  // If echo is nonzero, the event is sent through the patch routing
  // for the currently selected patch- affecting the port(s) and channel(s)
  // where the event is sent. If echo is 0, the event is sent out as described
  // in the event (ie via outport and midichannel parameters).
  char echo;
};

// GoSub is really an event that encapsulates other events
// It allows us to fire off a subroutine of events by triggering one GoSubEvent
// The events that are fired off are defined by creating a binding to
// input event "go-sub"
class GoSubEvent : public Event {
  // Size of hashtable for indexing based on subs parameter
  const static int SUBS_HASH = 127;

 public:
  EVT_DEFINE(GoSubEvent,T_EV_GoSub);

  virtual void operator = (const Event &src) {
    GoSubEvent &s = (GoSubEvent &) src;
    sub = s.sub;
    param1 = s.param1;
    param2 = s.param2;
    param3 = s.param3;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("sub",FWEELIN_GETOFS(sub),T_int,SUBS_HASH);
    case 1:
      return EventParameter("param1",FWEELIN_GETOFS(param1),T_float);
    case 2:
      return EventParameter("param2",FWEELIN_GETOFS(param2),T_float);
    case 3:
      return EventParameter("param3",FWEELIN_GETOFS(param3),T_float);
    }

    return EventParameter();
  };    

  int sub;      // Subroutine #
  float param1,     // Parameter 1
        param2,     // Parameter 2
        param3;     // Parameter 3
};

class KeyInputEvent : public Event {
 public:   
  EVT_DEFINE(KeyInputEvent,T_EV_Input_Key);
  virtual void operator = (const Event &src) {
    KeyInputEvent &s = (KeyInputEvent &) src;
    down = s.down;
    keysym = s.keysym;
    unicode = s.unicode;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("keydown",FWEELIN_GETOFS(down),T_char);
    case 1:
      return EventParameter("key",FWEELIN_GETOFS(keysym),T_int,SDLK_LAST);
    case 2:
      return EventParameter("unicode",FWEELIN_GETOFS(unicode),T_int);
    }

    return EventParameter();
  };    

  char down; // Nonzero if key is pressed, zero if key is released
  int keysym, // Keysym of key pressed
    unicode;  // Unicode translation (if enabled)
  int presslen;
};

class LoopClickedEvent : public Event {
 public:   
  EVT_DEFINE(LoopClickedEvent,T_EV_LoopClicked);
  virtual void operator = (const Event &src) {
    LoopClickedEvent &s = (LoopClickedEvent &) src;
    down = s.down;
    button = s.button;
    loopid = s.loopid;
    in = s.in;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("down",FWEELIN_GETOFS(down),T_char);
    case 1:
      return EventParameter("button",FWEELIN_GETOFS(button),T_int);
    case 2:
      return EventParameter("loopid",FWEELIN_GETOFS(loopid),T_int);
    case 3:
      return EventParameter("in",FWEELIN_GETOFS(in),T_char);
    }

    return EventParameter();
  };    

  char down, // Nonzero if button is pressed, zero if button is released
    in;      // Zero if clicked in looptray, one if clicked in layout
  int button; // Button # pressed/released
  int loopid; // LoopID of loop that was clicked
  int presslen;
};

class JoystickButtonInputEvent : public Event {
 public:   
  EVT_DEFINE(JoystickButtonInputEvent,T_EV_Input_JoystickButton);
  virtual void operator = (const Event &src) {
    JoystickButtonInputEvent &s = (JoystickButtonInputEvent &) src;
    down = s.down;
    joystick = s.joystick;
    button = s.button;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("down",FWEELIN_GETOFS(down),T_char);
    case 1:
      return EventParameter("button",FWEELIN_GETOFS(button),T_int);
    case 2:
      return EventParameter("joystick",FWEELIN_GETOFS(joystick),T_int);
    }

    return EventParameter();
  };    

  char down;    // Nonzero if button is pressed, zero if button is released
  int button;   // Button # pressed/released
  int joystick; // Index of joystick
  int presslen;
};

class MouseButtonInputEvent : public Event {
 public:   
  EVT_DEFINE(MouseButtonInputEvent,T_EV_Input_MouseButton);
  virtual void operator = (const Event &src) {
    MouseButtonInputEvent &s = (MouseButtonInputEvent &) src;
    down = s.down;
    button = s.button;
    x = s.x;
    y = s.y;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("down",FWEELIN_GETOFS(down),T_char);
    case 1:
      return EventParameter("button",FWEELIN_GETOFS(button),T_int);
    case 2:
      return EventParameter("x",FWEELIN_GETOFS(x),T_int);
    case 3:
      return EventParameter("y",FWEELIN_GETOFS(y),T_int);
    }

    return EventParameter();
  };    

  char down; // Nonzero if button is pressed, zero if button is released
  int button; // Button # pressed/released
  int x, y;   // Coordinates of press (on screen)
  int presslen;
};

class MouseMotionInputEvent : public Event {
 public:   
  EVT_DEFINE(MouseMotionInputEvent,T_EV_Input_MouseMotion);
  virtual void operator = (const Event &src) {
    MouseMotionInputEvent &s = (MouseMotionInputEvent &) src;
    x = s.x;
    y = s.y;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("x",FWEELIN_GETOFS(x),T_int);
    case 1:
      return EventParameter("y",FWEELIN_GETOFS(y),T_int);
    }

    return EventParameter();
  };    

  int x, y;   // Coordinates of mouse motion (on screen)
};

class MIDIControllerInputEvent : public Event {
 public:
  EVT_DEFINE(MIDIControllerInputEvent,T_EV_Input_MIDIController);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIControllerInputEvent &s = (MIDIControllerInputEvent &) src;
    outport = s.outport;
    channel = s.channel;
    ctrl = s.ctrl;
    val = s.val;
    echo = s.echo;
  };
  virtual int GetNumParams() { return 5; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int);
    case 2:
      return EventParameter("controlnum",FWEELIN_GETOFS(ctrl),T_int,
                            MAX_MIDI_CONTROLLERS);
    case 3:
      return EventParameter("controlval",FWEELIN_GETOFS(val),T_int);
    case 4:
      return EventParameter("routethroughpatch",FWEELIN_GETOFS(echo),T_char);
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
    channel,   // MIDI channel
    ctrl,      // controller #
    val;       // value
};

class MIDIChannelPressureInputEvent : public Event {
 public:   
  EVT_DEFINE(MIDIChannelPressureInputEvent,
             T_EV_Input_MIDIChannelPressure);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIChannelPressureInputEvent &s = (MIDIChannelPressureInputEvent &) src;
    outport = s.outport;
    channel = s.channel;
    val = s.val;
    echo = s.echo;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
                            MAX_MIDI_CHANNELS);
    case 2:
      return EventParameter("pressureval",FWEELIN_GETOFS(val),T_int);
    case 3:
      return EventParameter("routethroughpatch",FWEELIN_GETOFS(echo),T_char);
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
      channel, // MIDI channel
      val;     // Channel pressure value
};

class MIDIProgramChangeInputEvent : public Event {
 public:   
  EVT_DEFINE(MIDIProgramChangeInputEvent,
             T_EV_Input_MIDIProgramChange);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIProgramChangeInputEvent &s = (MIDIProgramChangeInputEvent &) src;
    outport = s.outport;
    channel = s.channel;
    val = s.val;
    echo = s.echo;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
                            MAX_MIDI_CHANNELS);
    case 2:
      return EventParameter("programval",FWEELIN_GETOFS(val),T_int);
    case 3:
      return EventParameter("routethroughpatch",FWEELIN_GETOFS(echo),T_char);
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
    channel, // MIDI channel
    val;       // program change value
};

class MIDIPitchBendInputEvent : public Event {
 public:   
  EVT_DEFINE(MIDIPitchBendInputEvent,T_EV_Input_MIDIPitchBend);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIPitchBendInputEvent &s = (MIDIPitchBendInputEvent &) src;
    outport = s.outport;
    channel = s.channel;
    val = s.val;
    echo = s.echo;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
                            MAX_MIDI_CHANNELS);
    case 2:
      return EventParameter("pitchval",FWEELIN_GETOFS(val),T_int);
    case 3:
      return EventParameter("routethroughpatch",FWEELIN_GETOFS(echo),T_char);
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
    channel,   // MIDI channel
    val;       // pitch bend value
};

class MIDIKeyInputEvent : public Event {
 public:
  EVT_DEFINE(MIDIKeyInputEvent,T_EV_Input_MIDIKey);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIKeyInputEvent &s = (MIDIKeyInputEvent &) src;
    outport = s.outport;
    down = s.down;
    channel = s.channel;
    notenum = s.notenum;
    vel = s.vel;
    echo = s.echo;
  };
  virtual int GetNumParams() { return 6; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("keydown",FWEELIN_GETOFS(down),T_char);
    case 2:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
                            MAX_MIDI_CHANNELS);
    case 3:
      return EventParameter("notenum",FWEELIN_GETOFS(notenum),T_int);
    case 4:
      return EventParameter("velocity",FWEELIN_GETOFS(vel),T_int);
    case 5:
      return EventParameter("routethroughpatch",FWEELIN_GETOFS(echo),T_char);
    }

    return EventParameter();
  };    

  char down; // Nonzero if key is pressed, zero if key is released
  int outport, // # of MIDI output to send event to
    channel, // MIDI channel
    notenum,   // note number
    vel;       // velocity
};

class MIDIClockInputEvent : public Event {
public:
  EVT_DEFINE(MIDIClockInputEvent,T_EV_Input_MIDIClock);
  virtual void Recycle() {
    outport = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIClockInputEvent &s = (MIDIClockInputEvent &) src;
    outport = s.outport;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    }
    
    return EventParameter();
  };    
  
  int outport; // # of MIDI output to send event to
};

class MIDIStartStopInputEvent : public Event {
public:
  EVT_DEFINE(MIDIStartStopInputEvent,T_EV_Input_MIDIStartStop);
  virtual void Recycle() {
    outport = 1;
    start = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    MIDIStartStopInputEvent &s = (MIDIStartStopInputEvent &) src;
    outport = s.outport;
    start = s.start;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
      case 1:
        return EventParameter("start",FWEELIN_GETOFS(start),T_char);
    }
    
    return EventParameter();
  };    
  
  int outport; // # of MIDI output to send event to
  char start;  // 1- MIDI Start, 0- MIDI Stop
};

class SetVariableEvent : public Event {
 public:
  EVT_DEFINE(SetVariableEvent,T_EV_SetVariable);
  virtual void Recycle() {
    maxjumpcheck = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetVariableEvent &s = (SetVariableEvent &) src;
    var = s.var;
    value.type = s.value.type;
    value = s.value;
    maxjumpcheck = s.maxjumpcheck;
    maxjump.type = s.maxjump.type;
    maxjump = s.maxjump;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variableref);
    case 1:
      return EventParameter("value",FWEELIN_GETOFS(value),T_variable);      
    case 2:
      return EventParameter("maxjumpcheck",FWEELIN_GETOFS(maxjumpcheck),T_char);      
    case 3:
      return EventParameter("maxjump",FWEELIN_GETOFS(maxjump),T_variable);      
    }

    return EventParameter();
  };    

  UserVariable *var;  // Variable to set
  UserVariable value, // Value to set it to
    maxjump;          // Maximum jump in variable between current value and new value
                      // Jumps beyond maxjump cause the variable not to be set
  char maxjumpcheck;  // Nonzero if we should check variable change against maxjump-
                      // If maxjumpcheck is zero, the variable is always set
};

class ToggleVariableEvent : public Event {
 public:
  EVT_DEFINE(ToggleVariableEvent,T_EV_ToggleVariable);
  virtual void Recycle() {
    maxvalue = 1;
    minvalue = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ToggleVariableEvent &s = (ToggleVariableEvent &) src;
    var = s.var;
    maxvalue = s.maxvalue;
    minvalue = s.minvalue;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variableref);
    case 1:
      return EventParameter("maxvalue",FWEELIN_GETOFS(maxvalue),T_int);
    case 2:
      return EventParameter("minvalue",FWEELIN_GETOFS(minvalue),T_int);
    }

    return EventParameter();
  };

  UserVariable *var;  // Variable to increment (toggle)
  int maxvalue,       // Maximum value of variable before wraparound
    minvalue;         // Value to wrap to
};

class SplitVariableMSBLSBEvent : public Event {
 public:
  EVT_DEFINE(SplitVariableMSBLSBEvent,T_EV_SplitVariableMSBLSB);
  virtual void Recycle() {
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SplitVariableMSBLSBEvent &s = (SplitVariableMSBLSBEvent &) src;
    var = s.var;
    msb = s.msb;
    lsb = s.lsb;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variable);
    case 1:
      return EventParameter("msb",FWEELIN_GETOFS(msb),T_variableref);
    case 2:
      return EventParameter("lsb",FWEELIN_GETOFS(lsb),T_variableref);
    }

    return EventParameter();
  };

  UserVariable var;   // Variable to split
  UserVariable *msb,  // MSB of var will be stored here
    *lsb;             // LSB of var will be stored here. var is unchanged
};

class ParamSetGetAbsoluteParamIdxEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(ParamSetGetAbsoluteParamIdxEvent,T_EV_ParamSetGetAbsoluteParamIdx);
  ParamSetGetAbsoluteParamIdxEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    paramidx = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ParamSetGetAbsoluteParamIdxEvent &s = (ParamSetGetAbsoluteParamIdxEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    paramidx = s.paramidx;
    absidx = s.absidx;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("paramidx",FWEELIN_GETOFS(paramidx),T_int);
    case 3:
      return EventParameter("absidx",FWEELIN_GETOFS(absidx),T_variableref);
    }

    return EventParameter();
  };

  int interfaceid, // Interface in which parameter set display is defined
    displayid,     // Display ID of parameter set display
    paramidx;      // Relative index of parameter
  UserVariable *absidx;     // Absolute index of parameter will be stored in absidx
};

class ParamSetGetParamEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(ParamSetGetParamEvent,T_EV_ParamSetGetParam);
  ParamSetGetParamEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    paramidx = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ParamSetGetParamEvent &s = (ParamSetGetParamEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    paramidx = s.paramidx;
    var = s.var;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("paramidx",FWEELIN_GETOFS(paramidx),T_int);
    case 3:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variableref);
    }

    return EventParameter();
  };

  int interfaceid, // Interface in which parameter set display is defined
    displayid,     // Display ID of parameter set display
    paramidx;      // Index of parameter to get (relative to the first parameter of the active page in the active bank)
  UserVariable *var;     // Variable to use to store value of parameter
};

class ParamSetSetParamEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(ParamSetSetParamEvent,T_EV_ParamSetSetParam);
  ParamSetSetParamEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    paramidx = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ParamSetSetParamEvent &s = (ParamSetSetParamEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    paramidx = s.paramidx;
    value = s.value;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("paramidx",FWEELIN_GETOFS(paramidx),T_int);
    case 3:
      return EventParameter("value",FWEELIN_GETOFS(value),T_float);
    }

    return EventParameter();
  };

  int interfaceid, // Interface in which parameter set display is defined
    displayid,     // Display ID of parameter set display
    paramidx;      // Index of parameter to set (relative to the first parameter of the active page in the active bank)
  float value;     // Value to store in parameter
};

class LogFaderVolToLinearEvent : public Event {
 public:
  EVT_DEFINE(LogFaderVolToLinearEvent,T_EV_LogFaderVolToLinear);
  virtual void Recycle() {
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    LogFaderVolToLinearEvent &s = (LogFaderVolToLinearEvent &) src;
    var = s.var;
    fadervol.type = s.fadervol.type;
    fadervol = s.fadervol;
    scale = s.scale;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variableref);
    case 1:
      return EventParameter("fadervol",FWEELIN_GETOFS(fadervol),T_variable);
    case 2:
      return EventParameter("scale",FWEELIN_GETOFS(scale),T_float);
    }

    return EventParameter();
  };

  UserVariable *var;     // Variable to store converted fadervol
  UserVariable fadervol; // Fader volume to convert to linear
  float scale;           // Scaling factor for linear output - ie 0.0dB on the
                         // fader becomes 'scale'. For example, if scale is 16384,
                         // 0.0dB on the fader is 16384 and 6.0dB on the fader is
                         // roughly 32768. Note that fadervol is not actually
                         // a dB value, but a 'fadervol', which is a value between
                         // 0 and 1 which corresponds to the throw of a
                         // log volume fader. Please see AudioLevel class.
};

class ALSAMixerControlSetEvent : public Event {
 public:
  EVT_DEFINE(ALSAMixerControlSetEvent,T_EV_ALSAMixerControlSet);
  virtual void Recycle() {
    hwid = 0;
    numid = -1;
    val1 = -1;
    val2 = -1;
    val3 = -1;
    val4 = -1;

    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ALSAMixerControlSetEvent &s = (ALSAMixerControlSetEvent &) src;
    hwid = s.hwid;
    numid = s.numid;
    val1 = s.val1;
    val2 = s.val2;
    val3 = s.val3;
    val4 = s.val4;
  };
  virtual int GetNumParams() { return 6; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter("hwid",FWEELIN_GETOFS(hwid),T_int);
    case 1:
      return EventParameter("numid",FWEELIN_GETOFS(numid),T_int);
    case 2:
      return EventParameter("val1",FWEELIN_GETOFS(val1),T_int);
    case 3:
      return EventParameter("val2",FWEELIN_GETOFS(val2),T_int);
    case 4:
      return EventParameter("val3",FWEELIN_GETOFS(val3),T_int);
    case 5:
      return EventParameter("val4",FWEELIN_GETOFS(val4),T_int);
    }

    return EventParameter();
  };

  int hwid,   // Hardware interface ID for alsa (ie hwid=0 is hw:0)
    numid;    // ALSA mixer control numid (ie 'amixer cset numid=5')

  int val1, val2, val3, val4; // Values to set (up to 4, leave blank for fewer)
};

class VideoShowLoopEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(VideoShowLoopEvent,T_EV_VideoShowLoop);
  VideoShowLoopEvent() : loopid(0,0) { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    layoutid = 0;
    loopid = Range(0,0);
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowLoopEvent &s = (VideoShowLoopEvent &) src;
    interfaceid = s.interfaceid;
    layoutid = s.layoutid;
    loopid = s.loopid;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("layoutid",FWEELIN_GETOFS(layoutid),T_int);
    case 2:
      return EventParameter("loopid",FWEELIN_GETOFS(loopid),T_range);
    }

    return EventParameter();
  };    

  int interfaceid, // Interface in which layout is defined
    layoutid;      // Layout in which to show loops
  Range loopid;    // Range of loop IDs that will be set to correspond to 
                   // elements in layout
};

class VideoShowSnapshotPageEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(VideoShowSnapshotPageEvent,T_EV_VideoShowSnapshotPage);
  VideoShowSnapshotPageEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    page = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowSnapshotPageEvent &s = (VideoShowSnapshotPageEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    page = s.page;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("page",FWEELIN_GETOFS(page),T_int);
    }

    return EventParameter();
  };    

  int interfaceid, // Interface in which snapshot display is defined
    displayid,     // Display ID of snapshot display
    page;          // +1 (next page) or -1 (previous page)
};

class VideoShowParamSetBankEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(VideoShowParamSetBankEvent,T_EV_VideoShowParamSetBank);
  VideoShowParamSetBankEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    bank = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowParamSetBankEvent &s = (VideoShowParamSetBankEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    bank = s.bank;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("bank",FWEELIN_GETOFS(bank),T_int);
    }

    return EventParameter();
  };

  int interfaceid, // Interface in which parameter set display is defined
    displayid,     // Display ID of parameter set display
    bank;          // +1 (next bank of parameters) or -1 (previous bank)
};

class VideoShowParamSetPageEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(VideoShowParamSetPageEvent,T_EV_VideoShowParamSetPage);
  VideoShowParamSetPageEvent() { Recycle(); };
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    page = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowParamSetPageEvent &s = (VideoShowParamSetPageEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    page = s.page;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) {
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("page",FWEELIN_GETOFS(page),T_int);
    }

    return EventParameter();
  };

  int interfaceid, // Interface in which parameter set display is defined
    displayid,     // Display ID of parameter set display
    page;          // +1 (next page of parameters) or -1 (previous page)
};

class VideoShowLayoutEvent : public Event {
 public:
  EVT_DEFINE(VideoShowLayoutEvent,T_EV_VideoShowLayout);
  virtual void Recycle() {
    interfaceid = -1;
    layoutid = 0;
    show = 0;
    hideothers = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowLayoutEvent &s = (VideoShowLayoutEvent &) src;
    interfaceid = s.interfaceid;
    layoutid = s.layoutid;
    show = s.show;
    hideothers = s.hideothers;
  };
  virtual int GetNumParams() { return 4; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("layoutid",FWEELIN_GETOFS(layoutid),T_int);
    case 2:
      return EventParameter("show",FWEELIN_GETOFS(show),T_char);
    case 3:
      return EventParameter("hideothers",FWEELIN_GETOFS(hideothers),T_char);
    }

    return EventParameter();
  };    

  int interfaceid, // Interface in which layout is defined
    layoutid;      // Layout to show
  char show,       // Show it or hide it?
    hideothers;    // Hide other layouts?
};

class VideoSwitchInterfaceEvent : public Event {
 public:
  EVT_DEFINE(VideoSwitchInterfaceEvent,T_EV_VideoSwitchInterface);
  virtual void Recycle() {
    interfaceid = -1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoSwitchInterfaceEvent &s = (VideoSwitchInterfaceEvent &) src;
    interfaceid = s.interfaceid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    }

    return EventParameter();
  };    

  int interfaceid; // Interface to switch to
};

class VideoShowDisplayEvent : public Event {
 public:
  EVT_DEFINE(VideoShowDisplayEvent,T_EV_VideoShowDisplay);
  virtual void Recycle() {
    interfaceid = -1;
    displayid = 0;
    show = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowDisplayEvent &s = (VideoShowDisplayEvent &) src;
    interfaceid = s.interfaceid;
    displayid = s.displayid;
    show = s.show;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    case 1:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 2:
      return EventParameter("show",FWEELIN_GETOFS(show),T_char);
    }

    return EventParameter();
  };    

  int interfaceid, // Interface in which display is defined
    displayid;     // Display to show
  char show;       // Show it or hide it?
};

class ShowDebugInfoEvent : public Event {
 public:
  EVT_DEFINE(ShowDebugInfoEvent,T_EV_ShowDebugInfo);
  virtual void Recycle() {
    show = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    ShowDebugInfoEvent &s = (ShowDebugInfoEvent &) src;
    show = s.show;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("show",FWEELIN_GETOFS(show),T_char);
    }

    return EventParameter();
  };    

  char show;       // Show debugging info onscreen?
};

class VideoShowHelpEvent : public Event {
 public:
  EVT_DEFINE(VideoShowHelpEvent,T_EV_VideoShowHelp);
  virtual void Recycle() {
    page = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowHelpEvent &s = (VideoShowHelpEvent &) src;
    page = s.page;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("page",FWEELIN_GETOFS(page),T_int);
    }

    return EventParameter();
  };    

  int page;       // Help page to show or 0 for no help
};

class VideoFullScreenEvent : public Event {
 public:
  EVT_DEFINE(VideoFullScreenEvent,T_EV_VideoFullScreen);
  virtual void Recycle() {
    fullscreen = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoFullScreenEvent &s = (VideoFullScreenEvent &) src;
    fullscreen = s.fullscreen;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("fullscreen",FWEELIN_GETOFS(fullscreen),
                            T_char);
    }

    return EventParameter();
  };    

  char fullscreen;       // Freewheeling is full screen or in a window?
};

class StartSessionEvent : public Event {
 public:
  EVT_DEFINE(StartSessionEvent,T_EV_StartSession);
};

class StartInterfaceEvent : public Event {
 public:
  EVT_DEFINE(StartInterfaceEvent,T_EV_StartInterface);
  virtual void Recycle() {
    interfaceid = -1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    StartInterfaceEvent &s = (StartInterfaceEvent &) src;
    interfaceid = s.interfaceid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter(INTERFACEID,FWEELIN_GETOFS(interfaceid),T_int);
    }

    return EventParameter();
  };    

  int interfaceid; // Interface to start
};

class ExitSessionEvent : public Event {
 public:
  EVT_DEFINE(ExitSessionEvent,T_EV_ExitSession);
};

class SlideMasterInVolumeEvent : public Event {
 public:
  EVT_DEFINE(SlideMasterInVolumeEvent,T_EV_SlideMasterInVolume);
  virtual void operator = (const Event &src) {
    SlideMasterInVolumeEvent &s = (SlideMasterInVolumeEvent &) src;
    slide = s.slide;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("slide",FWEELIN_GETOFS(slide),T_float);
    }

    return EventParameter();
  };    

  float slide; // Change in speed of amplitude slide
};

class SlideMasterOutVolumeEvent : public Event {
 public:
  EVT_DEFINE(SlideMasterOutVolumeEvent,T_EV_SlideMasterOutVolume);
  virtual void operator = (const Event &src) {
    SlideMasterOutVolumeEvent &s = (SlideMasterOutVolumeEvent &) src;
    slide = s.slide;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("slide",FWEELIN_GETOFS(slide),T_float);
    }

    return EventParameter();
  };    

  float slide; // Change in speed of amplitude slide
};

class SlideInVolumeEvent : public Event {
 public:
  EVT_DEFINE(SlideInVolumeEvent,T_EV_SlideInVolume);
  virtual void operator = (const Event &src) {
    SlideInVolumeEvent &s = (SlideInVolumeEvent &) src;
    input = s.input;
    slide = s.slide;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("input",FWEELIN_GETOFS(input),T_int);
    case 1:
      return EventParameter("slide",FWEELIN_GETOFS(slide),T_float);
    }

    return EventParameter();
  };    

  int input;   // Number of input to change volume for
  float slide; // Change in speed of amplitude slide
};

class SetMasterInVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetMasterInVolumeEvent,T_EV_SetMasterInVolume);
  virtual void Recycle() {
    vol = -1.;
    fadervol = -1.;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetMasterInVolumeEvent &s = (SetMasterInVolumeEvent &) src;
    vol = s.vol;
    fadervol = s.fadervol;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    case 1:
      return EventParameter("fadervol",FWEELIN_GETOFS(fadervol),T_float);
    }

    return EventParameter();
  };    

  float vol, // Linear volume to set
    fadervol; // Logarithmic volume to set (by fader throw)
};

class SetMasterOutVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetMasterOutVolumeEvent,T_EV_SetMasterOutVolume);
  virtual void Recycle() {
    vol = -1.;
    fadervol = -1.;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetMasterOutVolumeEvent &s = (SetMasterOutVolumeEvent &) src;
    vol = s.vol;
    fadervol = s.fadervol;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    case 1:
      return EventParameter("fadervol",FWEELIN_GETOFS(fadervol),T_float);
    }

    return EventParameter();
  };    

  float vol,  // Linear volume to set
    fadervol; // Logarithmic volume to set (by fader throw)
};

class SetInVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetInVolumeEvent,T_EV_SetInVolume);
  virtual void Recycle() {
    input = 1;
    vol = -1.;
    fadervol = -1.;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetInVolumeEvent &s = (SetInVolumeEvent &) src;
    input = s.input;
    vol = s.vol;
    fadervol = s.fadervol;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("input",FWEELIN_GETOFS(input),T_int);
    case 1:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    case 2:
      return EventParameter("fadervol",FWEELIN_GETOFS(fadervol),T_float);
    }

    return EventParameter();
  };    

  int input;  // Number of input to change volume for
  float vol,  // Linear volume to set
    fadervol; // Logarithmic volume to set (by fader throw)
};

class ToggleInputRecordEvent : public Event {
 public:
  EVT_DEFINE(ToggleInputRecordEvent,T_EV_ToggleInputRecord);
  virtual void operator = (const Event &src) {
    ToggleInputRecordEvent &s = (ToggleInputRecordEvent &) src;
    input = s.input;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("input",FWEELIN_GETOFS(input),T_int);
    }

    return EventParameter();
  };    

  int input; // Number of input to toggle for recording
};

class SetMidiEchoPortEvent : public Event {
 public:
  EVT_DEFINE(SetMidiEchoPortEvent,T_EV_SetMidiEchoPort);
  virtual void operator = (const Event &src) {
    SetMidiEchoPortEvent &s = (SetMidiEchoPortEvent &) src;
    echoport = s.echoport;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("echoport",FWEELIN_GETOFS(echoport),T_int);
    }

    return EventParameter();
  };    

  int echoport; // Port # to echo MIDI events to or 0 for disable echo
};

class SetMidiEchoChannelEvent : public Event {
 public:
  EVT_DEFINE(SetMidiEchoChannelEvent,T_EV_SetMidiEchoChannel);
  virtual void operator = (const Event &src) {
    SetMidiEchoChannelEvent &s = (SetMidiEchoChannelEvent &) src;
    echochannel = s.echochannel;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("echochannel",FWEELIN_GETOFS(echochannel),T_int);
    }

    return EventParameter();
  };    

  int echochannel; // Override MIDI channel for MIDI event echo 
                   // (-1 to leave as is)
};

class AdjustMidiTransposeEvent : public Event {
 public:
  EVT_DEFINE(AdjustMidiTransposeEvent,T_EV_AdjustMidiTranspose);
  virtual void operator = (const Event &src) {
    AdjustMidiTransposeEvent &s = (AdjustMidiTransposeEvent &) src;
    adjust = s.adjust;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("adjust",FWEELIN_GETOFS(adjust),T_int);
    }

    return EventParameter();
  };    

  int adjust; // Number of semitones to add to MIDI transpose
};

class FluidSynthEnableEvent : public Event {
 public:
  EVT_DEFINE(FluidSynthEnableEvent,T_EV_FluidSynthEnable);
  virtual void operator = (const Event &src) {
    FluidSynthEnableEvent &s = (FluidSynthEnableEvent &) src;
    enable = s.enable;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("enable",FWEELIN_GETOFS(enable),T_char);
    }

    return EventParameter();
  };    

  char enable; // FluidSynth enabled?
};

class SetMidiTuningEvent : public Event {
 public:
  EVT_DEFINE(SetMidiTuningEvent,T_EV_SetMidiTuning);
  virtual void Recycle() {
    tuning = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetMidiTuningEvent &s = (SetMidiTuningEvent &) src;
    tuning = s.tuning;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("tuning",FWEELIN_GETOFS(tuning),T_int);
    }

    return EventParameter();
  };    

  int tuning; // New offset of 0 position for pitch bender- 
              // shifts whole pitch bend by this value
};

class SetTriggerVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetTriggerVolumeEvent,T_EV_SetTriggerVolume);
  virtual void operator = (const Event &src) {
    SetTriggerVolumeEvent &s = (SetTriggerVolumeEvent &) src;
    index = s.index;
    vol = s.vol;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    }

    return EventParameter();
  };    

  int index;   // Index of loop to set trigger volume for
  float vol;   // New trigger volume
};

class SlideLoopAmpEvent : public Event {
 public:
  EVT_DEFINE(SlideLoopAmpEvent,T_EV_SlideLoopAmp);
  virtual void operator = (const Event &src) {
    SlideLoopAmpEvent &s = (SlideLoopAmpEvent &) src;
    index = s.index;
    slide = s.slide;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("slide",FWEELIN_GETOFS(slide),T_float);
    }

    return EventParameter();
  };    

  int index;   // Index of loop to slide amplitude for
  float slide; // Change in speed of slide
};

class SetLoopAmpEvent : public Event {
 public:
  EVT_DEFINE(SetLoopAmpEvent,T_EV_SetLoopAmp);
  virtual void operator = (const Event &src) {
    SetLoopAmpEvent &s = (SetLoopAmpEvent &) src;
    index = s.index;
    amp = s.amp;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("amp",FWEELIN_GETOFS(amp),T_float);
    }

    return EventParameter();
  };    

  int index; // Index of loop to set amplitude for
  float amp; // Amplitude to set loop at
};

class AdjustLoopAmpEvent : public Event {
 public:
  EVT_DEFINE(AdjustLoopAmpEvent,T_EV_AdjustLoopAmp);
  virtual void operator = (const Event &src) {
    AdjustLoopAmpEvent &s = (AdjustLoopAmpEvent &) src;
    index = s.index;
    ampfactor = s.ampfactor;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("ampfactor",FWEELIN_GETOFS(ampfactor),T_float);
    }

    return EventParameter();
  };    

  int index;  // Index of loop to adjust amplitude for
  float ampfactor; // Factor to multiply loop amplitudesby
};

class TriggerLoopEvent : public Event {
 public:
  EVT_DEFINE(TriggerLoopEvent,T_EV_TriggerLoop);
  virtual void Recycle() {
    index = 0;
    vol = 1.0;
    engage = -1;
    shot = 0;
    od = 0;
    od_fb = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    TriggerLoopEvent &s = (TriggerLoopEvent &) src;
    index = s.index;
    vol = s.vol;
    engage = s.engage;
    shot = s.shot;
    od = s.od;
    od_fb = s.od_fb;
  };
  virtual int GetNumParams() { return 6; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("engage",FWEELIN_GETOFS(engage),T_int);
    case 2:
      return EventParameter("shot",FWEELIN_GETOFS(shot),T_char);      
    case 3:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    case 4:
      return EventParameter("overdub",FWEELIN_GETOFS(od),T_char);
    case 5:
      return EventParameter("overdubfeedback",FWEELIN_GETOFS(od_fb),T_variableref);
    }

    return EventParameter();
  };    

  int index;   // Index of loop
  int engage;  // -1 is the default behavior, where each trigger-loop
               // event toggles between rec/play and off
               // 0 forces the loop to off
               // 1 forces the loop to on
  char shot;   // Nonzero if we should only play thru once- no loop
  float vol;   // Volume of trigger
  char od;     // Nonzero if we should trigger an overdub
  UserVariable *od_fb;  // Variable which holds overdub feedback- can be continuously varied
};

class MoveLoopEvent : public Event {
 public:
  EVT_DEFINE(MoveLoopEvent,T_EV_MoveLoop);
  virtual void operator = (const Event &src) {
    MoveLoopEvent &s = (MoveLoopEvent &) src;
    oldloopid = s.oldloopid;
    newloopid = s.newloopid;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("oldloopid",FWEELIN_GETOFS(oldloopid),T_int);
    case 1:
      return EventParameter("newloopid",FWEELIN_GETOFS(newloopid),T_int);
    }

    return EventParameter();
  };    

  int oldloopid, // Old index of loop
    newloopid; // New index of loop
};

class EraseLoopEvent : public Event {
 public:
  EVT_DEFINE(EraseLoopEvent,T_EV_EraseLoop);
  virtual void operator = (const Event &src) {
    EraseLoopEvent &s = (EraseLoopEvent &) src;
    index = s.index;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    }

    return EventParameter();
  };    

  int index; // Index of loop
};

class EraseAllLoopsEvent : public Event {
 public:
  EVT_DEFINE(EraseAllLoopsEvent,T_EV_EraseAllLoops);
};

class EraseSelectedLoopsEvent : public Event {
 public:
  EVT_DEFINE(EraseSelectedLoopsEvent,T_EV_EraseSelectedLoops);
  virtual void Recycle() {
    setid = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    EraseSelectedLoopsEvent &s = 
      (EraseSelectedLoopsEvent &) src;
    setid = s.setid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    }

    return EventParameter();
  };    

  int setid; // ID # of the selection set to work on
};

class SlideLoopAmpStopAllEvent : public Event {
 public:
  EVT_DEFINE(SlideLoopAmpStopAllEvent,T_EV_SlideLoopAmpStopAll);
};

class ToggleDiskOutputEvent : public Event {
 public:
  EVT_DEFINE(ToggleDiskOutputEvent,T_EV_ToggleDiskOutput);
};

class SetAutoLoopSavingEvent : public Event {
 public:
  EVT_DEFINE(SetAutoLoopSavingEvent,T_EV_SetAutoLoopSaving);
  virtual void operator = (const Event &src) {
    SetAutoLoopSavingEvent &s = (SetAutoLoopSavingEvent &) src;
    save = s.save;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("save",FWEELIN_GETOFS(save),T_char);
    }

    return EventParameter();
  };    

  char save; // Are we autosaving loops?
};

class SaveLoopEvent : public Event {
 public:
  EVT_DEFINE(SaveLoopEvent,T_EV_SaveLoop);
  virtual void operator = (const Event &src) {
    SaveLoopEvent &s = (SaveLoopEvent &) src;
    index = s.index;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    }

    return EventParameter();
  };    

  int index; // Index of loop to save
};

class SaveNewSceneEvent : public Event {
 public:
  EVT_DEFINE(SaveNewSceneEvent,T_EV_SaveNewScene);
};

class SaveCurrentSceneEvent : public Event {
public:
  EVT_DEFINE(SaveCurrentSceneEvent,T_EV_SaveCurrentScene);
};

class SetLoadLoopIdEvent : public Event {
 public:
  EVT_DEFINE(SetLoadLoopIdEvent,T_EV_SetLoadLoopId);
  virtual void operator = (const Event &src) {
    SetLoadLoopIdEvent &s = (SetLoadLoopIdEvent &) src;
    index = s.index;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    }

    return EventParameter();
  };    

  int index; // Index to load loops into
};

class SetDefaultLoopPlacementEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(SetDefaultLoopPlacementEvent,
                       T_EV_SetDefaultLoopPlacement);
  SetDefaultLoopPlacementEvent() : looprange(0,0) { Recycle(); };
  virtual void Recycle() {
    looprange = Range(0,0);
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetDefaultLoopPlacementEvent &s = (SetDefaultLoopPlacementEvent &) src;
    looprange = s.looprange;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("looprange",FWEELIN_GETOFS(looprange),T_range);
    }

    return EventParameter();
  };    

  Range looprange; // Range of loop IDs to be used when others are full
};

class BrowserMoveToItemEvent : public Event {
 public:
  EVT_DEFINE(BrowserMoveToItemEvent,T_EV_BrowserMoveToItem);
  virtual void Recycle() {
    browserid = -1;
    adjust = 0;
    jumpadjust = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    BrowserMoveToItemEvent &s = (BrowserMoveToItemEvent &) src;
    browserid = s.browserid;
    adjust = s.adjust;
    jumpadjust = s.jumpadjust;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("browserid",FWEELIN_GETOFS(browserid),T_int);
    case 1:
      return EventParameter("adjust",FWEELIN_GETOFS(adjust),T_int);
    case 2:
      return EventParameter("jumpadjust",FWEELIN_GETOFS(jumpadjust),T_int);
    }

    return EventParameter();
  };    

  int browserid;   // Display ID of browser
  int adjust;      // Move fwd/back by adjust items
  int jumpadjust;  // Jump fwd/back by jumpadjust divisions
};

class BrowserMoveToItemAbsoluteEvent : public Event {
 public:
  EVT_DEFINE(BrowserMoveToItemAbsoluteEvent,T_EV_BrowserMoveToItemAbsolute);
  virtual void Recycle() {
    browserid = -1;
    idx = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    BrowserMoveToItemAbsoluteEvent &s = (BrowserMoveToItemAbsoluteEvent &) src;
    browserid = s.browserid;
    idx = s.idx;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("browserid",FWEELIN_GETOFS(browserid),T_int);
    case 1:
      return EventParameter("idx",FWEELIN_GETOFS(idx),T_int);
    }

    return EventParameter();
  };    

  int browserid;   // Display ID of browser
  int idx;         // Index to move to (absolute from beginning of list)
};

class BrowserSelectItemEvent : public Event {
 public:
  EVT_DEFINE(BrowserSelectItemEvent,T_EV_BrowserSelectItem);
  virtual void Recycle() {
    browserid = -1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    BrowserSelectItemEvent &s = (BrowserSelectItemEvent &) src;
    browserid = s.browserid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("browserid",FWEELIN_GETOFS(browserid),T_int);
    }

    return EventParameter();
  };    

  int browserid;   // Display ID of browser
};

class BrowserRenameItemEvent : public Event {
 public:
  EVT_DEFINE(BrowserRenameItemEvent,T_EV_BrowserRenameItem);
  virtual void Recycle() {
    browserid = -1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    BrowserRenameItemEvent &s = (BrowserRenameItemEvent &) src;
    browserid = s.browserid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("browserid",FWEELIN_GETOFS(browserid),T_int);
    }

    return EventParameter();
  };    

  int browserid;   // Display ID of browser
};

class BrowserItemBrowsedEvent : public Event {
 public:
  EVT_DEFINE(BrowserItemBrowsedEvent,T_EV_BrowserItemBrowsed);
  virtual void Recycle() {
    browserid = -1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    BrowserItemBrowsedEvent &s = (BrowserItemBrowsedEvent &) src;
    browserid = s.browserid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("browserid",FWEELIN_GETOFS(browserid),T_int);
    }

    return EventParameter();
  };    

  int browserid;   // Display ID of browser
};

class PatchBrowserMoveToBankEvent : public Event {
 public:
  EVT_DEFINE(PatchBrowserMoveToBankEvent,T_EV_PatchBrowserMoveToBank);
  virtual void Recycle() {
    direction = 1;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    PatchBrowserMoveToBankEvent &s = (PatchBrowserMoveToBankEvent &) src;
    direction = s.direction;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("direction",FWEELIN_GETOFS(direction),T_int);
    }

    return EventParameter();
  };    

  int direction;   // Direction to move (+1/-1 next/previous)
};

class PatchBrowserMoveToBankByIndexEvent : public Event {
 public:
  EVT_DEFINE(PatchBrowserMoveToBankByIndexEvent,
             T_EV_PatchBrowserMoveToBankByIndex);
  virtual void Recycle() {
    index = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    PatchBrowserMoveToBankByIndexEvent &s = 
      (PatchBrowserMoveToBankByIndexEvent &) src;
    index = s.index;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("index",FWEELIN_GETOFS(index),T_int);
    }

    return EventParameter();
  };    

  int index; // Index of patchbank to choose
};

class RenameLoopEvent : public Event {
 public:
  EVT_DEFINE(RenameLoopEvent,T_EV_RenameLoop);
  virtual void Recycle() {
    loopid = 0;
    in = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    RenameLoopEvent &s = (RenameLoopEvent &) src;
    loopid = s.loopid;
    in = s.in;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(loopid),T_int);
    case 1:
      return EventParameter("in",FWEELIN_GETOFS(in),T_char);
    }

    return EventParameter();
  };    

  int loopid; // Loop ID to rename
  char in;    // Rename in loop tray (0) or layout (1)
};

class SelectPulseEvent : public Event {
 public:
  EVT_DEFINE(SelectPulseEvent,T_EV_SelectPulse);
  virtual void operator = (const Event &src) {
    SelectPulseEvent &s = (SelectPulseEvent &) src;
    pulse = s.pulse;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("pulse",FWEELIN_GETOFS(pulse),T_int);
    }

    return EventParameter();
  };    

  int pulse; // Index of pulse
};

class DeletePulseEvent : public Event {
 public:
  EVT_DEFINE(DeletePulseEvent,T_EV_DeletePulse);
  virtual void operator = (const Event &src) {
    DeletePulseEvent &s = (DeletePulseEvent &) src;
    pulse = s.pulse;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("pulse",FWEELIN_GETOFS(pulse),T_int);
    }

    return EventParameter();
  };    

  int pulse; // Index of pulse
};

class TapPulseEvent : public Event {
 public:
  EVT_DEFINE(TapPulseEvent,T_EV_TapPulse);
  virtual void operator = (const Event &src) {
    TapPulseEvent &s = (TapPulseEvent &) src;
    pulse = s.pulse;
    newlen = s.newlen;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("pulse",FWEELIN_GETOFS(pulse),T_int);
    case 1:
      return EventParameter("newlen",FWEELIN_GETOFS(newlen),T_char);
    }

    return EventParameter();
  };    

  int pulse;   // Index of pulse
  char newlen; // Nonzero to redefine pulse length or create new pulse
};

class SwitchMetronomeEvent : public Event {
 public:
  EVT_DEFINE(SwitchMetronomeEvent,T_EV_SwitchMetronome);
  virtual void operator = (const Event &src) {
    SwitchMetronomeEvent &s = (SwitchMetronomeEvent &) src;
    pulse = s.pulse;
    metronome = s.metronome;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("pulse",FWEELIN_GETOFS(pulse),T_int);
    case 1:
      return EventParameter("metronome",FWEELIN_GETOFS(metronome),T_char);
    }

    return EventParameter();
  };    

  int pulse;      // Index of pulse
  char metronome; // Metronome active?
};

class SetSyncTypeEvent : public Event {
 public:
  EVT_DEFINE(SetSyncTypeEvent,T_EV_SetSyncType);
  virtual void operator = (const Event &src) {
    SetSyncTypeEvent &s = (SetSyncTypeEvent &) src;
    stype = s.stype;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("type",FWEELIN_GETOFS(stype),T_char);
    }

    return EventParameter();
  };    

  char stype; // Nonzero for beat sync, zero for bar sync
};

class SetSyncSpeedEvent : public Event {
 public:
  EVT_DEFINE(SetSyncSpeedEvent,T_EV_SetSyncSpeed);
  virtual void operator = (const Event &src) {
    SetSyncSpeedEvent &s = (SetSyncSpeedEvent &) src;
    sspd = s.sspd;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("speed",FWEELIN_GETOFS(sspd),T_int);
    }

    return EventParameter();
  };    

  int sspd; // Number of external transport beats/bars per internal pulse
};

class SetMidiSyncEvent : public Event {
public:
  EVT_DEFINE(SetMidiSyncEvent,T_EV_SetMidiSync);
  virtual void operator = (const Event &src) {
    SetMidiSyncEvent &s = (SetMidiSyncEvent &) src;
    midisync = s.midisync;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("midisync",FWEELIN_GETOFS(midisync),T_int);
    }
    
    return EventParameter();
  };    
  
  int midisync; // Nonzero to transmit MIDI sync, zero for no MIDI sync
};

class ToggleSelectLoopEvent : public Event {
 public:
  EVT_DEFINE(ToggleSelectLoopEvent,T_EV_ToggleSelectLoop);
  virtual void operator = (const Event &src) {
    ToggleSelectLoopEvent &s = (ToggleSelectLoopEvent &) src;
    setid = s.setid;
    loopid = s.loopid;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("loopid",FWEELIN_GETOFS(loopid),T_int);
    }

    return EventParameter();
  };    

  int setid, // ID # of the selection set in which to toggle the loop
    loopid;  // ID # of loop to toggle
};

class SelectOnlyPlayingLoopsEvent : public Event {
 public:
  EVT_DEFINE(SelectOnlyPlayingLoopsEvent,T_EV_SelectOnlyPlayingLoops);
  virtual void operator = (const Event &src) {
    SelectOnlyPlayingLoopsEvent &s = (SelectOnlyPlayingLoopsEvent &) src;
    setid = s.setid;
    playing = s.playing;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("playing",FWEELIN_GETOFS(playing),T_char);
    }

    return EventParameter();
  };    

  int setid; // ID # of the selection set to work on
  char playing; // Nonzero if we should select only those loops currently 
                // playing-- zero if we should select only those loops 
                // currently idle
};

class SelectAllLoopsEvent : public Event {
 public:
  EVT_DEFINE(SelectAllLoopsEvent,T_EV_SelectAllLoops);
  virtual void operator = (const Event &src) {
    SelectAllLoopsEvent &s = (SelectAllLoopsEvent &) src;
    setid = s.setid;
    select = s.select;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("select",FWEELIN_GETOFS(select),T_char);
    }

    return EventParameter();
  };    

  int setid; // ID # of the selection set to work on
  char select; // Nonzero to select/zero to unselect all loops
};

class InvertSelectionEvent : public Event {
public:
  EVT_DEFINE(InvertSelectionEvent,T_EV_InvertSelection);
  virtual void operator = (const Event &src) {
    InvertSelectionEvent &s = (InvertSelectionEvent &) src;
    setid = s.setid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    }
    
    return EventParameter();
  };    
  
  int setid; // ID # of the selection set to invert (select all other loops)
};

class CreateSnapshotEvent : public Event {
public:
  EVT_DEFINE(CreateSnapshotEvent,T_EV_CreateSnapshot);
  virtual void Recycle() {
    snapid = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    CreateSnapshotEvent &s = (CreateSnapshotEvent &) src;
    snapid = s.snapid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("snapid",FWEELIN_GETOFS(snapid),T_int);
    }
    
    return EventParameter();
  };    
  
  int snapid; // Create and store snapshot #snapid
};

class SwapSnapshotsEvent : public Event {
public:
  EVT_DEFINE(SwapSnapshotsEvent,T_EV_SwapSnapshots);
  virtual void Recycle() {
    snapid1 = 0;
    snapid2 = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SwapSnapshotsEvent &s = (SwapSnapshotsEvent &) src;
    snapid1 = s.snapid1;
    snapid2 = s.snapid2;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("snapid1",FWEELIN_GETOFS(snapid1),T_int);
      case 1:
        return EventParameter("snapid2",FWEELIN_GETOFS(snapid2),T_int);
    }
    
    return EventParameter();
  };    
  
  int snapid1, snapid2; // Swap these two snapshots 
};

class RenameSnapshotEvent : public Event {
public:
  EVT_DEFINE(RenameSnapshotEvent,T_EV_RenameSnapshot);
  virtual void Recycle() {
    snapid = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    RenameSnapshotEvent &s = (RenameSnapshotEvent &) src;
    snapid = s.snapid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("snapid",FWEELIN_GETOFS(snapid),T_int);
    }
    
    return EventParameter();
  };    
  
  int snapid; // Rename snapshot #snapid
};

class TriggerSnapshotEvent : public Event {
public:
  EVT_DEFINE(TriggerSnapshotEvent,T_EV_TriggerSnapshot);
  virtual void Recycle() {
    snapid = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    TriggerSnapshotEvent &s = (TriggerSnapshotEvent &) src;
    snapid = s.snapid;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
      case 0:
        return EventParameter("snapid",FWEELIN_GETOFS(snapid),T_int);
    }
    
    return EventParameter();
  };    
  
  int snapid; // Trigger snapshot #snapid
};

class TriggerSelectedLoopsEvent : public Event {
 public:
  EVT_DEFINE(TriggerSelectedLoopsEvent,T_EV_TriggerSelectedLoops);
  virtual void Recycle() {
    setid = 0;
    vol = 1.0;
    toggleloops = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    TriggerSelectedLoopsEvent &s = (TriggerSelectedLoopsEvent &) src;
    setid = s.setid;
    vol = s.vol;
    toggleloops = s.toggleloops;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    case 2:
      return EventParameter("toggleloops",FWEELIN_GETOFS(toggleloops),T_char);
    }

    return EventParameter();
  };    

  int setid; // ID # of the selection set to work on
  float vol; // Volume at which to trigger selected loops
  char toggleloops; // Nonzero to toggle loops, zero to force active
};

class SetSelectedLoopsTriggerVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetSelectedLoopsTriggerVolumeEvent,
             T_EV_SetSelectedLoopsTriggerVolume);
  virtual void Recycle() {
    setid = 0;
    vol = 1.0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    SetSelectedLoopsTriggerVolumeEvent &s = 
      (SetSelectedLoopsTriggerVolumeEvent &) src;
    setid = s.setid;
    vol = s.vol;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    }

    return EventParameter();
  };    

  int setid; // ID # of the selection set to work on
  float vol; // Trigger volume to set for loops
};

class AdjustSelectedLoopsAmpEvent : public Event {
 public:
  EVT_DEFINE(AdjustSelectedLoopsAmpEvent,
             T_EV_AdjustSelectedLoopsAmp);
  virtual void Recycle() {
    setid = 0;
    ampfactor = 1.0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    AdjustSelectedLoopsAmpEvent &s = 
      (AdjustSelectedLoopsAmpEvent &) src;
    setid = s.setid;
    ampfactor = s.ampfactor;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("setid",FWEELIN_GETOFS(setid),T_int);
    case 1:
      return EventParameter("ampfactor",FWEELIN_GETOFS(ampfactor),T_float);
    }

    return EventParameter();
  };    

  int setid;       // ID # of the selection set to work on
  float ampfactor; // Factor to multiply all loop amplitudes by
};

class EndRecordEvent : public Event {
 public:   
  EVT_DEFINE(EndRecordEvent,T_EV_EndRecord);

  // Nonzero if recording should be kept
  char keeprecord;
};

// List refers to loops in memory or on disk- used by background save/load
class LoopListEvent : public Event {
 public:
  EVT_DEFINE(LoopListEvent,T_EV_LoopList);
  virtual void Recycle() {
    l = 0;
    strcpy(l_filename,"");
    l_idx = 0;
    l_vol = 1.0;
    Event::Recycle();
  };

  Loop *l;          // Loop 
                    // Filename of loop on disk 
                    // (note we have to copy the string)
  char l_filename[FWEELIN_OUTNAME_LEN]; 
  int l_idx;        // Index of loop in map
  float l_vol;      // Volume to set loop to when loading
};

class SceneMarkerEvent : public Event {
 public:   
  EVT_DEFINE(SceneMarkerEvent,T_EV_SceneMarker);
  virtual void Recycle() {
    strcpy(s_filename,"");
    Event::Recycle();
  };
  
  char s_filename[FWEELIN_OUTNAME_LEN]; // Filename of scene on disk
};

// This event is broadcast once on the downbeat of each pulse
class PulseSyncEvent : public Event {
 public:   
  EVT_DEFINE(PulseSyncEvent,T_EV_PulseSync);
};

// TriggerSet is fired by TriggerMap whenever a change is made to the map
// The LoopTray and potentially others listen for changes to the TriggerMap
class TriggerSetEvent : public Event {
 public:   
  EVT_DEFINE(TriggerSetEvent,T_EV_TriggerSet);

  int idx;  // On index 'idx'
  Loop *nw; // ..we now have 'nw'
};

// This event is added internally to RootProcessor's RT queue whenever a child processor should be
// added to RootProcessor (ie recording/playing a loop)
class AddProcessorEvent : public Event {
 public:
  EVT_DEFINE(AddProcessorEvent,T_EV_AddProcessor);

  ProcessorItem *new_processor; // Pointer to new processor item instance
};

// This event is added internally to RootProcessor's RT queue whenever a child processor should be
// deleted from RootProcessor (ie stop play/record/overdub)
class DelProcessorEvent : public Event {
 public:
  EVT_DEFINE(DelProcessorEvent,T_EV_DelProcessor);

  ProcessorItem *processor; // ProcessorItem which holds processor to remove
};

// This event is called when a Processor's memory should be freed
class CleanupProcessorEvent : public Event {
 public:
  EVT_DEFINE(CleanupProcessorEvent,T_EV_CleanupProcessor);

  ProcessorItem *processor; // ProcessorItem which holds processor to cleanup
};

// TransmitPlayingLoopsToDAW can be used to send all playing
// loops to a connected DAW via OSC
// Only saved loops will be sent
class TransmitPlayingLoopsToDAWEvent : public Event {
 public:
  EVT_DEFINE(TransmitPlayingLoopsToDAWEvent,T_EV_TransmitPlayingLoopsToDAW);
};

// Consider making EventListenerItem a Preallocated type to improve performance
// when adding/removing listeners-- this will happen more often now that
// PulseSyncEvent and other BMG type tasks are being merged into EMG
class EventListenerItem {
 public:
  EventListenerItem(EventListener *callwhom, 
                    EventProducer *eventsfrom,
                    EventType oftype, char block_self_calls) :
    callwhom(callwhom), eventsfrom(eventsfrom), oftype(oftype),
    block_self_calls(block_self_calls), next(0) {};

  // Call this listener..
  EventListener *callwhom;
  // When events from this event producer..
  EventProducer *eventsfrom;
  // And this type.. are produced
  EventType oftype;

  // If nonzero, this flag stops a producer's events from calling itself
  // if that producer is also a listener
  char block_self_calls;

  // Pointer to next list item
  EventListenerItem *next;
};

// EventManager manages generic event types
// Allows for sending & receiving of events between different
// parts of the application-- which is useful because those
// parts can then be loosely coupled through extensible modular event 
// types
//
// Events can be sent immediately in the same thread they are broadcast
// Or they can be sent through a dispatch thread
//
// Events can be sent through RT safe methods.. the dispatch is
// optimized by event type, to eliminate lengthy searches for
// eventlisteners
class EventManager {
 public:
  EventManager();
  ~EventManager();

  // Create ring buffers after ALL writer threads are created
  void FinalPrep();

  // Event queue functions ** NO THREAD PROTECTION PROVIDED **
  // These are good for single threaded queue/dequeue operations

  static Event *DeleteQueue(Event *first);
  static void QueueEvent(Event **first, Event *nw);
  static void RemoveEvent(Event **first, Event *prev, Event **cur);

  // Broadcast immediately (RT safe only if listeners' receive methods are RT safe - depends on event)!
  inline void BroadcastEventNow(Event *ev, 
                                EventProducer *source,
                                char allowslowdelivery = 1,
                                char deleteonsend = 1) {
    int evnum = (int) ev->GetType();
    
    // Check if this event is slow-delivery only
    if (allowslowdelivery && Event::ett[evnum].slowdelivery)
      BroadcastEvent(ev,source);
    else {
      // Scan through the listeners to see who to call
      EventListenerItem *cur = listeners[evnum];
      while (cur != 0) {
        if ((cur->eventsfrom == 0 && 
             (!cur->block_self_calls || source != (void *) cur->callwhom)) ||
            source == 0 || cur->eventsfrom == source)
          cur->callwhom->ReceiveEvent(ev,source);
        cur = cur->next;
      }
      
      // This event has been broadcast.. erase it!.. use RTDelete()
      if (deleteonsend)
        ev->RTDelete();
    }
  };

  // Broadcast through dispatch thread!
  // RT safe! -- so long as you allocate your event with RTNew()
  void BroadcastEvent(Event *ev, 
                      EventProducer *source);

  // Not RT safe, but threadsafe!
  // Listen for the given event (optionally from the given producer) and callme
  // when it occurs-- optionally, block calls from myself
  void ListenEvent(EventListener *callme, 
                   EventProducer *from, EventType type, 
                   char block_self_calls = 0);

  // Not RT safe, but threadsafe!
  void UnlistenEvent(EventListener *callme,
                     EventProducer *from, EventType type);

  // Wakeup the event dispatch thread. Non blocking, RT safe.
  inline void WakeupIfNeeded(char always_wakeup = 0) {
    if (always_wakeup || needs_wakeup) {
      /* if (!always_wakeup)
        printf("EVENT: Woken because of priority inversion\n"); */

      // Wake up the dispatch thread
      if (pthread_mutex_trylock (&dispatch_thread_lock) == 0) {
        pthread_cond_signal (&dispatch_ready);
        pthread_mutex_unlock (&dispatch_thread_lock);
      }
      else {
        // Priority inversion - we are interrupting the event dispatch thread while it's processing eq
        // This is not an issue, because eq uses SRMWRingBuffer. However, we the event dispatch thread
        // may go to sleep, missing the new messages until it's woken again. So, set a flag and the RT audio
        // thread will wake it up next process cycle.

        if (always_wakeup) {
          // printf("EVENT: WARNING: Priority inversion during event broadcast!\n"); // ,Event::ett[(int) ev->GetType()].name);
          needs_wakeup = 1;
        }
      }
    }
  };

private:

  static void *run_dispatch_thread (void *ptr);

  // For each event type, we store a list of listeners..
  EventListenerItem **listeners;
  // Event queue- for calling listeners in lowpriority
  SRMWRingBuffer<Event *> *eq;

  volatile char needs_wakeup; // Event dispatch thread needs wakeup? (priority inversion)
  
  pthread_t dispatch_thread;
  pthread_mutex_t dispatch_thread_lock,
    listener_list_lock;
  pthread_cond_t  dispatch_ready;

  int threadgo;
  
  jack_ringbuffer_data_t asd;
};

#endif
