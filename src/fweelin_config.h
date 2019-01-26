#ifndef __FWEELIN_CONFIG_H
#define __FWEELIN_CONFIG_H

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

#include <SDL/SDL.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "fweelin_block.h"

// Number of beats per bar 
// (used only to compute sync, since FW does not use the concept of bars/beats internally)
#define SYNC_BEATS_PER_BAR 4

// Maximum number of time pulses
#define MAX_PULSES 10 

// How many different sets of loop selections to remember?
#define NUM_LOOP_SELECTION_SETS 10 

// Keep track of the last n indexes we recorded to
#define LAST_REC_COUNT 8 

// Divisions are added in browsers wherever files are greater than
// FWEELIN_FILE_BROWSER_DIVISION_TIME seconds apart
#define FWEELIN_FILE_BROWSER_DIVISION_TIME 3600

#define FWEELIN_CONFIG_DIR  ".fweelin"
#define FWEELIN_CONFIG_FILE "fweelin.xml"
#define FWEELIN_CONFIG_EXT  ".xml"
#define FWEELIN_CONFIG_HELP_TOKEN "HELP:"

#define FWEELIN_OUTPUT_STREAM_NAME "live"
#define FWEELIN_OUTPUT_TIMING_EXT  ".wav.usx"
#define FWEELIN_OUTPUT_LOOP_NAME   "loop"
#define FWEELIN_OUTPUT_SCENE_NAME  "scene"
#define FWEELIN_OUTPUT_SNAPSHOT_NAME "snapshot"
#define FWEELIN_OUTPUT_LOOPSNAPSHOT_NAME "loopsnap"
#define FWEELIN_OUTPUT_DATA_EXT    ".xml"

// Console sequence for error color
#define FWEELIN_ERROR_COLOR_ON "\033[31;1m"
#define FWEELIN_ERROR_COLOR_OFF "\033[0m"

// Interface ID assigned to first non-switchable interface
#define NS_INTERFACE_START_ID 1000

#ifdef __MACOSX__
// On Linux, FWEELIN_DATADIR refers to /usr/local/share/fweelin as set by autoconf
// On Mac, we store our data in the Resource directory of the fweelin.app bundle. 
// This string stores the location of the Resource directory, and is set in the Obj-C stub
// for the code.
extern char *FWEELIN_DATADIR;

// On Linux, VERSION is defined within configure.ac
// For Mac OS, we set it here:
#define VERSION "0.6"
#endif

#include "fweelin_datatypes.h"
#include "fweelin_audioio.h"
#include "fweelin_event.h"
#include "fweelin_videoio.h" // TTF_Font decl

class Event;
class SDLIO;
class MidiIO;
class CircularMap;
class PatchBrowser;
class ParamSetBank;

// ****************** CONFIG CLASSES

enum CfgTokenType {
  T_CFG_None,
  T_CFG_UserVariable,
  T_CFG_EventParameter,
  T_CFG_Static
};

// Config tokens can reference user variables (UserVariable)
// or input EventParameters (EventParameter)
// or static values
class CfgToken {
 public:
  CfgToken() : cvt(T_CFG_None), var(0) {};

  CfgTokenType cvt;

  // Dump CfgToken to stdout
  void Print();

  // Evaluate the current value of this token to dst
  // Using event ev as a reference for event parameter
  // If overwritetype is nonzero, sets dst to be of the appropriate data type
  // Otherwise, converts to existing type of dst 
  void Evaluate(UserVariable *dst, Event *ev, char overwritetype);

  // Reference a user defined variable
  UserVariable *var;
  // Reference an event parameter
  EventParameter evparam;
  // Or reference a static value
  UserVariable val;
};

// Simple algebra is possible in config file
// it allows, for example, a midi fader level to be divided into an appropriate
// amplitude.
// This is one math operation as part of an expression
class CfgMathOperation {
 public:
  CfgMathOperation() : next(0) {};

  // Symbols for different math operators (div, mul, add, sub, etc..)
  const static char operators[];
  // Number of different math operators
  const static int numops;
  
  char otype; // One of the above operator types
  CfgToken operand;

  CfgMathOperation *next;
};

// A complete expression of config tokens modified by math operations
class ParsedExpression {
 public:
  ParsedExpression() : ops(0) {};
  ~ParsedExpression() {
    // Erase math ops
    CfgMathOperation *cur = ops;
    while (cur != 0) {
      CfgMathOperation *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  };

  // Evaluate this expression
  UserVariable Evaluate(Event *input);
 
  // Returns nonzero if this expression contains only static tokens
  // and no user variables and no input parameters
  char IsStatic();

  // Dump expression to stdout
  void Print();

  CfgToken start; // Starting token of expression
  CfgMathOperation *ops; // Optional sequence of math ops to perform on 'val'
};

// DynamicToken is an expression and a config token, 
// used in a few places:
// 
// 1) To evaluate an expression and assign the result to an output event 
//    parameter (wrapped in CfgToken)
// 2) To evaluate an expression and compare the result to an input event 
//    parameter (wrapped in CfgToken)
// 2) To evaluate an expression and compare the result to a user variable
//    (wrapped in CfgToken)
class DynamicToken {
 public:
  DynamicToken() : exp(0), next(0) {};
  ~DynamicToken() { 
    if (exp != 0) 
      delete exp;
  };

  CfgToken token; // Variable/parameter to compare or assign to
  ParsedExpression *exp;   // Expression to evaluate
  DynamicToken *next;
};

// Binding between a freewheeling event and some input action
// controls basic user interface
class EventBinding {
 public:
  EventBinding() : 
    boundproto(0), echo(0), 
    tokenconds(0), paramsets(0), continued(0), next(0) {};
  virtual ~EventBinding();

  Event *boundproto; // Prototype instance of the output event

  // Nonzero if input events should be rebroadcast even
  // if they are consumed in this binding
  char echo;

  // ** Conditions

  // List of dynamic token conditions
  // (for example, MIDI channel on input must match given expression)
  DynamicToken *tokenconds;

  // ** Parameter mappings

  // List of dynamic parameter assignments for output events
  // (for example, when triggering from MIDI keyboard: loop # = notenum + 12)
  DynamicToken *paramsets;

  // Continued is nonzero if the next binding should always be triggered
  // when this binding is triggered- so the next binding is a 
  // continuation of this binding
  char continued;

  EventBinding *next;
};

class InputMatrix : public EventProducer, public EventListener {
 public:

  InputMatrix(Fweelin *app);
  virtual ~InputMatrix();

  // Sets the given variable to the given value- string is interpreted
  // based on variable type
  void SetVariable (UserVariable *var, char *value);

  // Called during configuration to create user defined variables
  void CreateVariable (xmlNode *declare);

  // Called during configuration to bind input controllers to events
  void CreateBinding (int interfaceid, xmlNode *binding);

  // Are the conditions in the EventBinding bind matched by the
  // given input event and user variables?
  char CheckConditions(Event *input, EventBinding *bind);

  // Receive input events
  void ReceiveEvent(Event *ev, EventProducer *from);

  // Start function, called shortly before Fweelin begins running
  void Start();

  // *********** User defined variables

  UserVariable *vars;

  Fweelin *app;

  // Parses a given expression string, extracting tokens
  // for example: 'VAR_curnote+12' references variable VAR_curnote and
  // creates 1 math operation +12
  // The expression may also reference parameters in event 'ref'
  // and these references will be extracted
  ParsedExpression *ParseExpression(char *str, Event *ref, 
                                    char enable_keynames = 0);

 private:

  // Removes leading and trailing spaces from string str
  // Modifies the end of string str and returns a pointer to the new 
  // beginning after spaces
  char *RemoveSpaces (char *str);

  // Adds one key to the given list based on the keysym name
  // Returns the new first pointer
  SDLKeyList *AddOneKey (SDLKeyList *first, char *str);

  // Extracts named keys from the given string and returns a list
  // of the keysyms (named keys are separated by ,)
  SDLKeyList *ExtractKeys (char *str);

  // Parses the given token (no math ops!) into dst
  // Correctly identifies when variables or event parameters are referenced
  void ParseToken(char *str, CfgToken *dst, Event *ref, 
                  char enable_keynames = 0);

  // Stores in ptr the value val given that ptr is of type dtype
  void StoreParameter(char *ptr, CoreDataType dtype, UserVariable *val);

  // Using the eventbinding's parametersets as a template, dynamically
  // sets parameters in the output event
  void SetDynamicParameters(Event *input, Event *output, EventBinding *bind);

  // Scans in the given binding for settings for output event parameters
  // and sets us up to handle those
  void CreateParameterSets (int interfaceid,
                            EventBinding *bind, xmlNode *binding, 
                            Event *input, unsigned char contnum);

  // Scans in the given binding for conditions on input event parameters 
  // or user variables, and sets us up to handle those
  // Returns the hash index for this binding, based on an indexed parameter,
  // or 0 if this binding is not indexed
  int CreateConditions (int interfaceid, EventBinding *bind, 
                        xmlNode *binding, Event *input, int paramidx);

  // Traverses through the list of event bindings beginning at 'start'
  // looking for a binding that matches current user variables and input
  // event 'ev'
  EventBinding *MatchBinding(Event *ev, EventBinding *start);

  // *********** Event Bindings

  // Bindings that trigger on input events- for each input event type,
  // a hashtable of bindings along an indexed parameter
  EventBinding ***input_bind;
};

class FloLayoutElementGeometry {
 public:  
  FloLayoutElementGeometry() : next(0) {};

  virtual ~FloLayoutElementGeometry() {};

  // Draw this element to the given screen-
  // implementation given in videoio.cc
  virtual void Draw(SDL_Surface *screen, SDL_Color clr) = 0;

  // Inside returns nonzero if the given coordinates fall inside this
  // element geometry
  virtual char Inside(int x, int y) = 0;

  // Next geo
  FloLayoutElementGeometry *next;
};

class FloLayoutBox : public FloLayoutElementGeometry {
 public:

  // Draw this element to the given screen-
  // implementation given in videoio.cc
  virtual void Draw(SDL_Surface *screen, SDL_Color clr);

  // Inside returns nonzero if the given coordinates fall inside this
  // element geometry
  virtual char Inside(int x, int y) {
    if (x >= left && x <= right &&
        y >= top && y <= bottom)
      return 1;
    else
      return 0;
  };
  
  // Outlines along borders?
  char lineleft, linetop, lineright, linebottom;
  // Coordinates of box
  int left, top, right, bottom;
};

class FloLayoutElement {
 public:  
  FloLayoutElement() : id(0), name(0), nxpos(0), nypos(0), bx(0.0), by(0.0), 
    loopmap(0), loopx(0), loopy(0), loopsize(0), geo(0), next(0) {};
  virtual ~FloLayoutElement() {
    if (name != 0)
      delete[] name;
    // Erase geometries
    FloLayoutElementGeometry *cur = geo;
    while (cur != 0) {
      FloLayoutElementGeometry *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  };

  // Inside returns nonzero if the given coordinates fall inside this
  // element- we check all geometries
  char Inside(int x, int y) {
    FloLayoutElementGeometry *cur = geo;
    while (cur != 0) {
      if (cur->Inside(x,y))
        return 1; // Inside this geo, so inside this element
      cur = cur->next;
    }    

    return 0; // Not inside any geo, so not inside this element
  };

  int id; // Id of element
  char *name; // Name of element
  int nxpos, nypos; // Location to print name label
  float bx, by; // Base position for element

  // Generated map that will take a flat scope and project it onto the right
  // size circle-- see videoio
  CircularMap *loopmap; 

  int loopx, loopy, // Position of loop graphic for the element
    loopsize; // Size of loop graphic (diameter)

  // Geo describes how to draw this element
  FloLayoutElementGeometry *geo;

  // Next element 
  FloLayoutElement *next;
};

// The user can define the onscreen layout for loops-- see config file!
class FloLayout {
 public:
  FloLayout() : id(0), iid(0), xpos(0), ypos(0), loopids(0,0),
    name(0), nxpos(0), nypos(0), elems(0), show(1), showlabel(1),
    showelabel(1), next(0) {};
  ~FloLayout() {
    if (name != 0)
      delete[] name;
    // Erase elements
    FloLayoutElement *cur = elems;
    while (cur != 0) {
      FloLayoutElement *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  };
  
  int id, // User refers to a layout by layout ID
    iid,  // Interface id. Interface id + layout id uniquely identify 
          // a layout
    xpos, ypos; // Base location on screen for this layout
  Range loopids; // Range of loopids that map to interface elements
  char *name; // ex PC Keyboard, MIDI Footpedal
  int nxpos, nypos; // Location to print name label

  FloLayoutElement *elems; // Elements that make up this layout

  char show, // Layout shown onscreen?
    showlabel, // Name of layout shown onscreen?
    showelabel; // Element names in layout shown onscreen?

  // Next layout
  FloLayout *next;
};

// List of strings- optional two strings per listitem
// Second string is assumed to be substring of first string
// Only first string is deleted at destructor
class FloStringList {
 public:
  FloStringList(char *str, char *str2 = 0) : str(str), str2(str2),
    next(0) {};
  ~FloStringList() {
    if (str != 0)
      delete[] str;
  };

  char *str, *str2;
  FloStringList *next;
};

// List of fonts used in video- video handles the loading and unloading,
// but config sets up these structures to know which fonts and sizes to load
class FloFont {
 public:
  FloFont() : name(0), filename(0), font(0), size(0), next(0) {};
  ~FloFont() {
    if (filename != 0)
      delete[] filename;
    if (name != 0)
      delete[] name;
  };

  char *name, *filename;
  TTF_Font *font;
  int size;
  FloFont *next;
};

enum FloDisplayType {
  FD_Unknown,
  FD_Browser,
  FD_Snapshots,
  FD_ParamSet,
};

// List of variable displays used in video
// There are different types of displays, this is a base class
class FloDisplay {
 public:
  FloDisplay (int iid) : iid(iid), 
    id(-1), exp(0), font(0), title(0), xpos(0), ypos(0), show(1), forceshow(0),
    next(0) {};
  virtual ~FloDisplay() {
    if (title != 0)
      delete[] title;
    if (exp != 0)
      delete exp;
  };

  // Draw this display to the given screen-
  // implementation given in videoio.cc
  virtual void Draw(SDL_Surface *screen) = 0;

  virtual FloDisplayType GetFloDisplayType() { return FD_Unknown; };

  // Set whether this display is showing
  virtual void SetShow(char show) { this->show = show; };

  int iid,  // Interface id. Interface id + display id uniquely identify 
            // a display
    id;     // Display ID

  ParsedExpression *exp; // Expression which evaluates to a value to display
  FloFont *font;         // Font for text
  char *title;           // Title to be displayed
  int xpos, ypos;        // Onscreen location for display
  char show,             // Show (nonzero) or hide (zero) display
    forceshow;           // Force display to show?
    
  FloDisplay *next;
};

// Panel is a 'container' that holds several child displays and surrounds it with a frame
// Allows easy showing and hiding of a number of displays together
class FloDisplayPanel : public FloDisplay
{
 public:
  FloDisplayPanel (int iid) : FloDisplay(iid), sx(100), sy(100), num_children(0), child_displays(0) {};
  ~FloDisplayPanel () {
    if (child_displays != 0)
      delete[] child_displays;
  };

  virtual void Draw(SDL_Surface *screen);

  // Set whether this display is showing
  virtual void SetShow(char show) {
    this->show = show;

    // Show/hide child displays
    for (int i = 0; i < num_children; i++)
      child_displays[i]->SetShow(show);
  };

  int sx, sy; // Size of panel
  int margin; // Margin for displays inside panel

  // Array of pointers to all child displays. Child displays are still stored in the master list of
  // displays, and drawn independently of the panel.
  int num_children;
  FloDisplay **child_displays;
};

// Text display shows the value of expression 'exp' as onscreen text
class FloDisplayText : public FloDisplay 
{
 public:
  FloDisplayText (int iid) : FloDisplay(iid) {};

  virtual void Draw(SDL_Surface *screen);
};

// Switch display shows the title in different color depending on the value of
// expression 'exp'
class FloDisplaySwitch : public FloDisplay 
{
 public:
  FloDisplaySwitch (int iid) : FloDisplay(iid) {};

  virtual void Draw(SDL_Surface *screen);
};

// Circle switch display shows a circle which changes color and optionally
// flashes depending on the value of expression 'exp'
class FloDisplayCircleSwitch : public FloDisplay 
{
 public:
  FloDisplayCircleSwitch (int iid) : FloDisplay(iid), rad1(0), rad0(0), flash(0), prevnonz(0), 
    nonztime(0.) {};

  virtual void Draw(SDL_Surface *screen);

  int rad1, rad0; // Radii of circle when switch is on or off

  // For flashing
  char flash,  // Flash or solid colors?
    prevnonz;  // Previous character value of expression (for nonzero test)
  double nonztime; // System time at which the expression last became nonzero
};

// Text switch display shows one string of text when a value is nonzero,
// and another string of text when a value is zero.
class FloDisplayTextSwitch : public FloDisplay 
{
 public:
  FloDisplayTextSwitch (int iid) : FloDisplay(iid), text1(0), text0(0) {};

  virtual void Draw(SDL_Surface *screen);

  char *text1, // Text for nonzero value
    *text0;    // Text for zero value
};

enum CfgOrientation {
  O_Horizontal,
  O_Vertical
};

// Bar display shows the value of expression 'exp' as a bar
class FloDisplayBar : public FloDisplay 
{
 public:
  FloDisplayBar (int iid) : FloDisplay(iid), 
    orient(O_Vertical), barscale(1.0), thickness(10), dbscale(0), marks(0), maxdb(0) {};

  virtual void Draw(SDL_Surface *screen);

  CfgOrientation orient; // Orientation of bar
  float barscale; // Scaling factor for size of bar
  int thickness;  // Thickness of bar
  char dbscale,   // If nonzero, this bar maps a linear amplitude variable to a logarithmic scale bar
                  // If zero, there is a linear mapping to the bar
    marks;        // If nonzero, calibration marks are shown (dB scale only)
  float maxdb;    // (dbscale) Maximum dB level shown
};

// Bar-switch display shows the value of expression 'exp' as a bar and changes the color of the bar
// depending on the value of expression 'switchexp'
class FloDisplayBarSwitch : public FloDisplayBar
{
 public:
  FloDisplayBarSwitch (int iid) : FloDisplayBar(iid), 
    switchexp(0), color(1), calibrate(0), cval(0.0) {};
  virtual ~FloDisplayBarSwitch() {
    if (switchexp != 0)
      delete switchexp;
  };

  virtual void Draw(SDL_Surface *screen);

  ParsedExpression *switchexp; // Expression which evaluates to a value. Nonzero values cause the bar 
                               // to appear bright, zero values cause a dim, faded bar
  
  int color; // Color of bar-switch (index of hardcoded color)
  char calibrate; // Nonzero shows calibration value on barswitch & changes color when level exceeds calibration value
  float cval; // Calibration value (linear)
};

class FloDisplaySquares : public FloDisplay 
{
 public:
  FloDisplaySquares (int iid) : FloDisplay(iid), orient(O_Horizontal) {};

  virtual void Draw(SDL_Surface *screen);

  CfgOrientation orient; // Orientation of bar
  float v1, v2,          // Value corresponding to first and last square
    sinterval;           // 1 square for every 'sinterval' change in value
  int sx, sy;            // Square size
};

// FluidSynth config
#include "fweelin_fluidsynth.h"

#if USE_FLUIDSYNTH

class FluidSynthParam {
 public:
  FluidSynthParam(char *name) : next(0) {
    this->name = new char[strlen(name)+1];
    strcpy(this->name,name);
  };
  virtual ~FluidSynthParam() { delete[] name; };

  // Send this parameter into the given settings
  virtual void Send(fluid_settings_t *settings) = 0;

  char *name;
  FluidSynthParam *next;
};

class FluidSynthParam_Num : public FluidSynthParam {
 public:

  FluidSynthParam_Num(char *name, double val) :
    FluidSynthParam(name), val(val) {};

  virtual void Send(fluid_settings_t *settings);

  double val;
};

class FluidSynthParam_Int : public FluidSynthParam {
 public:

  FluidSynthParam_Int(char *name, int val) :
    FluidSynthParam(name), val(val) {};

  virtual void Send(fluid_settings_t *settings);

  int val;
};

class FluidSynthParam_Str : public FluidSynthParam {
 public:

  FluidSynthParam_Str(char *name, char *val) :
    FluidSynthParam(name) {
    this->val = new char[strlen(val)+1];
    strcpy(this->val,val);
  };
  virtual ~FluidSynthParam_Str() { delete[] val; };

  virtual void Send(fluid_settings_t *settings);

  char *val;
};

class FluidSynthSoundFont {
 public:
  FluidSynthSoundFont(char *name) : next(0) {
    if (strchr(name,'/') != 0) {
      // Path specified
      this->name = new char[strlen(name)+1];
      strcpy(this->name,name);
    } else {
      // Path not specified, use default
      this->name = new char[strlen(FWEELIN_DATADIR)+1+strlen(name)+1];
      sprintf(this->name,"%s/%s",FWEELIN_DATADIR,name);
    }
  };
  ~FluidSynthSoundFont() { delete[] name; };

  char *name;
  FluidSynthSoundFont *next;
};
#endif

// Fweelin configuration
class FloConfig {
 public:
  FloConfig(Fweelin *app);
  ~FloConfig();

  // Parse configuration file, setup config
  void Parse();

  // Start function, called shortly before Fweelin begins running
  void Start() { im.Start(); };

  // Send start-interface event to all interfaces
  void StartInterfaces ();

  // Copy config file from shared folder
  // Optionally copy all config files
  void CopyConfigFile (char *cfgname, char copyall);

  // Prepare to load configuration file 'cfgname' 
  // Finds the file in one of several places, and copies it to
  // the config folder. Returns the path name if found, or null if not found
  char *PrepareLoadConfigFile (char *cfgname, char basecfg);
  
  // Configure bindings between events and their triggers
  void ConfigureEventBindings(xmlDocPtr /*doc*/, xmlNode *events,
                              int interfaceid = 0, char firstpass = 0);

  // Configuring displays
  void SetupParamSetBank(xmlDocPtr /*doc*/, xmlNode *banknode, ParamSetBank *bank);
  FloDisplay *SetupParamSet(xmlDocPtr doc, xmlNode *paramset, int interfaceid);

  // Configuration sections
  void ConfigureElement(xmlDocPtr /*doc*/, xmlNode *elemn,
                        FloLayoutElement *elem, float xscale, float yscale);
  void ConfigureLayout(xmlDocPtr doc, xmlNode *layn, 
                       FloLayout *lay, float xscale, float yscale);
  void ConfigureDisplay_Common(xmlNode *disp, FloDisplay *nw, FloDisplayPanel *parent);
  void ConfigureDisplay(xmlDocPtr doc, xmlNode *disp, int interfaceid, FloDisplayPanel *parent = 0);
  void ConfigurePatchBanks(xmlNode *pb, PatchBrowser *br);
  void ConfigureGraphics(xmlDocPtr doc, xmlNode *vid, int interfaceid = 0);
  void ConfigureBasics(xmlDocPtr /*doc*/, xmlNode *gen);
  void ConfigureInterfaces (xmlDocPtr /*doc*/, xmlNode *ifs, char firstpass);
  void ConfigureRoot (xmlDocPtr doc, xmlNode *root, int interfaceid = 0,
                      char firstpass = 0);
  
  // Is node 'n' a comment with help information? If so, add to our
  // internal help list
  void CheckForHelp(xmlNode *n);

  // Creates an empty variable based on the given name. The config file
  // can then refer to the variable
  UserVariable *AddEmptyVariable(char *name);

  // Returns a pointer to the given variable
  UserVariable *GetVariable(char *name);

  // Makes the given variable into a system variable by linking it to
  // the pointer
  void LinkSystemVariable(char *name, CoreDataType type, char *ptr);

  // Input matrix- stores and handles all bindings between inputs and events
  inline InputMatrix *GetInputMatrix() { return &im; };
  InputMatrix im;

  // Add an event hook, if none already exists
  // An event hook gets first dibs on incoming events
  // We use this to override the usual functions of, say, the keyboard,
  // and redirect them elsewhere, like typing text.
  // 
  // Returns zero on success and nonzero if another hook exists already
  inline char AddEventHook (EventHook *hook) {
    if (ev_hook == 0) {
      ev_hook = hook;
      return 0;
    } else
      return 1;
  };

  // Remove an event hook
  // Returns zero on success
  inline char RemoveEventHook (EventHook *hook) {
    if (ev_hook == hook) {
      ev_hook = 0;
      return 0;
    } else
      return 1;
  };

  // Event hook- right now, we only support one at a time
  EventHook *ev_hook;

  // Extracts an array of floats (delimited by character delim_char)
  // from the given string- returns size of array in 'size'
  float *ExtractArray(char *n, int *size, char delim_char = ',');
  
  // Same, with ints
  int *ExtractArrayInt(char *n, int *size, char delim_char = ',');
  
  // Library path
  inline char *GetLibraryPath() { 
    if (librarypath != 0)
      return librarypath; 
    else {
      printf("CORE: ERROR: Library path not set in configuration!\n");
      exit(1);
    }
  };
  char *librarypath;

  // Number of MIDI out ports
  inline int GetNumMIDIOuts() { return midiouts; };
  int midiouts;

  // List of MIDI ports to transmit sync info to
  inline int GetNumMIDISyncOuts() { return msnumouts; };
  inline int *GetMIDISyncOuts() { return msouts; };
  int msnumouts, *msouts;
  
  // Is input/output #n stereo?
  inline char IsStereoInput(int out_n) { return ms_inputs[out_n]; };
  inline char IsStereoOutput(int /*out_n*/) { return IsStereoMaster(); };
  inline char IsInputMonitoring(int out_n) { return monitor_inputs[out_n]; };
  char *ms_inputs,    // Zero or nonzero for each input- is this input stereo?
    *monitor_inputs;  // Nonzero if monitoring is enabled for this input
  // Is FreeWheeling running in stereo or completely in mono?
  char IsStereoMaster();

  // Stream settings
  inline char IsStreamFinal() { return stream_final; };
  inline char IsStreamLoops() { return stream_loops; };
  inline char IsStreamInputs(int n) { return stream_inputs[n]; };
  char stream_final,
    stream_loops,
    *stream_inputs;

  // Number of external audio inputs into FreeWheeling (specified in config file)
  // AudioIO may add its own inputs internal to FreeWheeling 
  // (for example, softsynth)
  inline int GetExtAudioIns() { return extaudioins; };
  int extaudioins;
  
  // Maximum play volume
  inline float GetMaxPlayVol() { return maxplayvol; };
  float maxplayvol;

  // Maximum limiter gain
  inline float GetMaxLimiterGain() { return maxlimitergain; };
  float maxlimitergain;

  // Limiter threshhold
  inline float GetLimiterThreshhold() { return limiterthreshhold; };
  float limiterthreshhold;

  // Limiter release rate
  inline float GetLimiterReleaseRate() { return limiterreleaserate; };
  float limiterreleaserate;
  
  // Logarithmic fader settings
  inline float GetFaderMaxDB() { return fadermaxdb; };
  float fadermaxdb;
    
  // File format to save loops to
  inline codec GetLoopOutFormat() { return loopoutformat; };
  codec loopoutformat;

  // File format to save streams to
  inline codec GetStreamOutFormat() { return streamoutformat; }; 
  codec streamoutformat;

  inline char *GetCodecName (codec i) {
    switch (i) {
      case VORBIS: return "ogg"; 
      case WAV: return "wav"; 
      case FLAC: return "flac";
      case AU: return "au";
      default: return "UNKNOWN";
    }
  };

  inline codec GetCodecFromName (const char *n) {
    for (codec i = FIRST_FORMAT; i < END_OF_FORMATS; i = (codec) (i+1))
      if (!strcasecmp(n,GetCodecName(i)))
        return i;
    
    return UNKNOWN;
  };

  inline char *GetAudioFileExt (codec i) {
    switch (i) { 
      case VORBIS: return ".ogg"; 
      case WAV: return ".wav"; 
      case FLAC: return ".flac";
      case AU: return ".au";
      default: return ".ogg";
    };
  };

  // Quality for encoding OGG files
  inline float GetVorbisEncodeQuality() { return vorbis_encode_quality; };
  float vorbis_encode_quality;

  // Number of triggers (loop ids)
  inline int GetNumTriggers() { return num_triggers; };
  int num_triggers;

  // Video config
  inline int *GetVSize() { return vsize; };
  inline float XCvtf(float x) { return (x*vsize[0]); };
  inline float YCvtf(float y) { return (y*vsize[1]); };
  inline int XCvt(float x) { return (int) (x*vsize[0]); };
  inline int YCvt(float y) { return (int) (y*vsize[1]); };
  int vsize[2];
  inline int GetVDelay() { return vdelay; };
  int vdelay;

  // # of samples in visual oscilloscope buffer
  nframes_t scope_sample_len;
  inline nframes_t GetScopeSampleLen() { return scope_sample_len; };

  // Macro to check whether debug info is on
#define CRITTERS (app->getCFG()->IsDebugInfo())
  // Return nonzero if debug info to be shown
  char IsDebugInfo() { return showdebug; };
  // Show debugging info?
  char showdebug;

  // Graphical layouts
  FloLayout *GetLayouts() { return layouts; };
  FloLayout *layouts;

  // Graphical fonts
  // Returns the named font from our list of fonts
  FloFont *GetFont (char *name) {
    FloFont *cur = fonts;
    while (cur != 0 && strcmp(cur->name,name))
      cur = cur->next;
    return cur;
  };
  FloFont *GetFonts() { return fonts; };
  FloFont *fonts;

  // Graphical displays
  inline FloDisplay *GetDisplays() { return displays; };
  inline FloDisplay *GetDisplayById (int iid, int id) {
    FloDisplay *cur = displays;
    while (cur != 0) {
      if (cur->iid == iid && cur->id == id)
        return cur;
      cur = cur->next;
    }
    return 0;
  };
  inline FloDisplay *GetDisplayByType (FloDisplayType typ) {
    FloDisplay *cur = displays;
    while (cur != 0) {
      if (cur->GetFloDisplayType() == typ)
        return cur;
      cur = cur->next;
    }
    return 0;
  };
  FloDisplay *displays;

  // Help text
  int GetNumHelpLines() {
    FloStringList *cur = help;
    int cnt = 0;
    while (cur != 0) {
      cnt++;
      cur = cur->next;
    }

    return cnt;
  };
  char *GetHelpLine(int idx, int col) {
    FloStringList *cur = help;
    int cnt = 0;
    while (cur != 0 && cnt != idx) {
      cnt++;
      cur = cur->next;
    }
    
    if (cur == 0)
      return 0;
    else
      return (col == 0 ? cur->str : cur->str2);
  };
  FloStringList *help;

#if USE_FLUIDSYNTH
  // FluidSynth config
  int fsinterp;
  int GetFluidInterpolation() { return fsinterp; };
  int fschannel;
  int GetFluidChannel() { return fschannel; };
  char fsstereo;
  char GetFluidStereo() { return fsstereo; };
  float fstuning;
  float GetFluidTuning() { return fstuning; };
  FluidSynthParam *fsparam;
  FluidSynthParam *GetFluidParam() { return fsparam; };
  void AddFluidParam(FluidSynthParam *nw) {
    if (fsparam == 0)
      fsparam = nw;
    else {
      FluidSynthParam *cur = fsparam;
      while (cur->next != 0)
        cur = cur->next;
      cur->next = nw;
    }
  };
  FluidSynthSoundFont *fsfont;
  void AddFluidFont(FluidSynthSoundFont *nw) {
    if (fsfont == 0)
      fsfont = nw;
    else {
      FluidSynthSoundFont *cur = fsfont;
      while (cur->next != 0)
        cur = cur->next;
      cur->next = nw;
    }
  };
  FluidSynthSoundFont *GetFluidFont() { return fsfont; };
#endif

  // Pitch transpose on outgoing MIDI events
  signed int transpose;

  // Chunksize for peaks & avgs display of loops
  // (bigger # means shorter displays)
  nframes_t loop_peaksavgs_chunksize;

  int status_report;
#define FS_REPORT_BLOCKMANAGER 1

  // Total number of interfaces defined in config 
  int numinterfaces, // Switchable interfaces
                     // (range 1<=i<=numinterfaces) 
    numnsinterfaces; // Nonswitchable interfaces

  // Maximum number of snapshots user can create
  inline int GetMaxSnapshots() { return max_snapshots; };
  int max_snapshots;

  // Seconds of fixed audio history 
  const static float AUDIO_MEMORY_LEN;
  // # of audio blocks to preallocate
  const static int NUM_PREALLOCATED_AUDIO_BLOCKS;
  // # of time markers to preallocate
  const static int NUM_PREALLOCATED_TIME_MARKERS;
  // Maximum path length for config files
  const static int CFG_PATH_MAX;
};

#endif
