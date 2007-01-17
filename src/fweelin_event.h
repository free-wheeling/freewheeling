#ifndef __FWEELIN_EVENT_H
#define __FWEELIN_EVENT_H

#include <pthread.h>

#include <SDL/SDL.h>

class Loop;
class Event;

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
#define FWEELIN_OUTNAME_LEN 512

#define MAX_MIDI_CHANNELS 16
#define MAX_MIDI_CONTROLLERS 127
#define MAX_MIDI_NOTES 127
#define MAX_MIDI_PORTS 4

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
  T_EV_Input_MIDIPitchBend,
  T_EV_StartSession,
  T_EV_GoSub,

  T_EV_LoopClicked,
  T_EV_BrowserItemBrowsed,

  // End of bindable events-
  // Events after this can not trigger config bindings
  T_EV_Last_Bindable,

  T_EV_Input_MouseButton,
  T_EV_Input_MouseMotion,

  T_EV_EndRecord,
  T_EV_LoopList,
  T_EV_SceneMarker,
  T_EV_PulseSync,
  T_EV_TriggerSet,

  T_EV_SetVariable,
  T_EV_ToggleVariable,

  T_EV_VideoShowLoop,
  T_EV_VideoShowLayout,
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

  T_EV_ToggleSelectLoop,
  T_EV_SelectOnlyPlayingLoops,
  T_EV_SelectAllLoops,
  T_EV_TriggerSelectedLoops,
  T_EV_SetSelectedLoopsTriggerVolume,
  T_EV_InvertSelection,

  T_EV_SetTriggerVolume,
  T_EV_SlideLoopAmp,
  T_EV_SetLoopAmp,
  T_EV_AdjustLoopAmp,
  T_EV_TriggerLoop,
  T_EV_MoveLoop,
  T_EV_RenameLoop,
  T_EV_EraseLoop,
  T_EV_EraseAllLoops,
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
    name(name), mgr(mgr), proto(proto), paramidx(paramidx), 
    slowdelivery(slowdelivery) {};

  char *name;
  PreallocatedType *mgr;
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
    time = 0;
    echo = 0;
  };

  virtual void operator = (const Event &src) {};
  virtual EventType GetType() { return T_EV_None; };  
  // Returns the number of parameters this event has
  virtual int GetNumParams() { return 0; };
  // Returns the nth parameter
  virtual EventParameter GetParam(int n) { 
    return EventParameter();
  };

  // Get the memory manager for the given type
  static inline PreallocatedType *GetMemMgrByType(EventType typ) {
    return ett[(int) typ].mgr;
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
  // Timing of event (s)
  double time;
  // Event is an echo of an unhandled event?
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
  };
  virtual int GetNumParams() { return 4; };
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
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
    channel,   // MIDI channel
    ctrl,      // controller #
    val;       // value
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
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
			    MAX_MIDI_CHANNELS);
    case 2:
      return EventParameter("programval",FWEELIN_GETOFS(val),T_int);
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
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("outport",FWEELIN_GETOFS(outport),T_int);
    case 1:
      return EventParameter("midichannel",FWEELIN_GETOFS(channel),T_int,
			    MAX_MIDI_CHANNELS);
    case 2:
      return EventParameter("pitchval",FWEELIN_GETOFS(val),T_int);
    }

    return EventParameter();
  };    

  int outport, // # of MIDI output to send event to
    channel, // MIDI channel
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
  };
  virtual int GetNumParams() { return 5; };
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
    }

    return EventParameter();
  };    

  char down; // Nonzero if key is pressed, zero if key is released
  int outport, // # of MIDI output to send event to
    channel, // MIDI channel
    notenum,   // note number
    vel;       // velocity
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
    maxjump;	      // Maximum jump in variable between current value and new value
		      // Jumps beyond maxjump cause the variable not to be set
  char maxjumpcheck;  // Nonzero if we should check variable change against maxjump-
		      // If maxjumpcheck is zero, the variable is always set
};

class ToggleVariableEvent : public Event {
 public:
  EVT_DEFINE(ToggleVariableEvent,T_EV_ToggleVariable);
  virtual void operator = (const Event &src) {
    ToggleVariableEvent &s = (ToggleVariableEvent &) src;
    var = s.var;
    maxvalue = s.maxvalue;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("var",FWEELIN_GETOFS(var),T_variableref);
    case 1:
      return EventParameter("maxvalue",FWEELIN_GETOFS(maxvalue),T_int);      
    }

    return EventParameter();
  };    

  UserVariable *var;  // Variable to increment (toggle)
  int maxvalue;       // Maximum value of variable before wraparound to 0
};

class VideoShowLoopEvent : public Event {
 public:
  EVT_DEFINE_NO_CONSTR(VideoShowLoopEvent,T_EV_VideoShowLoop);
  VideoShowLoopEvent() : loopid(0,0) { Recycle(); };
  virtual void Recycle() {
    layoutid = 0;
    loopid = Range(0,0);
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowLoopEvent &s = (VideoShowLoopEvent &) src;
    layoutid = s.layoutid;
    loopid = s.loopid;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("layoutid",FWEELIN_GETOFS(layoutid),T_int);
    case 1:
      return EventParameter("loopid",FWEELIN_GETOFS(loopid),T_range);
    }

    return EventParameter();
  };    

  int layoutid;    // Layout in which to show loops
  Range loopid;    // Range of loop IDs that will be set to correspond to 
                   // elements in layout
};

class VideoShowLayoutEvent : public Event {
 public:
  EVT_DEFINE(VideoShowLayoutEvent,T_EV_VideoShowLayout);
  virtual void Recycle() {
    layoutid = 0;
    show = 0;
    hideothers = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowLayoutEvent &s = (VideoShowLayoutEvent &) src;
    layoutid = s.layoutid;
    show = s.show;
    hideothers = s.hideothers;
  };
  virtual int GetNumParams() { return 3; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("layoutid",FWEELIN_GETOFS(layoutid),T_int);
    case 1:
      return EventParameter("show",FWEELIN_GETOFS(show),T_char);
    case 2:
      return EventParameter("hideothers",FWEELIN_GETOFS(hideothers),T_char);
    }

    return EventParameter();
  };    

  int layoutid;    // Layout to show
  char show,       // Show it or hide it?
    hideothers;    // Hide other layouts?
};

class VideoShowDisplayEvent : public Event {
 public:
  EVT_DEFINE(VideoShowDisplayEvent,T_EV_VideoShowDisplay);
  virtual void Recycle() {
    displayid = 0;
    show = 0;
    Event::Recycle();
  };
  virtual void operator = (const Event &src) {
    VideoShowDisplayEvent &s = (VideoShowDisplayEvent &) src;
    displayid = s.displayid;
    show = s.show;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("displayid",FWEELIN_GETOFS(displayid),T_int);
    case 1:
      return EventParameter("show",FWEELIN_GETOFS(show),T_char);
    }

    return EventParameter();
  };    

  int displayid;   // Display to show
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
  virtual void operator = (const Event &src) {
    SetMasterInVolumeEvent &s = (SetMasterInVolumeEvent &) src;
    vol = s.vol;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    }

    return EventParameter();
  };    

  float vol; // Volume to set
};

class SetMasterOutVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetMasterOutVolumeEvent,T_EV_SetMasterOutVolume);
  virtual void operator = (const Event &src) {
    SetMasterOutVolumeEvent &s = (SetMasterOutVolumeEvent &) src;
    vol = s.vol;
  };
  virtual int GetNumParams() { return 1; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    }

    return EventParameter();
  };    

  float vol; // Volume to set
};

class SetInVolumeEvent : public Event {
 public:
  EVT_DEFINE(SetInVolumeEvent,T_EV_SetInVolume);
  virtual void operator = (const Event &src) {
    SetInVolumeEvent &s = (SetInVolumeEvent &) src;
    input = s.input;
    vol = s.vol;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("input",FWEELIN_GETOFS(input),T_int);
    case 1:
      return EventParameter("vol",FWEELIN_GETOFS(vol),T_float);
    }

    return EventParameter();
  };    

  int input; // Number of input to change volume for
  float vol; // Volume to set
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
    damp = s.damp;
  };
  virtual int GetNumParams() { return 2; };
  virtual EventParameter GetParam(int n) { 
    switch (n) {
    case 0:
      return EventParameter("loopid",FWEELIN_GETOFS(index),T_int);
    case 1:
      return EventParameter("damp",FWEELIN_GETOFS(damp),T_float);
    }

    return EventParameter();
  };    

  int index;  // Index of loop to adjust amplitude for
  float damp; // Delta for amplitude (how much to change by)
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
  
  char s_filename[FWEELIN_OUTNAME_LEN];	// Filename of scene on disk
};

class PulseSyncEvent : public Event {
 public:   
  EVT_DEFINE(PulseSyncEvent,T_EV_PulseSync);

  int syncidx;      // If this sync is used-defined, this is the sync index
                    // returned from AddSyncPos. If this sync is for the 
                    // downbeat, this parameter is -1.
};

// TriggerSet is fired by TriggerMap whenever a change is made to the map
// The LoopTray and potentially others listen for changes to the TriggerMap
class TriggerSetEvent : public Event {
 public:   
  EVT_DEFINE(TriggerSetEvent,T_EV_TriggerSet);

  int idx;  // On index 'idx'
  Loop *nw; // ..we now have 'nw'
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
  EventManager () : events(0), threadgo(1) {
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

    // Start an event dispatch thread
    int ret = pthread_create(&dispatch_thread,
			     &attr,
			     run_dispatch_thread,
			     static_cast<void *>(this));
    if (ret != 0) {
      printf("(eventmanager) pthread_create failed, exiting");
      exit(1);
    }

    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    // Should event manager be SCHED_FIFO? good question!
    // not an easy question.. non RT event manager means we may
    // introduce a perceptible delay in events handled through
    // broadcast (not broadcastnow, as those go direct)
    //
    // but!.. currently EndRecordEvent does things that are not RT safe
    // so we can't have RT manager!! 
    schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
    //schp.sched_priority = sched_get_priority_min(SCHED_FIFO);
    if (pthread_setschedparam(dispatch_thread, SCHED_OTHER, &schp) != 0) {    
      printf("EVENT: Can't set hi priority thread, will use regular!\n");
    }
  };
  ~EventManager();

  // Event queue functions **

  inline static Event *DeleteQueue(Event *first) {
    Event *cur = first;
    while (cur != 0) {
      Event *tmp = cur->next;
      cur->RTDelete();
      cur = tmp;
    }

    return 0;
  }

  inline static void QueueEvent(Event **first, Event *nw) {
    Event *cur = *first;
    if (cur == 0)
      *first = nw;
    else {
      while (cur->next != 0)
	cur = cur->next;
      cur->next = nw;
    }
  };

  inline static void RemoveEvent(Event **first, Event *prev, Event **cur) {
    Event *tmp = (*cur)->next;
    if (prev != 0)
      prev->next = tmp;
    else 
      *first = tmp;
    (*cur)->RTDelete();
    *cur = tmp;
  };

  // Broadcast immediately!
  inline void BroadcastEventNow(Event *ev, 
				EventProducer *source,
				char allowslowdelivery = 1) {
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
      ev->RTDelete();
    }
  };

  // Broadcast through dispatch thread!
  // RT safe! -- so long as you allocate your event with RTNew()
  void BroadcastEvent(Event *ev, 
		      EventProducer *source) {
    ev->from = source;
    //ev->time = mygettime();

    QueueEvent(&events,ev);
    
    // Wake up the dispatch thread
    if (pthread_mutex_trylock (&dispatch_thread_lock) == 0) {
      pthread_cond_signal (&dispatch_ready);
      pthread_mutex_unlock (&dispatch_thread_lock);
    }
    else
      // Priority inversion
      printf("EVENT: WARNING: Priority inversion during event broadcast! Event '%s' may be lost!\n",Event::ett[(int) ev->GetType()].name);

    // printf("EVENT: SENT: %s!\n",Event::ett[(int) ev->GetType()].name);
  };

  // Not RT safe, but threadsafe!
  // Listen for the given event (optionally from the given producer) and callme
  // when it occurs-- optionally, block calls from myself
  void ListenEvent(EventListener *callme, 
		   EventProducer *from, EventType type, 
		   char block_self_calls = 0) {
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
  void UnlistenEvent(EventListener *callme,
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

  static void *run_dispatch_thread (void *ptr) {
    EventManager *inst = static_cast<EventManager *>(ptr);

    pthread_mutex_lock(&inst->dispatch_thread_lock);

    while (inst->threadgo) {
      // Scan through all events
      Event *cur = inst->events;
      while (cur != 0) {
	//printf("broadcast thread\n");
	// Print time elapsed since broadcast
	//double dt = (mygettime()-cur->time) * 1000; 
	//printf("Evt dispatch- dt: %2.2f ms\n",dt);

	if (cur->GetMgr() == 0)
	  printf("EVENT: WARNING: Broadcast from RT nonRT event!!\n");

	// printf("EVENT: DISPATCH: %s!\n",Event::ett[(int) cur->GetType()].name);
	inst->BroadcastEventNow(cur,cur->from,0); // Force delivery now
	cur = cur->next;
      }

      // Potentially a problem right here-- 
      // If we get interrupted by RT which adds an event, 
      // It will be lost right here when we empty queue
      
      // ** So, perhaps modify event queue so that we block in the case that
      // someone is accessing the queue (for ex, BroadcastEvent)
      // So the dispatch thread holds for other threads. Not a big deal.

      // ** Or, implement lockless ring buffer design here

      // Empty queue!
      inst->events = 0;

      // Wait for wakeup
      pthread_cond_wait (&inst->dispatch_ready, &inst->dispatch_thread_lock);
      // printf("EVENT: WAKEUP!\n");
    }

    printf("Event Manager: end dispatch thread\n");

    pthread_mutex_unlock(&inst->dispatch_thread_lock);

    return 0;
  };

  // For each event type, we store a list of listeners..
  EventListenerItem **listeners;
  // Event queue- for calling listeners in lowpriority
  Event *events;

  pthread_t dispatch_thread;
  pthread_mutex_t dispatch_thread_lock,
    listener_list_lock;
  pthread_cond_t  dispatch_ready;

  int threadgo;
};

#endif
