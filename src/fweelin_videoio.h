#ifndef __FWEELIN_VIDEOIO_H
#define __FWEELIN_VIDEOIO_H

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

#include <SDL/SDL.h>

#ifdef __MACOSX__
#include <SDL_gfx/SDL_gfxPrimitives.h>
#include <SDL_ttf/SDL_ttf.h>
#include "FweelinMac.h"
#else
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>
#endif

#ifdef __MACOSX__
#define CAPITAL_FILLED_PIE
#endif

// Different versions of sdl-gfx have different naming
#ifdef CAPITAL_FILLED_PIE
#define FILLED_PIE filledPieRGBA
#else
#define FILLED_PIE filledpieRGBA
#endif

// Convert from 640,480 screen size to the user's coordinate system
#define OCX(x) (app->getCFG()->XCvt((float)(x)/640))
#define OCY(y) (app->getCFG()->YCvt((float)(y)/480))

#define FWEELIN_TITLE_IMAGE FWEELIN_DATADIR "/fweelin.bmp"

extern double mygettime(void);
extern int myround(float num);

class Fweelin;
class LoopManager;
class FloConfig;
class FloLayoutElement;

// This crazy class computes a mapping
// to bend a flat strip bitmap into a circle-- good for makin wheels!
class CircularMap {
public:
  // Create a mapping from the flat surface 'in' to a circular region
  // of the given dimensions-- the output surface location is specified when
  // actually mapping
  CircularMap(SDL_Surface *in, 
              int map_xs, int map_ys,
              int in_xs, int in_ys,
              int rinner, int rsize);
  // Frees this map
  ~CircularMap();
  
  // Map flat scope onto circle
  char Map(SDL_Surface *out, int dstx, int dsty);

  // Scans for a map with map_xs == sz
  CircularMap *Scan(int sz) {
    CircularMap *cur = this;
    while (cur != 0 && cur->map_xs != sz)
      cur = cur->next;
    return cur;
  };
  // Links the given map to the end of this map list
  void Link(CircularMap *nw) {
    CircularMap *cur = this;
    while (cur->next != 0)
      cur = cur->next;
    cur->next = nw;
  };

  SDL_Surface *in;
  Uint8 **map;
  int *scanmap;
  int map_xs, map_ys,
    in_xs, in_ys,
    rinner, rsize;

  CircularMap *next;
};

// Video Handler
class VideoIO : public EventProducer, public EventListener {
  friend class Fweelin;

public:
  VideoIO (Fweelin *app) : app(app), screen(0), cmaps(0), 
    showlooprange(0,0), showhelppage(0), cur_iid(0), videothreadgo(0) {};

  virtual ~VideoIO() {};

  int activate ();
  void close ();

  char IsActive () { return videothreadgo; };

  double GetVideoTime() { return video_time; };

  void SetVideoMode(char fullscreen);
  char GetVideoMode() { return fullscreen; };

  void ReceiveEvent(Event *ev, EventProducer *from);

  // Draw text, and optionally return size of text drawn in sx and sy
  static int draw_text(SDL_Surface *out, TTF_Font *font,
                       char *str, int x, int y, SDL_Color clr, char centerx = 0,
                       char centery = 0, int *sx = 0, int *sy = 0);

  // If no suitable map exists in list 'cmaps', creates a planar>circular map
  // of diameter 'sz', mapping from the given surface. 
  CircularMap *CreateMap(SDL_Surface *lscopepic, int sz);

  inline SDL_Surface *getLSCOPEPIC() { return lscopepic; };

  // Draw the circular scope and status information for one loop--
  // given by loopid 'i' and optional layout element 'curel'..
  // if curel is null, we draw direct with the given circular map
  // and position.
  // Returns nonzero if loop not drawn
  char DrawLoop(LoopManager *loopmgr, int i, 
                SDL_Surface *screen, SDL_Surface *lscopepic,
                SDL_Color *loopcolors, float colormag,
                FloConfig *fs, FloLayoutElement *curel,
                
                CircularMap *direct_map, int direct_xpos,
                int direct_ypos,
                
                float lvol,
                char drawtext = 1);

 protected:

  // Core app
  Fweelin *app;

  // Video event handler thread
  static void *run_video_thread (void *ptr);
  void video_event_loop ();

  // This is a custom surface blitter that doesn't use large block
  // memory writes and thus avoids the strange video glitch of introducing
  // audio pops on some machines-- some loss in performance!
  void Custom_BlitSurface(SDL_Surface *in, SDL_Surface *out,
                          SDL_Rect *dstrect);

  void Squeeze_BlitSurface(SDL_Surface *in, SDL_Surface *out,
                           SDL_Rect *dstrect);

#ifdef __MACOSX__
  FweelinMac cocoa; // Cocoa thread setup
#endif
  
  char fullscreen; // Fullscreen video?

  SDL_Surface *screen;
  
  // Pointers to fonts that video uses - links into FloFont structures
  TTF_Font *mainfont, *helpfont, *smallfont, *tinyfont;

  // Planar->Circular mapping variables
  CircularMap *cmaps;
  int lscopewidth, lscopeheight;
  SDL_Surface *lscopepic;

  nframes_t *curpeakidx,
    *lastpeakidx;
  float *oldpeak;

  Range showlooprange; // Which loops to show onscreen
  int showhelppage, // Which help page to show (0=off)
    numhelppages;   // Number of help pages that are defined

  int cur_iid;      // ID of currently visible (switchable) interface
  
  // Length of time taken (s) for one iteration of video loop
  double video_time;

  pthread_t video_thread;
  char videothreadgo;

  pthread_mutex_t video_thread_lock;
  
#ifdef LCD_DISPLAY
#define LCD_ROWS 4
#define LCD_COLS 20

  // Initialize LCD connection
  int InitLCD (char *devname, int baud);

  // Writing directly to LCD
  inline void LCD_Send (unsigned char dat) {
    if (lcd_handle)
      while (write(lcd_handle, &dat, 1) != 1) {
        printf("LCD_Send failed\n");
        usleep(1000);
      }
  };
  
  inline void LCD_SendStr (char *data) {
    while (*data)
      LCD_Send(*data++);
  }
  
  // Writing to LCD via double buffer (more efficient)
  inline void LCDB_SetChar (int row, int col, char dat) {
    if (row >= 0 && row < LCD_ROWS && col >= 0 && col < LCD_COLS)
      lcd_dat[lcd_curb][row][col] = dat;
  };

  inline void LCDB_SetStr (int row, int col, char *data) {
    while (*data)
      LCDB_SetChar(row,col++,*data++);
  }
  
  inline void LCDB_Clear () {
    memset(lcd_dat[lcd_curb],' ',sizeof(char) * LCD_ROWS * LCD_COLS);
  };

  inline void LCDB_Debug () {
    char buf[LCD_COLS+1];
    buf[LCD_COLS] = '\0';
    for (int r = 0; r < LCD_ROWS; r++) {
      strncpy(buf,lcd_dat[lcd_curb][r],LCD_COLS);
      printf("%s\n",buf);
    }
  };

  // Dump buffer to LCD, writing any changes, then switch buffer
  void LCDB_Dump ();

  // Handle for LCD output
  int lcd_handle;
  char lcd_dat[2][LCD_ROWS][LCD_COLS]; // Double buffer for LCD screen
  int lcd_curb;                        // Which of two double buffers is 
                                       // being written to
#endif
};

#endif
