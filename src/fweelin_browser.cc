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

#include "fweelin_core.h"
#include "fweelin_fluidsynth.h"
#include "fweelin_browser.h"

const double ItemRenamer::BLINK_DELAY = 0.5;

ItemRenamer::ItemRenamer (Fweelin *app, RenameCallback *cb, 
                          char *oldname) : app(app), cb(cb) {
  if (oldname == 0)
    strcpy(rename_tmpbuf,"");
  else if (strlen(oldname)+1 >= (unsigned int) RENAME_BUF_SIZE) {
    strncpy(rename_tmpbuf,oldname,RENAME_BUF_SIZE-1);
    rename_tmpbuf[RENAME_BUF_SIZE-1] = '\0';
  } else
    strcpy(rename_tmpbuf,oldname);

  if (app->getCFG()->AddEventHook(this)) {
    printf("RENAME: You can only rename one item at a time.\n");
    renaming = 0;
  }
  else {
    app->getSDLIO()->EnableUNICODE(1);
    app->getSDLIO()->EnableKeyRepeat(1);    
    renaming = 1;
  }
};

char ItemRenamer::HookEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_Input_Key :
    {
      KeyInputEvent *kev = (KeyInputEvent *) ev;

      // Intercept alphanumeric keys (use UNICODE translation)
      if ((kev->unicode > 32 &&
           kev->unicode < 127) || 
          (kev->unicode > 160 &&
           kev->unicode < 256)) {
        if (kev->down) 
          Rename_Append((char) (kev->unicode & 0xFF)); 
        return 1;
      } else if (kev->keysym == SDLK_BACKSPACE) {
        if (kev->down) 
          Rename_Backspace();
        return 1;
      } else if (kev->keysym == SDLK_SPACE) {
        if (kev->down)
          Rename_Append(' ');

      } else if (kev->keysym == SDLK_RETURN ||
                 kev->keysym == SDLK_KP_ENTER) {
        if (kev->down) {
          app->getCFG()->RemoveEventHook(this);
          app->getSDLIO()->EnableUNICODE(0);
          app->getSDLIO()->EnableKeyRepeat(0);

          // Notify callback that item has been renamed
          cb->ItemRenamed(rename_tmpbuf); // Callback may delete -this-
        }
        return 1;
      } else if (kev->keysym == SDLK_ESCAPE) {
        if (kev->down) {
          app->getCFG()->RemoveEventHook(this);
          app->getSDLIO()->EnableUNICODE(0);
          app->getSDLIO()->EnableKeyRepeat(0);

          // Stop
          cb->ItemRenamed(0); // Callback may delete -this-
        }
        return 1;
      }

      return 0;
    }
    break;

  default:
    break;
  }

  return 0;
};

// Add an item to this browser
// (Doubley linked list add with sort)
// Nonzero if we should sort (according to the BrowserItem::Compare() method)
void Browser::AddItem(BrowserItem *nw, char sort) {
  LockBrowser();
  
  if (first == 0) {
    first = nw;
    cur = first;
    // printf("BROWSER: (id: %d) First item now: %s\n",id,first->name);
  } else {
    BrowserItem *cur = first;
    if (sort && nw->Compare(cur) < 0) {
      // Insert before first if that's where she goes
      nw->next = first;
      nw->prev = 0;
      first->prev = nw;
      first = nw;       
    } else {
      // Insert at end or in the right place if sorting
      if (sort) {
        while (cur->next != 0 && nw->Compare(cur->next) >= 0)
          cur = cur->next;
      } else {
        while (cur->next != 0) 
          cur = cur->next;
      }
      
      // Insert after cur
      nw->next = cur->next;
      if (cur->next != 0)
        cur->next->prev = nw;
      nw->prev = cur;
      cur->next = nw;
    }
  }
  
  UnlockBrowser();
};

// Remove an item from this browser
// For each item we call the MatchItem(itemmatch) method in BrowserItem 
// until MatchItem returns zero. Then we remove that item.
void Browser::RemoveItem(int itemmatch) {
  LockBrowser();
  
  BrowserItem *curi = first;
  while (curi != 0 && curi->MatchItem(itemmatch))
    curi = curi->next;
  
  if (curi != 0) {
    // Found, remove
    if (curi->prev != 0)
      curi->prev->next = curi->next;
    else
      first = curi->next;
    
    if (curi->next != 0)
      curi->next->prev = curi->prev;
    
    // Move currently selected item if we just removed it
    if (cur == curi)
      cur = (curi->prev != 0 ? curi->prev : curi->next);
    
    delete curi;
  }
  
  UnlockBrowser();
};

void Browser::ItemRenamed(char *nw) {
  if (nw != 0) {
    // Assign new name and stop
    RenameItem(cur,nw);
        
    delete renamer;
    renamer = 0;

    // Notify callback of new name
    if (callback != 0)
      callback->ItemRenamed(cur);
    
    // If edit is blank, assign default name for onscreen display
    if (strlen(cur->name) == 0) {
      char tmp[FWEELIN_OUTNAME_LEN];

      if (GetType() == B_Loop) 
        GetDisplayName(((LoopBrowserItem *) cur)->filename,
                       &((LoopBrowserItem *) cur)->time,
                       tmp,FWEELIN_OUTNAME_LEN);
      else if (GetType() == B_Scene)
        GetDisplayName(((SceneBrowserItem *) cur)->filename,
                       &((SceneBrowserItem *) cur)->time,
                       tmp,FWEELIN_OUTNAME_LEN);

      RenameItem(cur,tmp);
      cur->default_name = 1;
    } else
      cur->default_name = 0;
  } else {
    // Rename was aborted
    delete renamer;
    renamer = 0;
  }
};

// Via this method, a browser is notified of a change to the on-disk name
// of an item. For example, when a Loop in memory is renamed, we also
// rename it on disk, and the loop browser must be notified of this new name.
void Browser::ItemRenamedOnDisk(char *old_filename, char *new_filename,
                                char *new_name) {
  if (old_filename == 0 ||
      new_filename == 0)
    return;

  BrowserItem *cur = first;
  char found = 0;
  while (cur != 0 && !found) {
    char *filename = 0;
    if (cur->GetType() == B_Loop) 
      filename = ((LoopBrowserItem *) cur)->filename;
    else if (cur->GetType() == B_Scene)
      filename = ((SceneBrowserItem *) cur)->filename;

    // printf("looking for oldfilename: %s .. cur: %s\n",
    //   old_filename, filename);
    if (filename != 0 && !strcmp(filename,old_filename))
      found = 1;
    else
      cur = cur->next;
  }

  if (found) {
    // Found this item in our browser-- rename in our browser

    // Assign new filename
    char *filename = 0;
    if (cur->GetType() == B_Loop) 
      filename = ((LoopBrowserItem *) cur)->filename;
    else if (cur->GetType() == B_Scene)
      filename = ((SceneBrowserItem *) cur)->filename;

    delete[] filename;
    filename = new char[strlen(new_filename)+1];
    strcpy(filename,new_filename);

    if (cur->GetType() == B_Loop) 
      ((LoopBrowserItem *) cur)->filename = filename;
    else if (cur->GetType() == B_Scene)
      ((SceneBrowserItem *) cur)->filename = filename;
    
    // Assign new name and stop
    RenameItem(cur,new_name);
        
    // If new name is blank, assign default name for onscreen display
    if (cur->name == 0 || strlen(cur->name) == 0) {
      char tmp[FWEELIN_OUTNAME_LEN];

      if (GetType() == B_Loop) 
        GetDisplayName(((LoopBrowserItem *) cur)->filename,
                       &((LoopBrowserItem *) cur)->time,
                       tmp,FWEELIN_OUTNAME_LEN);
      else if (GetType() == B_Scene)
        GetDisplayName(((SceneBrowserItem *) cur)->filename,
                       &((SceneBrowserItem *) cur)->time,
                       tmp,FWEELIN_OUTNAME_LEN);
      
      RenameItem(cur,tmp);
      cur->default_name = 1;
    }
    else
      cur->default_name = 0;
  }
};

// Get the onscreen display name for a file with given name. 
// (filename must refer to a file of the type this browser handles)
// Write the name to outbuf, with max maxlen characters.
// Returns nonzero if we used a 'default' name.
char Browser::GetDisplayName(char *filename, 
                             time_t *filetime,
                             char *outbuf, int maxlen) {
  // Loop exists, use combination of time and hash as name
  int baselen = strlen(app->getCFG()->GetLibraryPath()) + 1;
  if (btype == B_Loop)
    baselen += strlen(FWEELIN_OUTPUT_LOOP_NAME);
  else if (btype == B_Scene) 
    baselen += strlen(FWEELIN_OUTPUT_SCENE_NAME);
  else {
    printf("BROWSER: We don't support getting a display name for type %d\n",
           btype);
    return 1;
  }

  char sf_basename[FWEELIN_OUTNAME_LEN],
    sf_hash[FWEELIN_OUTNAME_LEN],
    sf_objname[FWEELIN_OUTNAME_LEN];

  if (Saveable::SplitFilename(filename, baselen, sf_basename, sf_hash, 
                              sf_objname,FWEELIN_OUTNAME_LEN) || 
      strlen(sf_objname) == 0) {
    // No object name given in filename
    // Compute default name
    char hashshort_1 = 'X',
      hashshort_2 = 'X';
    Saveable::GetHashFirst(filename,baselen,&hashshort_1,&hashshort_2);
    
    // Compose name for item
    snprintf(outbuf,maxlen,"%c%c %s",hashshort_1,hashshort_2,ctime(filetime));
    outbuf[maxlen-1] = '\0';
    outbuf[strlen(outbuf)-1] = '\0'; // Cut off return character

    return 1;
  } else {
    // Use name given in filename
    strncpy(outbuf,sf_objname,maxlen);
    outbuf[maxlen-1] = '\0';    

    return 0;
  }
}

Browser::~Browser() {
  if (app != 0) {
    app->getEMG()->UnlistenEvent(this,0,T_EV_BrowserMoveToItem);
    app->getEMG()->UnlistenEvent(this,0,T_EV_BrowserSelectItem);
  }

  ClearAllItems(); 

  pthread_mutex_destroy (&browser_lock);
};

void Browser::ClearAllItems() {
  LockBrowser();

  BrowserItem *cur = first;
  first = 0;
  while (cur != 0) {
    BrowserItem *tmp = cur->next;
    delete cur;
    cur = tmp;
  };
  
  UnlockBrowser();
};

void Browser::Setup(Fweelin *a, BrowserCallback *c) {
  app = a;
  callback = c;
  
  app->getEMG()->ListenEvent(this,0,T_EV_BrowserMoveToItem);
  app->getEMG()->ListenEvent(this,0,T_EV_BrowserMoveToItemAbsolute);
  app->getEMG()->ListenEvent(this,0,T_EV_BrowserSelectItem);
  app->getEMG()->ListenEvent(this,0,T_EV_BrowserRenameItem);
};

// Add divisions between browser items using the Compare function-
// whenever two neighbouring items have difference greater than maxdelta,
// a division is inserted. In browsing files, for example, this allows us
// to group files that where created close to one another in time
//
// We also put divisions around any items that have been given a unique
// name
void Browser::AddDivisions(int maxdelta) {
  if (first != 0) {
    BrowserItem *cur = first;
    char go = 1;
    do {
      while (cur->next != 0 && cur->next->Compare(cur) < maxdelta &&
             ((cur->default_name && cur->next->default_name) || 
              cur->GetType() == B_Division || 
              cur->next->GetType() == B_Division)) 
        cur = cur->next;

      if (cur->next != 0) {
        // Add a division between cur and cur->next
        BrowserItem *div = new BrowserDivision();
        div->prev = cur;
        div->next = cur->next;
        cur->next->prev = div;
        cur->next = div;

        cur = div->next;
      } else
        // Done
        go = 0;
    } while (go);
  }
};

void Browser::MoveTo(int adjust, int jumpadjust) {
  LockBrowser();

  if (xpand) {
    xpand_lastactivity = mygettime();
    xpanded = 1;
  }

  if (cur == 0)
    cur = first;
  
  if (cur != 0) {
    // Jump by jumpadjust
    int adjdir = (jumpadjust >= 0 ? 1 : -1),
      adjmag = (adjdir == 1 ? abs(jumpadjust) : abs(jumpadjust)+1);
    BrowserItem *prev = cur;
    for (int i = 0; cur != 0 && i < adjmag; i++) {
      prev = cur;

      while (cur != 0 && cur->GetType() != B_Division) 
        cur = (adjdir == 1 ? cur->next : cur->prev);
      if (cur != 0)
        // We are on the division, skip over it which way?
        cur = (adjdir == 1 || i+1 >= adjmag ? cur->next : cur->prev);
      if (cur == 0) {
        if (adjdir == -1 && i+1 >= adjmag) {
          // Going back to beginning of list
          cur = first;
        } else
          // Went too far, go back
          cur = prev;
      }
    }

    // Move by adjust
    adjdir = (adjust >= 0 ? 1 : -1);
    adjmag = abs(adjust);
    for (int i = 0; cur != 0 && i < adjmag; i++) {
      prev = cur;
      cur = (adjdir == 1 ? cur->next : cur->prev);
      if (cur != 0 && cur->GetType() == B_Division)
        i--; // Don't count divisions
    }
    if (cur == 0)
      cur = prev;
  } else {
    printf("BROWSER: No elements to move to for browser '%s'\n",
           GetTypeName(GetType()));
  }

  // Call browser virtual method for item browsed-
  // derived types of browsers may need to do internal updates
  ItemBrowsed(); 

  // Send out 'item-browsed' event-- which can be bound to
  // in the config file.
  BrowserItemBrowsedEvent *bievt = (BrowserItemBrowsedEvent *) 
    Event::GetEventByType(T_EV_BrowserItemBrowsed);  
  bievt->browserid = id;
  // printf("BROWSER: itembrowsed send %p id: %d\n",bievt,id);
  app->getEMG()->BroadcastEventNow(bievt, this);

  if (cur != 0 && callback != 0)
    callback->ItemBrowsed(cur);

  UnlockBrowser();
};

void Browser::Select() {
  if (cur != 0 && callback != 0)
    callback->ItemSelected(cur);
};

void Browser::Rename() {
  if (cur != 0 && renamer == 0 && 
      (GetType() == B_Loop || GetType() == B_Scene)) {
    renamer = new ItemRenamer(app,this,(cur->default_name ? 0 : cur->name));
    if (!renamer->IsRenaming()) {
      delete renamer;
      renamer = 0;
    }
  }
};

void Browser::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_BrowserMoveToItem :
    {
      BrowserMoveToItemEvent *bev = (BrowserMoveToItemEvent *) ev;
      
      // Meant for this browser?
      if (bev->browserid == id) {
        if (CRITTERS)
          printf("BROWSER: Received BrowserMoveToItem "
                 "(browser id: %d, adjust: %d, jumpadjust: %d)\n",
                 bev->browserid, bev->adjust, bev->jumpadjust);
        
        MoveTo(bev->adjust, bev->jumpadjust);
      }
    }
    break;

  case T_EV_BrowserMoveToItemAbsolute :
    {
      BrowserMoveToItemAbsoluteEvent *bev = 
        (BrowserMoveToItemAbsoluteEvent *) ev;
      
      // Meant for this browser?
      if (bev->browserid == id) {
        if (CRITTERS)
          printf("BROWSER: Received BrowserMoveToItemAbsolute "
                 "(browser id: %d, idx: %d)\n",
                 bev->browserid, bev->idx);
        
        MoveToBeginning();
        MoveTo(bev->idx, 0);
      }
    }
    break;

  case T_EV_BrowserSelectItem :
    {
      BrowserSelectItemEvent *bev = (BrowserSelectItemEvent *) ev;
      
      // Meant for this browser?
      if (bev->browserid == id) {
        if (CRITTERS)
          printf("BROWSER: Received BrowserSelectItem\n");
        
        Select();
      }
    }
    break;

  case T_EV_BrowserRenameItem :
    {
      BrowserRenameItemEvent *bev = (BrowserRenameItemEvent *) ev;
      
      // Meant for this browser?
      if (bev->browserid == id) {
        if (CRITTERS)
          printf("BROWSER: Received BrowserRenameItem\n");
        
        Rename();
      }
    }
    break;

  default:
    break;
  }
}

LoopTray::~LoopTray() {
  if (app != 0) {
    app->getEMG()->UnlistenEvent(this,0,T_EV_TriggerSet);
    app->getEMG()->UnlistenEvent(this,0,T_EV_RenameLoop);
  }
};

void LoopTray::Setup(Fweelin *a, BrowserCallback *c) {
  Browser::Setup(a,c);

  basepos = app->getCFG()->XCvt(0.016);
  iconsize = app->getCFG()->XCvt(0.03);
  xpanded = xpand;

  app->getEMG()->ListenEvent(this,0,T_EV_TriggerSet);
  app->getEMG()->ListenEvent(this,0,T_EV_RenameLoop);
};

void LoopTray::ReceiveEvent(Event *ev, EventProducer *from) {
  switch (ev->GetType()) {
  case T_EV_TriggerSet :
    {
      TriggerSetEvent *tev = (TriggerSetEvent *) ev;
      // printf("tmap set: %d->%p\n",tev->idx,tev->nw);

      if (tev->nw == 0) {
        // Delete,
        // Scan for a LoopTrayItem corresponding to this index
        RemoveItem(tev->idx);
        touchtray = 1;
      } else {
        // Get the placename- where the loop is mapped
        char *pname = 0;
        FloLayout *curl = app->getCFG()->GetLayouts();
        while (curl != 0) {
          if (tev->idx >= curl->loopids.lo &&
              tev->idx <= curl->loopids.hi) {
            // Loop is mapped to an element in this layout- get the name
            int firstid = curl->loopids.lo;
            FloLayoutElement *curel = curl->elems;
            while (curel != 0 && firstid + curel->id != tev->idx)
              curel = curel->next;

            if (curel != 0)
              pname = curel->name;
          }

          curl = curl->next;
        }

        // Add,
        LoopTrayItem *nw = new LoopTrayItem(tev->nw,tev->idx,
                                            tev->nw->name,
                                            (tev->nw->name == 0 ? 1 : 0),
                                            pname);
        AddItem(nw,1);
        touchtray = 1;
      }
    }
    break;

  case T_EV_RenameLoop :
    {
      RenameLoopEvent *rev = (RenameLoopEvent *) ev;

      if (rev->in == 0) {
        Rename(rev->loopid);
        if (CRITTERS)
          printf("LOOPTRAY: Received RenameLoop(loopid: %d)\n",rev->loopid);    
      }
    }
    break;

  default:
    // Let base class handle it
    Browser::ReceiveEvent(ev,from);
    break;
  }
};

// A loop was renamed from outside the loop tray
// (loops can be renamed in layouts)
// (or on disk)
void LoopTray::ItemRenamedFromOutside(Loop *l, char *nw) {
  if (l != 0 && nw != 0) {
    // Find the item that has been renamed
    {
      LoopTrayItem *curl = (LoopTrayItem *) first;
      while (curl != 0 && curl->l != l)
        curl = (LoopTrayItem *) curl->next;
      
      cur = curl;
    }

    if (cur != 0) {
      // Assign new name
      RenameItem(cur,nw);
      cur->default_name = 0;
      touchtray = 1;
    }
  }
};

void LoopTray::ItemRenamed(char *nw) {
  if (nw != 0) {
    // Assign new name and stop
    RenameItem(cur,nw);
    delete renamer;
    renamer = 0;

    // Notify callback of new name
    if (callback != 0)
      callback->ItemRenamed(cur);

    cur->default_name = 0;
    touchtray = 1;
  } else {
    // Rename was aborted
    delete renamer;
    renamer = 0;
  }
};

void LoopTray::Rename(int loopid) {
  if (renamer == 0) {
    // Find the LoopTrayItem with the given 'loopid'
    {
      LoopTrayItem *curl = (LoopTrayItem *) first;
      while (curl != 0 && curl->loopid != loopid)
        curl = (LoopTrayItem *) curl->next;
      
      cur = curl;
    }
    
    if (cur != 0) {
      if (CRITTERS)
        printf("RENAME: Item: %p\n",cur);
      renamer = new ItemRenamer(app,this,(cur->default_name ? 0 : cur->name));
      if (!renamer->IsRenaming()) {
        delete renamer;
        renamer = 0;
      }
    }
  }
};

char LoopTray::MouseMotion(MouseMotionInputEvent *mev) {
  if (resize_win != RS_Off) {
    int xdelta = mev->x - resize_xhand,
      ydelta = mev->y - resize_yhand;
      
    if (resize_win == RS_Move) {
      xpand_x1 = old_xpand_x1 + xdelta;
      xpand_y1 = old_xpand_y1 + ydelta;
      xpand_x2 = old_xpand_x2 + xdelta;
      xpand_y2 = old_xpand_y2 + ydelta;
    } else {
      if ((resize_win == RS_Right || resize_win == RS_TopRight ||
           resize_win == RS_BottomRight) &&
          xpand_x1+basepos*2 <= old_xpand_x2+xdelta) {
        xpand_x2 = old_xpand_x2 + xdelta;
      } else if ((resize_win == RS_Left || resize_win == RS_TopLeft ||
                  resize_win == RS_BottomLeft) &&
                 old_xpand_x1+xdelta+basepos*2 <= xpand_x2) {
        xpand_x1 = old_xpand_x1 + xdelta;
      }

      if ((resize_win == RS_Bottom || resize_win == RS_BottomLeft ||
           resize_win == RS_BottomRight) &&
          xpand_y1+basepos*2 <= old_xpand_y2+ydelta) {
        xpand_y2 = old_xpand_y2 + ydelta;
      } else if ((resize_win == RS_Top || resize_win == RS_TopLeft ||
                  resize_win == RS_TopRight) &&
                 old_xpand_y1+ydelta+basepos*2 <= xpand_y2) {
        xpand_y1 = old_xpand_y1 + ydelta;
      }

      touchtray = 1;
    }

    return 1;
  }

  return 0;
}

char LoopTray::MouseButton(MouseButtonInputEvent *mev) {
  if (resize_win != RS_Off && mev->button == resize_button &&
      !mev->down) {
    // Mouse release on resize
    resize_win = RS_Off;
    return 1;
  }

  int mx = mev->x, my = mev->y;

  // Check if mouse event inside tray
  if (mx >= xpand_x1 && mx <= xpand_x2 &&
      my >= xpand_y1 && my <= xpand_y2) {
    // Mouse inside tray
    if (mx > xpand_x1+basepos && my > xpand_y1+basepos &&
        mx < xpand_x2-basepos && my < xpand_y2-basepos) {
      // Mouse in inner region
      if (mev->down) {
        // Check if the mouse is clicking in any loop
        LoopTrayItem *curl = (LoopTrayItem *) first;
        char go = 1, found = 0;
        while (curl != 0 && go) {
          if (curl->xpos != -1) {
            int relx = mx-xpand_x1-(curl->xpos+loopmap->map_xs/2),
              rely = my-xpand_y1-(curl->ypos+loopmap->map_ys/2);
            if (relx*relx+rely*rely <= loopsize*loopsize/4) {
              // Mouse is clicking within this loop
              found = 1;

              // Issue 'loop clicked' event
              LoopClickedEvent *lcevt = (LoopClickedEvent *) 
                Event::GetEventByType(T_EV_LoopClicked);
              
              lcevt->button = mev->button;
              lcevt->down = mev->down;
              lcevt->loopid = curl->loopid;
              lcevt->in = 0; // In=0 means clicked in looptray
              app->getEMG()->BroadcastEvent(lcevt, this);
            } 
          } else
            go = 0;
          
          curl = (LoopTrayItem *) curl->next;
        }

        if (!found) {
          // Not clicking inside any loop--

          // Start moving
          resize_win = RS_Move;
          resize_button = mev->button;
          resize_xhand = mx;
          resize_yhand = my;
          old_xpand_x1 = xpand_x1;
          old_xpand_y1 = xpand_y1;
          old_xpand_x2 = xpand_x2;
          old_xpand_y2 = xpand_y2;
        }
      }
    } else {
      // Mouse in border region
      if (mev->down) {
        // Start resizing
        char bord_l = 0,
          bord_lm = 0,
          bord_r = 0,
          bord_rm = 0,
          bord_t = 0,
          bord_tm = 0,
          bord_b = 0,
          bord_bm = 0;
        int bord_mid_thresh = basepos*3;

        if (mx >= xpand_x1 && mx <= xpand_x1+basepos) {
          bord_l = 1;
          bord_lm = 1;
        } else if (mx >= xpand_x2-basepos && mx <= xpand_x2) {
          bord_r = 1;
          bord_rm = 1;
        } else if (mx >= xpand_x1 && mx <= xpand_x1+bord_mid_thresh) {
          bord_lm = 1;
        } else if (mx >= xpand_x2-bord_mid_thresh && mx <= xpand_x2) {
          bord_rm = 1;
        }

        if (my >= xpand_y1 && my <= xpand_y1+basepos) {
          bord_t = 1;
          bord_tm = 1;
        } else if (my >= xpand_y2-basepos && my <= xpand_y2) {
          bord_b = 1;
          bord_bm = 1;
        } else if (my >= xpand_y1 && my <= xpand_y1+bord_mid_thresh) {
          bord_tm = 1;
        } else if (my >= xpand_y2-bord_mid_thresh && my <= xpand_y2) {
          bord_bm = 1;
        }

        if (bord_l) {
          if (bord_tm)
            resize_win = RS_TopLeft;
          else if (bord_bm)
            resize_win = RS_BottomLeft;
          else
            resize_win = RS_Left;
        } else if (bord_r) {
          if (bord_tm)
            resize_win = RS_TopRight;
          else if (bord_bm)
            resize_win = RS_BottomRight;
          else
            resize_win = RS_Right;
        } else if (bord_t) {
          if (bord_lm)
            resize_win = RS_TopLeft;
          else if (bord_rm)
            resize_win = RS_TopRight;
          else
            resize_win = RS_Top;
        } else if (bord_b) {
          if (bord_lm)
            resize_win = RS_BottomLeft;
          else if (bord_rm)
            resize_win = RS_BottomRight;
          else
            resize_win = RS_Bottom;
        } else {
          printf("BROWSER: Error in mouse border algorithm!\n");
          resize_win = RS_Off;
        }
          
        resize_button = mev->button;
        resize_xhand = mev->x;
        resize_yhand = mev->y;
        old_xpand_x1 = xpand_x1;
        old_xpand_y1 = xpand_y1;
        old_xpand_x2 = xpand_x2;
        old_xpand_y2 = xpand_y2;
      } 
    }

    // Eat event
    return 1;
  } else if (mx >= xpos && mx <= xpos+iconsize &&
             my >= ypos && my <= ypos+iconsize) {
    if (mev->down) {
      // Mouse clicked on icon
      xpanded = (xpanded ? 0 : 1);
      touchtray = 1;
    }

    return 1;
  } else {
    // Mouse outside tray, ignore
    return 0;
  }
};

PatchBrowser::~PatchBrowser() {
  // Invoke our special ClearAllItems method
  ClearAllItems(); 

  if (app != 0) {
    app->getEMG()->UnlistenEvent(this,0,T_EV_PatchBrowserMoveToBank);
    app->getEMG()->UnlistenEvent(this,0,T_EV_PatchBrowserMoveToBankByIndex);
  }
};

void PatchBrowser::ClearAllItems() {
  LockBrowser();

  cur = 0;
  first = 0;

  // Delete all items in all patch banks
  PatchBank *curpb = pb_first;
  pb_first = 0;
  while (curpb != 0) {
    BrowserItem *cur = curpb->first;

    curpb->first = 0;
    curpb->cur = 0;

    while (cur != 0) {
      BrowserItem *tmp = cur->next;
      delete cur;
      cur = tmp;
    }

    curpb = curpb->next;
  };
  
  // *** Do we need to clear all patchbanks too?

  UnlockBrowser();
};

void PatchBrowser::PB_MoveTo (int direction) {
  LockBrowser();
  
  if (xpand) {
    xpand_lastactivity = mygettime();
    xpanded = 1;
  }

  // Update pb_cur
  if (pb_cur != 0) {
    pb_cur->cur = cur;
    pb_cur->first = first;
  } else
    pb_cur = pb_first;

  if (pb_cur != 0) {
    if (direction > 0) {
      // Forward
      if (pb_cur->next != 0) 
        pb_cur = pb_cur->next;
    } else {
      // Slow way to move backwards in a singly linked list
      PatchBank *tmp = pb_first;
      while (tmp != 0 && tmp->next != pb_cur) 
        tmp = tmp->next;
      if (tmp != 0)
        pb_cur = tmp;
    }
  
    // Assign patch 'cur' and 'first' pointers based on this patch bank
    first = pb_cur->first;
    cur = pb_cur->cur;

    pb_cur_tag = pb_cur->tag;
    // Don't update when moving
    // SetMIDIForPatch();
  } else
    pb_cur_tag = -1;


  UnlockBrowser();
};

void PatchBrowser::PB_MoveToIndex (int index) {
  LockBrowser();
  
  if (xpand) {
    xpand_lastactivity = mygettime();
    xpanded = 1;
  }

  // Update pb_cur
  if (pb_cur != 0) {
    pb_cur->cur = cur;
    pb_cur->first = first;
  } else
    pb_cur = pb_first;

  // Count to right patchbank
  pb_cur = pb_first;
  for (int i = 0; i < index && pb_cur != 0; i++, pb_cur = pb_cur->next);
  
  if (pb_cur == 0)
    pb_cur = pb_first;
  else {
    // Assign patch 'cur' and 'first' pointers based on this patch bank
    first = pb_cur->first;
    cur = pb_cur->cur;

    // Don't update when moving
    // SetMIDIForPatch();
  }

  if (pb_cur != 0)
    pb_cur_tag = pb_cur->tag;
  else
    pb_cur_tag = -1;

  UnlockBrowser();
};

void PatchBrowser::Setup(Fweelin *a, BrowserCallback *c) {
  Browser::Setup(a,c);
  app->getEMG()->ListenEvent(this,0,T_EV_PatchBrowserMoveToBank);
  app->getEMG()->ListenEvent(this,0,T_EV_PatchBrowserMoveToBankByIndex);
};

void PatchBrowser::ReceiveEvent(Event *ev, EventProducer *from) {
  switch (ev->GetType()) {
  case T_EV_PatchBrowserMoveToBank :
    {
      PatchBrowserMoveToBankEvent *pbev = (PatchBrowserMoveToBankEvent *) ev;
      
      PB_MoveTo(pbev->direction);
      if (CRITTERS)
        printf("PATCH BROWSER: Received PatchBrowserMoveToBank "
               "(direction: %d)\n", pbev->direction);
    }
    break;

  case T_EV_PatchBrowserMoveToBankByIndex :
    {
      PatchBrowserMoveToBankByIndexEvent *pbev = 
        (PatchBrowserMoveToBankByIndexEvent *) ev;
      
      PB_MoveToIndex(pbev->index);
      if (CRITTERS)
        printf("PATCH BROWSER: Received PatchBrowserMoveToBankByIndex "
               "(index: %d)\n", pbev->index);
    }
    break;

  default:
    // Let base class handle it
    Browser::ReceiveEvent(ev,from);
    break;
  }
};

// Sets the right MIDI port(s) and channel(s) for echo based on this
// patches' settings
void PatchBrowser::SetMIDIForPatch() {
  /* printf("pb cur: %p port: %d\n",pb_cur,pb_cur->port);
  printf("cur: %p\n",cur);
  printf("app: %p\n",app);
  if (app != 0) 
    printf("getmidi: %p\n",app->getMIDI()); */

  if (app != 0 && app->getMIDI() != 0)
    app->getMIDI()->SetMIDIForPatch((pb_cur == 0 ? 0 : pb_cur->port),
                                    (cur != 0 && cur->GetType() == B_Patch ?
                                    (PatchItem *) cur : 0));
};

void FloDisplaySnapshots::Rename (int idx) {
  Snapshot *s = app->getSNAP(idx);
  if (renamer == 0 && s != 0 && s->exists) {
    renamer = new ItemRenamer(app,this,s->name);
    rename_idx = idx;
    
    if (!renamer->IsRenaming()) {
      delete renamer;
      renamer = 0;
    }
    
    // Keep display showing while we interactively rename
    forceshow = 1;
  }
};

void FloDisplaySnapshots::ItemRenamed(char *nw) {
  forceshow = 0;
  
  if (nw != 0) {
    // printf("NEW SNAPSHOT NAME: %s\n",nw);
    
    Snapshot *s = app->getSNAP(rename_idx);
    if (s != 0) {
      LockSnaps();
      if (s->name != 0) 
        delete[] s->name;
      if (nw != 0) {
        s->name = new char[strlen(nw)+1];
        strcpy(s->name,nw);
      } else
        s->name = 0;
      UnlockSnaps();
    }
      
    delete renamer;
    renamer = 0;
  } else {
    // Rename was aborted
    delete renamer;
    renamer = 0;
  }
};


