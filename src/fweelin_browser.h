#ifndef __FWEELIN_BROWSER_H
#define __FWEELIN_BROWSER_H

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

#include "fweelin_event.h"
#include "fweelin_config.h"

class CircularMap;
class MouseButtonInputEvent;
class MouseMotionInputEvent;

enum ResizeType {
  RS_Off,
  RS_Move, // Move only
  RS_Top,
  RS_Bottom,
  RS_Left,
  RS_Right,
  RS_TopLeft,
  RS_TopRight,
  RS_BottomLeft,
  RS_BottomRight
};

enum BrowserItemType {
  B_Undefined = 0,
  B_Loop_Tray,  // Loop tray  (loaded loops)
  B_Scene_Tray, // Scene tray (loaded scenes)
  B_Loop,       // Loop       (all loops in library)
  B_Scene,      // Scene      (all scenes in library)
  B_Patch,
  B_Division,
  B_Last
};

class BrowserItem {
 public:
  // Initialize a browser item with name n
  BrowserItem(const char *n = 0, char default_name = 1) :
    default_name(default_name), next(0), prev(0) {
    if (n == 0)
      name = 0;
    else {
      name = new char[strlen(n)+1];
      strcpy(name,n);
    }
  };
  virtual ~BrowserItem() {
    if (name != 0)
      delete[] name;
  };

  virtual BrowserItemType GetType() = 0;

  // Compare this item to a second item and return whether this item is
  // greater than (>0), less than (<0), or equal to (==0) second
  // Used for sorting browser items
  virtual int Compare(BrowserItem */*second*/) { return 0; };

  // Return zero if the item described by 'itemmatch' matches this item
  // This is a type-neutral compare function to be overridden to provide
  // meaningful matches for different browsers
  virtual int MatchItem(int /*itemmatch*/) { return 1; };

  char default_name; // Nonzero if the name for this item is a default name
  char *name;
  BrowserItem *next, *prev;
};

class BrowserDivision : public BrowserItem {
 public:

  virtual BrowserItemType GetType() { return B_Division; };
};

class BrowserCallback {
 public:
  virtual void ItemBrowsed(BrowserItem *i) = 0;
  virtual void ItemSelected(BrowserItem *i) = 0;
  virtual void ItemRenamed(BrowserItem *i) = 0;
};

class RenameCallback {
 public:
  virtual void ItemRenamed(char *nw) = 0;
};

class RenameUIVars {
 public:
  RenameUIVars() : rename_cursor_blinktime(0), rename_cursor_toggle(0) {};

  double rename_cursor_blinktime;  
  char rename_cursor_toggle;    
};

// This class encapsulates the UI for renaming an item 
class ItemRenamer : public EventHook {
 public:
  const static int RENAME_BUF_SIZE = 512; // Maximum length of item name
  const static double BLINK_DELAY;

  ItemRenamer(Fweelin *app, RenameCallback *cb, char *oldname);

  virtual ~ItemRenamer() {};

  // Hook events for typing new names
  virtual char HookEvent(Event *ev, EventProducer */*from*/);

  inline char *GetCurName() { return rename_tmpbuf; };
  inline RenameUIVars *UpdateUIVars() { 
    double t = mygettime();
    if (t-rui.rename_cursor_blinktime >= BLINK_DELAY) {
      rui.rename_cursor_blinktime = t;
      rui.rename_cursor_toggle = (rui.rename_cursor_toggle ? 0 : 1);
    }
          
    return &rui;
  };
  inline char IsRenaming() { return renaming; };

 private:

  inline void Rename_Append(char c) {
    int rblen = strlen(rename_tmpbuf);
    if (rblen+1 < RENAME_BUF_SIZE) {
      rename_tmpbuf[rblen] = c;
      // printf("KEY: %c\n",rename_tmpbuf[rblen]);
      rename_tmpbuf[++rblen] = '\0';
    }
  };

  inline void Rename_Backspace() {
    int rblen = strlen(rename_tmpbuf);
    if (rblen > 0)
      rename_tmpbuf[rblen-1] = '\0';
  };

  Fweelin *app;
  RenameCallback *cb; // Way to notify the caller when we are done renaming

  char rename_tmpbuf[RENAME_BUF_SIZE]; // Buffer for new name
  RenameUIVars rui;

  char renaming; // Nonzero if we are renaming
};

class Browser : public FloDisplay, public EventListener, public EventProducer,
                public RenameCallback {
 public:
  Browser (int iid, 
           BrowserItemType btype, char xpand, int xpand_x1, int xpand_y1,
           int xpand_x2, int xpand_y2, float xpand_delay) : 
    FloDisplay(iid),
    renamer(0), app(0), btype(btype), callback(0), first(0), cur(0),
    xpand_x1(xpand_x1), xpand_y1(xpand_y1), 
    xpand_x2(xpand_x2), xpand_y2(xpand_y2), xpand_centery(-1), 
    xpand_liney(-1), xpand_spread(-1), xpand_lastactivity(0.0), 
    xpand_delay(xpand_delay), xpand(xpand), xpanded(0) {
    pthread_mutex_init (&browser_lock,0);

    if (btype >= B_Last || (int) btype < 0) {
      printf("BROWSER: Invalid browsetype %d!\n",btype);
      btype = (BrowserItemType) 0;
    }
  };
  virtual ~Browser();
  
  // Call after construction when we have app & callback ready
  virtual void Setup(Fweelin *a, BrowserCallback *c);

  // Move to the beginning of the browser list
  // Does not fully update, so MoveTo must be called after
  inline void MoveToBeginning() { 
    LockBrowser(); 
    cur = first; 
    UnlockBrowser();
  };
  // Move to a new item, relative to the current
  void MoveTo(int adjust, int jumpadjust);

  // Select the current item
  void Select();

  // Rename the current item
  void Rename();

  // Mouse button pressed (return nonzero to eat event, zero to ignore)
  virtual char MouseButton(MouseButtonInputEvent */*evt*/) { return 0; };
  // Mouse moved (return nonzero to eat event, zero to ignore)
  virtual char MouseMotion(MouseMotionInputEvent */*evt*/) { return 0; };

  // Get the onscreen display name for a file with given name. 
  // (filename must refer to a file of the type this browser handles)
  // Write the name to outbuf, with max maxlen characters.
  // Returns nonzero if we used a 'default' name.
  char GetDisplayName(char *filename, time_t *filetime,
                      char *outbuf, int maxlen);

  ItemRenamer *renamer; // Renamer instance, or null if we are not renaming
  virtual void ItemRenamed(char *nw); // Callback for renamer
  
  // Via this method, a browser is notified of a change to the on-disk name
  // of an item. For example, when a Loop in memory is renamed, we also
  // rename it on disk, and the loop browser must be notified of this new name.
  void ItemRenamedOnDisk(char *old_filename, char *new_filename, 
                         char *new_name);

  inline BrowserItemType GetType() { return btype; };
  virtual FloDisplayType GetFloDisplayType() { return FD_Browser; };

  inline static char *GetTypeName(BrowserItemType b) {
    switch (b) {
    case B_Scene_Tray :
      return "Scene Tray";

    case B_Loop_Tray :
      return "Loop Tray";

    case B_Loop :
      return "Loop";

    case B_Scene :
      return "Scene";

    case B_Division :
      return "Division";
      
    case B_Patch :
      return "Patch";
      
    default :
      return "**UNKNOWN**";  
    }
  };

  // Receive events
  virtual void ReceiveEvent(Event *ev, EventProducer */*from*/);

  // Draw to screen
  virtual void Draw(SDL_Surface *screen);
  virtual void Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y);

  virtual void ClearAllItems();
  
  // Add divisions between browser items using the Compare function-
  // whenever two neighbouring items have difference greater than maxdelta,
  // a division is inserted. In browsing files, for example, this allows us
  // to group files that where created close to one another in time
  void AddDivisions(int maxdelta);

  // Add an item to this browser
  // (Doubley linked list add with sort)
  // Nonzero if we should sort (according to the BrowserItem::Compare() method)
  virtual void AddItem(BrowserItem *nw, char sort = 0);
  
  // Remove an item from this browser
  // For each item we call the MatchItem(itemmatch) method in BrowserItem 
  // until MatchItem returns zero. Then we remove that item.
  virtual void RemoveItem(int itemmatch);

  // Return current browser item  
  inline BrowserItem *GetCurItem() { return cur; };

  // Threadsafe item rename 
  inline void RenameItem(BrowserItem *i, char *nw) {
    LockBrowser();
    if (i->name != 0) 
      delete[] i->name;
    if (nw != 0) {
      i->name = new char[strlen(nw)+1];
      strcpy(i->name,nw);
    } else
      i->name = 0;
    UnlockBrowser();
  };
  
  inline void LockBrowser() {
    pthread_mutex_lock (&browser_lock);
  };
  inline void UnlockBrowser() {
    pthread_mutex_unlock (&browser_lock);
  };
  
  protected:

  // Internal update function called when a new item is browsed-
  // derived classes may override
  virtual void ItemBrowsed() {};

  Fweelin *app; 
  
  BrowserItemType btype;     // Browser type
  BrowserCallback *callback; // Callback to notify of browser events
  BrowserItem *first,        // First item
    *cur;                    // Current item
  
  // Dimensions of expanded browser view when moving between items:
  int xpand_x1, xpand_y1,
    xpand_x2, xpand_y2,
    xpand_centery,
    xpand_liney,
    xpand_spread;
  double xpand_lastactivity, // Time of last activity in browser-
                             // controls when to expand
    xpand_delay;             // Time to hold up expanded browser

  char xpand, // Nonzero if we should expand when moving
    xpanded;  // Nonzero if we are expanded

  pthread_mutex_t browser_lock; // A way to lock up a browser so two threads
                                // don't race on it
};

class LoopTray : public Browser {
 public:
  LoopTray (int iid,
            BrowserItemType btype, char xpand, int xpand_x1, int xpand_y1,
            int xpand_x2, int xpand_y2, int loopsize) : 
    Browser(iid,btype,xpand,xpand_x1,xpand_y1,xpand_x2,xpand_y2,0.0),
    loopsize(loopsize), basepos(0), iconsize(0), 
    resize_win(RS_Off), touchtray(0), loopmap(0) {};
  virtual ~LoopTray();

  // Call after construction when we have app & callback ready
  virtual void Setup(Fweelin *a, BrowserCallback *c);

  // Draw to screen
  virtual void Draw(SDL_Surface *screen);
  virtual void Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y);

  // Receive events
  virtual void ReceiveEvent(Event *ev, EventProducer *from);

  // Mouse updates
  virtual char MouseButton(MouseButtonInputEvent *mev);
  virtual char MouseMotion(MouseMotionInputEvent *mev);

  // Renaming
  void Rename(int loopid);
  virtual void ItemRenamed(char *nw);
  void ItemRenamedFromOutside(Loop *l, char *nw);

  int loopsize,   // Size of loops in tray
    basepos,      // Base (border) size in tray
    iconsize;     // Size of iconified tray
  
  // Window move/resize variables
  ResizeType resize_win;
  int resize_button,
    resize_xhand,
    resize_yhand,
    old_xpand_x1, old_xpand_x2, old_xpand_y1, old_xpand_y2;

  // Has the tray been changed and coordinates need recalculating?
  char touchtray;

  CircularMap *loopmap; // Mapping to draw circular loops
};

class CombiZone {
 public:
  void SetupZone (int kr_lo = 0, int kr_hi = 0,
                  char port_r = 0, int port = 0,
                  int bank = -1, int prog = -1, int channel = 0,
                  char bypasscc = 0, int bypasschannel = -1, float bypasstime1 = 0.0, float bypasstime2 = 10.0) {
    this->kr_lo = kr_lo;
    this->kr_hi = kr_hi;
    this->port_r = port_r;
    this->port = port;
    this->bank = bank;
    this->prog = prog;
    this->channel = channel;
    
    this->bypasscc = bypasscc;
    this->bypasschannel = bypasschannel;
    this->bypasstime1 = bypasstime1;
    this->bypasstime2 = bypasstime2;
  };

  int kr_lo,   // Bottom key for this zone
    kr_hi;     // Top key for this zone
  char port_r; // Nonzero if we should transmit to the MIDI port given below
  int port,  // By default, a zone transmits to the MIDI port given by 
             // a given patchbank. But it is also possible to redirect
             // a zone to output to any MIDI port. 0 is the internal 
             // FluidSynth port.
    bank, // Bank select for zone (-1 for none)
    prog, // Program change for zone (-1 for none)
    channel; // Transmit channel # for zone
    
  char bypasscc;      // MIDI CC to send for bypass
  int bypasschannel;  // MIDI channel to send bypass to (or -1 to use regular channel)
  float bypasstime1,  // Length of time (s) before auto-bypass when notes are sustaining (with or without pedal)
    bypasstime2;      // Length of time (s) before auto-bypass when notes have been released
};

// Small class to encapsulate patches
class PatchItem : public BrowserItem {
public:

  PatchItem (int id = 0, int bank = 0, int prog = 0, int channel = 0,
             const char *name = 0, char bypasscc = 0, int bypasschannel = -1, float bypasstime1 = 0.0, float bypasstime2 = 10.0) :
    BrowserItem(name), id(id), bank(bank), prog(prog), channel(channel),
    bypasscc(bypasscc), bypasschannel(bypasschannel), bypasstime1(bypasstime1), bypasstime2(bypasstime2), 
    zones(0), numzones(0) {};
  virtual ~PatchItem() {
    if (zones != 0)
      delete[] zones;
  };

  inline void SetupZones (int numzones) {
    this->numzones = numzones;
    zones = new CombiZone[numzones];
  }

  inline CombiZone *GetZone (int idx) {
    if (idx >= 0 && idx < numzones)
      return &zones[idx];
    else
      return 0;
  };

  // Is this a combi patch (multizone) (returns nonzero) 
  // or just a regular patch? (returns zero)
  inline char IsCombi() { return numzones > 0; };

  virtual BrowserItemType GetType() { return B_Patch; };

  int id, // Unique patch ID
    bank, // Bank select for patch
    prog, // Program change for patch
    channel; // Channel # for patch
  
  char bypasscc;      // MIDI CC to send for bypass
  int bypasschannel;  // MIDI channel to send bypass to (or -1 to use regular channel)
  float bypasstime1,  // Length of time (s) before auto-bypass when notes are sustaining (with or without pedal)
    bypasstime2;      // Length of time (s) before auto-bypass when notes have been released
  
  // For a combination patch, we have an array of all zones
  CombiZone *zones;
  int numzones;
};

// Patch browser is unique in that it manages -several- lists of
// BrowserItems-- one for each MIDI port and, optionally, each channel 
// to which patch names are assigned.
//
// Each list of patches is called a PatchBank:
#define MIDI_BANKCHANGE_MSB 0
#define MIDI_BANKCHANGE_LSB 32
class PatchBank {
 public:
  PatchBank (int port, int tag, char suppresschg) : port(port), 
    tag(tag), suppresschg(suppresschg),
    first(0), cur(0), next(0) {};

  int port,           // MIDI output port for this patch bank
                      // (or -1 for internal FluidSynth patch bank)
    tag;              // Tag for identifying patchbank- can be 
                      // used by the user config to change FW behavior when
                      // a given patchbank is active
  
  char suppresschg;   // Suppress program/bank change messages being sent 
                      // for this patch bank?

  BrowserItem *first, // First item
    *cur;             // Current item
  
  PatchBank *next;
};

class PatchBrowser : public Browser {
  friend class Fweelin;

 public:
  // Every DIV_SPACING patches, we insert a divider in the browser
  const static int DIV_SPACING = 10;
  
  PatchBrowser (int iid,
                BrowserItemType btype, char xpand, int xpand_x1, int xpand_y1,
                int xpand_x2, int xpand_y2, float xpand_delay) : 
    Browser(iid,btype,xpand,xpand_x1,xpand_y1,xpand_x2,xpand_y2,xpand_delay),
    pb_first(0), pb_cur(0), pb_cur_tag(-1), num_pb(0) {};
  virtual ~PatchBrowser();

  virtual void ClearAllItems();
  virtual void Setup(Fweelin *a, BrowserCallback *c);
  virtual void ReceiveEvent(Event *ev, EventProducer *from);

  // Add patch bank
  void PB_Add (PatchBank *pb) {
    LockBrowser();
    num_pb++;

    // Update pb_cur
    if (pb_cur != 0) {
      pb_cur->cur = cur;
      pb_cur->first = first;
    }

    // Insert at end of list
    if (pb_first == 0)
      pb_first = pb_cur = pb;
    else {
      PatchBank *pb_i = pb_first;
      while (pb_i->next != 0)
        pb_i = pb_i->next;
      pb_i->next = pb_cur = pb;
    }
    
    // And set cur & first pointers
    cur = pb->cur;
    first = pb->first;

    pb_cur_tag = pb->tag;
    SetMIDIForPatch();

    UnlockBrowser();
  };

  // Add patch bank at beginning of patchbanks
  void PB_AddBegin (PatchBank *pb) {
    LockBrowser();
    num_pb++;

    // Update pb_cur
    if (pb_cur != 0) {
      pb_cur->cur = cur;
      pb_cur->first = first;
    }

    // Insert at beginning of list
    pb->next = pb_first;
    pb_first = pb_cur = pb;
    
    // And set cur & first pointers
    cur = pb->cur;
    first = pb->first;

    pb_cur_tag = pb->tag;
    SetMIDIForPatch();

    UnlockBrowser();
  };

  // Move to the next (+ve direction) or previous (-ve direction) patch bank
  void PB_MoveTo (int direction);

  // Move to a patchbank given by index 
  void PB_MoveToIndex (int index);

  inline PatchBank *GetCurPatchBank() const { return pb_cur; };

  // Sets the right MIDI port(s) and channel(s) for echo & auto-bypass based on this
  // patches' settings
  void SetMIDIForPatch();

 protected:

  // Select patch automatically when we browse to it
  // ** Disabled
  // virtual void ItemBrowsed() { SetMIDIForPatch(); };

 private:

  PatchBank *pb_first, // List of patchbanks
    *pb_cur;           // Current patchbank
  int pb_cur_tag,      // Tag from current patchbank 
    num_pb;            // Number of patchbanks defined
};

class FloDisplaySnapshots : public FloDisplay, public RenameCallback
{
 public:
  FloDisplaySnapshots (Fweelin *app, int iid) : FloDisplay(iid), renamer(0),
    app(app), firstidx(0), numdisp(-1) {
    pthread_mutex_init (&snaps_lock,0);
  };
  ~FloDisplaySnapshots() { 
     pthread_mutex_destroy (&snaps_lock);
  };
  
  virtual FloDisplayType GetFloDisplayType() { return FD_Snapshots; };

  virtual void Draw(SDL_Surface *screen);

  ItemRenamer *renamer; // Renamer instance, or null if we are not renaming
  int rename_idx;       // Index of snapshot we are renaming
  virtual void ItemRenamed(char *nw); // Callback for renamer
  
  // Rename the snapshot with given index
  void Rename (int idx);

  inline void LockSnaps() {
    pthread_mutex_lock (&snaps_lock);
  };
  inline void UnlockSnaps() {
    pthread_mutex_unlock (&snaps_lock);
  };

  Fweelin *app;
  int firstidx,          // Index of first snapshot to display
    numdisp;             // Number of snapshots to display
  int sx, sy,            // Size of snapshots list
    margin;              // Margin for text
    
  protected:
  
  pthread_mutex_t snaps_lock; // A way to lock up snapshot display so two threads
                              // don't race on it
};

#endif
