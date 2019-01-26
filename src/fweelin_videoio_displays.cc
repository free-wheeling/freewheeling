/*
   Art is the beginning.
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

#include <sys/stat.h>
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

#include "fweelin_videoio.h"
#include "fweelin_core.h"
#include "fweelin_paramset.h"
#include "fweelin_logo.h"

void FloDisplayPanel::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };

  boxRGBA(screen,
          xpos,ypos,xpos+sx,ypos+sy,
          0,0,0,190);
  vlineRGBA(screen,xpos,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,xpos+sx,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font == 0 || font->font == 0) {
    printf("VIDEO: WARNING: No font specified for parameter set.\n");
    return;
  }

  int textheight = 0;
  TTF_SizeText(font->font,VERSION,0,&textheight);

  // Draw title
  if (title != 0)
    VideoIO::draw_text(screen,font->font,
                       title,xpos+sx-margin,ypos,titleclr,2,0);
}

void LoopTray::Draw(SDL_Surface *screen) {
  const static SDL_Color borderclr = {100, 100, 90, 0};

  LockBrowser();

  // **** Add event to change placenames when 'video-show-loop' is called

  VideoIO *vid = app->getVIDEO();

  // Generate circular map for loop tray
  if (loopmap == 0)
    loopmap = vid->CreateMap(vid->getLSCOPEPIC(),loopsize);

  // Draw iconified version
  boxRGBA(screen,xpos,ypos,xpos+iconsize,ypos+iconsize,
          borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+iconsize,ypos,40,40,40,255);
  hlineRGBA(screen,xpos,xpos+iconsize,ypos+iconsize,40,40,40,255);
  vlineRGBA(screen,xpos,ypos,ypos+iconsize,40,40,40,255);
  vlineRGBA(screen,xpos+iconsize,ypos,ypos+iconsize,40,40,40,255);
  FILLED_PIE(screen,xpos+iconsize/2,ypos+iconsize/2,
             iconsize*3/8,
             30,359,
             0xF9,0xE6,0x13,255);
  circleRGBA(screen,xpos+iconsize/2,ypos+iconsize/2,
             iconsize*3/8,
             40,40,40, 255); // Outline

  if (xpanded) {
    // Draw background
    {
      int xpand_yb1 = xpand_y1+basepos,
        xpand_yb2 = xpand_y2-basepos,
        xpand_xb1 = xpand_x1+basepos,
        xpand_xb2 = xpand_x2-basepos;

      boxRGBA(screen,xpand_x1,xpand_y1,xpand_xb1,xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_xb1,xpand_y1,xpand_x2,xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_x1,xpand_y1,xpand_x2,xpand_yb1,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_x1,xpand_yb2,xpand_x2,xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_xb1,xpand_yb1,xpand_xb2,xpand_yb2,0,0,0,255);
    }

    hlineRGBA(screen,xpand_x1,xpand_x2,xpand_y1,40,40,40,255);
    hlineRGBA(screen,xpand_x1,xpand_x2,xpand_y2,40,40,40,255);
    vlineRGBA(screen,xpand_x1,xpand_y1,xpand_y2,40,40,40,255);
    vlineRGBA(screen,xpand_x2,xpand_y1,xpand_y2,40,40,40,255);

    LoopTrayItem *curl = (LoopTrayItem *) first;

    // If necessary, recalc positions for loops
    if (touchtray) {
      char go = 1;
      int curx = basepos, cury = basepos;
      int loopjump = loopsize+basepos;

      // Space for loops?
      if (curx >= xpand_x2-xpand_x1-loopjump ||
          cury >= xpand_y2-xpand_y1-loopjump)
        go = 0;

      while (curl != 0) {
        if (go) {
          curl->xpos = curx;
          curl->ypos = cury;

          // Move to next spot
          curx += loopjump;
          if (curx >= xpand_x2-xpand_x1-loopjump) {
            curx = basepos;
            cury += loopjump;
            if (cury >= xpand_y2-xpand_y1-loopjump)
              go = 0;
          }
        } else {
          curl->xpos = -1;
          curl->ypos = -1;
        }

        curl = (LoopTrayItem *) curl->next;
      }

      touchtray = 0;
    }

    // Draw items
    curl = (LoopTrayItem *) first;
    char go = 1;
    while (curl != 0 && go) {
      if (curl->xpos != -1)
        Draw_Item(screen,curl,xpand_x1+curl->xpos,xpand_y1+curl->ypos);
      else
        go = 0;

      curl = (LoopTrayItem *) curl->next;
    }
  }

  UnlockBrowser();
};

void LoopTray::Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y) {
  const static float loop_colorbase = 0.5;
  const static SDL_Color white = { 0xEF, 0xAF, 0xFF, 0 };
  const static SDL_Color cursorclr = { 0xEF, 0x11, 0x11, 0 };
  static SDL_Color loop_color[4] = { { 0x62, 0x62, 0x62, 0 },
                                     { 0xF9, 0xE6, 0x13, 0 },
                                     { 0xFF, 0xFF, 0xFF, 0 },
                                     { 0xE0, 0xDA, 0xD5, 0 } };

  LoopManager *loopmgr = app->getLOOPMGR();
  LoopTrayItem *li = (LoopTrayItem *) i;

  float colormag;
  char loopexists;
  if (loopmgr->GetSlot(li->loopid) || loopmgr->IsActive(li->loopid)) {
    loopexists = 1;
    colormag = loop_colorbase + loopmgr->GetTriggerVol(li->loopid);
    if (colormag > 1.0)
      colormag = 1.0;
  } else {
    loopexists = 0;
    colormag = loop_colorbase;
  }

  // Draw loop
  if (loopexists) {
    if (!app->getVIDEO()->
        DrawLoop(loopmgr,li->loopid,screen,app->getVIDEO()->getLSCOPEPIC(),
                 loop_color,colormag,app->getCFG(),0,loopmap,x,y,
                 app->getMASTERLIMITER()->GetLimiterVolume(),0)) {
      // Place name
      if (li->placename != 0)
        VideoIO::draw_text(screen,font->font,li->placename,x,y,white);

      // Loop name
      if (xpand_liney == -1)
        TTF_SizeText(font->font,VERSION,0,&xpand_liney);

      // printf("cur: %p\n",cur);
      if (i == cur && renamer != 0) {
        RenameUIVars *rui = renamer->UpdateUIVars();

        // Draw text with cursor
        int sx, sy;
        int txty = y+loopsize-xpand_liney;
        char *curn = renamer->GetCurName();
        if (*curn != '\0')
          VideoIO::draw_text(screen,font->font,curn,
                             x,txty,white,0,0,&sx,&sy);
        else {
          sx = 0;
          sy = xpand_liney;
        }

        if (rui->rename_cursor_toggle)
          boxRGBA(screen,
                  x+sx,txty,
                  x+sx+sy/2,txty+sy,
                  cursorclr.r,cursorclr.g,cursorclr.b,255);
      } else if (li->name != 0)
        VideoIO::draw_text(screen,font->font,li->name,
                           x,y+loopsize-xpand_liney,
                           white);
    }
  }

  // Browser::Draw_Item(screen,i,x,y);
};

// Draw browser display
void Browser::Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y) {
  const static SDL_Color white = { 0xEF, 0xAF, 0xFF, 0 };
  const static SDL_Color cursorclr = { 0x77, 0x77, 0x77, 0 };
  const static unsigned int tmp_size = 256;
  static char tmp[tmp_size];

  if (font != 0 && font->font != 0 && i != 0) {
    switch (i->GetType()) {
    case B_Patch :
      {
        PatchItem *p = (PatchItem *) i;

        // Current patch
        snprintf(tmp,tmp_size,"%02d: %s",p->id,p->name);
        VideoIO::draw_text(screen,font->font,tmp,x,y,white);
      }
      break;

    case B_Division :
      break;

    default :
      {
        if (i->GetType() == B_Loop)
          snprintf(tmp,tmp_size,"%s-",FWEELIN_OUTPUT_LOOP_NAME);
        else if (i->GetType() == B_Scene)
          snprintf(tmp,tmp_size,"%s-",FWEELIN_OUTPUT_SCENE_NAME);
        else
          tmp[0] = '\0';

        if (i == cur && renamer != 0) {
          RenameUIVars *rui = renamer->UpdateUIVars();

          strncat(tmp,renamer->GetCurName(),tmp_size-1);
          tmp[tmp_size-1] = '\0';

          // Draw text with cursor
          int sx, sy;
          VideoIO::draw_text(screen,font->font,tmp,x,y,white,0,0,
                             &sx,&sy);

          if (rui->rename_cursor_toggle)
            boxRGBA(screen,
                    x+sx,y,
                    x+sx+sy/2,y+sy,
                    cursorclr.r,cursorclr.g,cursorclr.b,255);
        } else if (i->name != 0) {
          strncat(tmp,i->name,tmp_size-1);
          tmp[tmp_size-1] = '\0';
          VideoIO::draw_text(screen,font->font,tmp,x,y,white);
        }
      }
      break;
    }
  }
};

// Draw browser display
void Browser::Draw(SDL_Surface *screen) {
  LockBrowser();

  if (xpanded) {
    // Draw expanded view

    // Dim the background
    for (int i = xpand_y1; i <= xpand_y2; i++)
      hlineRGBA(screen,xpand_x1,xpand_x2,i,0,0,0,200);
    hlineRGBA(screen,xpand_x1,xpand_x2,xpand_y1,127,127,127,255);
    hlineRGBA(screen,xpand_x1,xpand_x2,xpand_y2,127,127,127,255);
    vlineRGBA(screen,xpand_x1,xpand_y1,xpand_y2,127,127,127,255);
    vlineRGBA(screen,xpand_x2,xpand_y1,xpand_y2,127,127,127,255);

    if (cur != 0) {
      BrowserItem *sp_1 = cur,
        *sp_2 = cur;

      // Compute text height and center of expanded window-- once!
      if (xpand_liney == -1) {
        TTF_SizeText(font->font,VERSION,0,&xpand_liney);
        xpand_centery = (xpand_y1+xpand_y2)/2;
        xpand_spread = MIN(xpand_centery-xpand_y1,
                           xpand_y2-xpand_centery);
        xpand_spread /= xpand_liney;

        // printf("compute xpand_liney: %d xpand_centery: %d\n",
        //    xpand_liney, xpand_centery);
      }

      int ofs_1 = 0;
      boxRGBA(screen,
              xpand_x1,xpand_centery,
              xpand_x2,xpand_centery+xpand_liney,
              127,0,0,255);
      Draw_Item(screen,sp_1,xpand_x1,
                xpand_centery-xpand_liney*ofs_1);
      while (ofs_1 < xpand_spread && sp_1->prev != 0) {
        sp_1 = sp_1->prev;
        ofs_1++;
        Draw_Item(screen,sp_1,xpand_x1,
                  xpand_centery-xpand_liney*ofs_1);
      }
      int ofs_2 = 0;
      while (ofs_2 < xpand_spread-1 && sp_2->next != 0) {
        sp_2 = sp_2->next;
        ofs_2++;
        Draw_Item(screen,sp_2,xpand_x1,
                  xpand_centery+xpand_liney*ofs_2);
      }
    }

    if (mygettime()-xpand_lastactivity >= xpand_delay) {
      // No activity, close expanded window
      xpanded = 0;
    }
  }

  // Draw single-line view
  Draw_Item(screen,cur,xpos,ypos);

  UnlockBrowser();
}

void FloDisplayParamSet::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color barclr = { 0xFF, 0x50, 0x20, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };

  boxRGBA(screen,
          xpos,ypos,xpos+sx,ypos+sy,
          0,0,0,190);
  vlineRGBA(screen,xpos,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,xpos+sx,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font == 0 || font->font == 0) {
    printf("VIDEO: WARNING: No font specified for parameter set.\n");
    return;
  }

  int textheight = 0;
  TTF_SizeText(font->font,VERSION,0,&textheight);

  // Draw title
  if (title != 0)
    VideoIO::draw_text(screen,font->font,
                       title,xpos+sx-margin,ypos,titleclr,2,0);

  if (curbank < numbanks) {
    ParamSetBank *b = &banks[curbank];

    if (b->name != 0)
      VideoIO::draw_text(screen,font->font,banks[curbank].name,
          xpos+margin,ypos,titleclr,0,0);

    // Draw bars for all active parameters in this bank
    int spacing = (sx - margin*2) / numactiveparams,
        cury = ypos + sy - margin,
        curbary = cury - textheight - margin;

    int maxheight = sy - margin*3 - textheight*3;
    float barscale = maxheight / b->maxvalue;
    int thickness = spacing / 4;

    int curx = xpos + thickness*2 + margin;

    for (int i = b->firstparamidx;
        i < b->numparams && i < b->firstparamidx + numactiveparams; i++, curx += spacing) {
      // Param name
      if (b->params[i].name != 0)
        VideoIO::draw_text(screen,font->font,b->params[i].name,
            curx,cury,titleclr,1,2);

      float lvl = b->params[i].value * barscale;

      // Show max value
      boxRGBA(screen,
              curx-thickness/2,curbary,
              curx+thickness/2,curbary-maxheight,
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
          curx-thickness,curbary,
          curx+thickness,curbary-lvl,
          barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
          curx-thickness/2,curbary,
          curx+thickness/2,curbary-lvl,
          barclr.r,barclr.g,barclr.b,255);
    }
  }
};

// Draw text display
void FloDisplayText::Draw(SDL_Surface *screen) {
  static char tmp[255];
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color valclr = { 0xDF, 0xEF, 0x20, 0 };

  if (font != 0 && font->font != 0) {
    int xofs = 0, yofs = 0;

    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,xpos,ypos,titleclr,0,1,
                         &xofs,&yofs);

    // Draw value
    UserVariable val = exp->Evaluate(0);
    val.Print(tmp,255);
    VideoIO::draw_text(screen,font->font,
                       tmp,xpos+xofs,ypos,valclr,0,1);
  }
};

// Draw switch display
void FloDisplaySwitch::Draw(SDL_Surface *screen) {
  const static SDL_Color title1clr = { 0xDF, 0xEF, 0x20, 0 };
  const static SDL_Color title0clr = { 0x11, 0x22, 0x33, 0 };

  if (font != 0 && font->font != 0 && title != 0) {
    // Evaluate exp
    UserVariable val = exp->Evaluate(0);
    char nonz = (char) val;

    // Draw title
    VideoIO::draw_text(screen,font->font,
                       title,xpos,ypos,(nonz ? title1clr : title0clr),0,1);
  }
};

// Draw circular switch display
void FloDisplayCircleSwitch::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  SDL_Color c1clr = { 0xDF, 0x20, 0x20, 0 };
  SDL_Color c0clr = { 0x11, 0x22, 0x33, 0 };
  const static float flashspd = 4.0;

  double dt = mygettime()-nonztime;
  char flashon = 1;
  if (flash)
    flashon = !((char) (((long int) (dt*flashspd)) % 2));

  // Evaluate exp
  UserVariable val = exp->Evaluate(0);
  char nonz = (char) val;
  if (nonz && !prevnonz)
    // Value is switching on-- store time
    nonztime = mygettime();
  prevnonz = nonz;

  // Draw circle
  SDL_Color *c = (nonz && flashon ? &c1clr : &c0clr);
  filledCircleRGBA(screen,xpos,ypos,(nonz && flashon ? rad1 : rad0),
                   c->r, c->g, c->b, 255);

  if (font != 0 && font->font != 0 && title != 0) {
    // Draw title
    VideoIO::draw_text(screen,font->font,
                       title,xpos+2*rad0,ypos,titleclr,0,1);
  }
};

// Draw text switch display
void FloDisplayTextSwitch::Draw(SDL_Surface *screen) {
  // No title displayed
  SDL_Color c1clr = { 0x77, 0x88, 0x99, 0 };
  SDL_Color c0clr = { 0x99, 0x88, 0x77, 0 };

  // Evaluate exp
  UserVariable val = exp->Evaluate(0);
  char nonz = (char) val;

  // Draw appropriate text
  char *dtxt = (nonz ? text1 : text0);
  if (dtxt != 0)
    VideoIO::draw_text(screen,font->font,dtxt,
                       xpos,ypos,
                       (nonz ? c1clr : c0clr),0,1);
};

// Draw text display
void FloDisplayBar::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color barclr = { 0xFF, 0x50, 0x20, 0 };
  const static float calwidth = 1.1;

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,xpos,ypos,titleclr,
                         (orient == O_Vertical ? 1 : 2),
                         (orient == O_Horizontal ? 1 : 0));
  }

  // Get value of expression
  UserVariable val = exp->Evaluate(0);
  float fval = (float) val;

  if (dbscale) {
    // dB

    // Convert linear value to dB and then to fader level:
    int lvl = (int) (AudioLevel::dB_to_fader(LIN2DB(fval), maxdb) * barscale);

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          // printf("%f > %f def\n",i,AudioLevel::dB_to_fader(i, maxdb));

          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * barscale);
          hlineRGBA(screen,
                    xpos-(int) (calwidth*thickness),
                    xpos-thickness,
                    ypos-clvl,
                    clr,clr,clr,255);
          hlineRGBA(screen,
                    xpos+thickness,
                    xpos+(int) (calwidth*thickness),
                    ypos-clvl,
                    clr,clr,clr,255);
        }
      }

      // Bar
      boxRGBA(screen,
              xpos-thickness,ypos,
              xpos+thickness,ypos-lvl,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              xpos-thickness/2,ypos,
              xpos+thickness/2,ypos-lvl,
              barclr.r,barclr.g,barclr.b,255);
    } else {
      // Horizontal

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * barscale);
          vlineRGBA(screen,
                    xpos+clvl,
                    ypos-(int) (calwidth*thickness),
                    ypos-thickness,
                    clr,clr,clr,255);
          vlineRGBA(screen,
                    xpos+clvl,
                    ypos+thickness,
                    ypos+(int) (calwidth*thickness),
                    clr,clr,clr,255);
        }
      }

      // Bar
      boxRGBA(screen,
              xpos,ypos-thickness,
              xpos+lvl,ypos+thickness,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              xpos,ypos-thickness/2,
              xpos+lvl,ypos+thickness/2,
              barclr.r,barclr.g,barclr.b,255);
    }
  } else {
    // Linear

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      // Show calibration
      boxRGBA(screen,
              xpos-thickness/2,ypos,
              xpos+thickness/2,(int) (ypos-barscale),
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
              xpos-thickness,ypos,
              xpos+thickness,(int) (ypos-fval*barscale),
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              xpos-thickness/2,ypos,
              xpos+thickness/2,(int) (ypos-fval*barscale),
              barclr.r,barclr.g,barclr.b,255);
    } else {
      // Horizontal

      // Show calibration
      boxRGBA(screen,
              xpos,ypos-thickness/2,
              (int) (xpos+barscale),ypos+thickness/2,
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
              xpos,ypos-thickness,
              (int) (xpos+fval*barscale),ypos+thickness,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              xpos,ypos-thickness/2,
              (int) (xpos+fval*barscale),ypos+thickness/2,
              barclr.r,barclr.g,barclr.b,255);
    }
  }
};

// Draw text display
void FloDisplayBarSwitch::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 },
    warnclr = { 0xFF, 0, 0, 0 };
  const static SDL_Color barclr[2] = { { 0xEF, 0xAF, 0xFF, 0 },
                                       { 0xCF, 0x4F, 0xFC, 0 } };
  const static float calwidth = 1.1;

  const SDL_Color *bc = (color == 2 ? &barclr[1] : &barclr[0]);

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,xpos,ypos,titleclr,
                         (orient == O_Vertical ? 1 : 2),
                         (orient == O_Horizontal ? 1 : 0));
  }

  // Get value of expression
  UserVariable val = exp->Evaluate(0);
  float fval = (float) val;
  UserVariable sval = switchexp->Evaluate(0);
  char sw = (char) sval;

  if (calibrate && fval >= cval)
    bc = &warnclr;

  if (dbscale) {
    // dB

    // Convert linear value to dB and then to fader level:
    int lvl = (int) (AudioLevel::dB_to_fader(LIN2DB(fval), maxdb) * barscale);

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * barscale);
          hlineRGBA(screen,
                    xpos-(int) (calwidth*thickness/2),
                    xpos-thickness/2,
                    ypos-clvl,
                    clr,clr,clr,(sw ? 255 : 127));
          hlineRGBA(screen,
                    xpos+thickness/2,
                    xpos+(int) (calwidth*thickness/2),
                    ypos-clvl,
                    clr,clr,clr,(sw ? 255 : 127));
        }
      }

      // Bar
      boxRGBA(screen,
              xpos-thickness/2,ypos,
              xpos+thickness/2,ypos-lvl,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
    } else {
      // Horizontal

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * barscale);
          vlineRGBA(screen,
                    xpos+clvl,
                    ypos-(int) (calwidth*thickness/2),
                    ypos-thickness/2,
                    clr,clr,clr,(sw ? 255 : 127));
          vlineRGBA(screen,
                    xpos+clvl,
                    ypos+thickness/2,
                    ypos+(int) (calwidth*thickness/2),
                    clr,clr,clr,(sw ? 255 : 127));
        }
      }

      // Bar
      boxRGBA(screen,
              xpos,ypos-thickness/2,
              xpos+lvl,ypos+thickness/2,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
    }
  } else {
    // Linear

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      // Bar
      boxRGBA(screen,
              xpos-thickness/2,ypos,
              xpos+thickness/2,(int) (ypos-fval*barscale),
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
      // Calibrate
      if (calibrate)
        hlineRGBA(screen,
                  xpos-thickness/2,
                  xpos+thickness/2,
                  (int) (ypos-cval*barscale),
                  255,255,255,(sw ? 255 : 127));
    } else {
      // Horizontal

      // Bar
      boxRGBA(screen,
              xpos,ypos-thickness/2,
              (int) (xpos+fval*barscale),ypos+thickness/2,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
      // Calibrate
      if (calibrate)
        vlineRGBA(screen,
                  (int) (xpos+cval*barscale),
                  ypos-thickness/2,
                  ypos+thickness/2,
                  255,255,255,(sw ? 255 : 127));
    }
  }
};

// Draw this element to the given screen-
// implementation given in videoio.cc
void FloLayoutBox::Draw(SDL_Surface *screen, SDL_Color clr) {
  // Solid box
  boxRGBA(screen,
          left,top,right,bottom,
          clr.r,clr.g,clr.b,255);
  // Outline
  if (lineleft)
    vlineRGBA(screen,left,top,bottom,0,0,0,255);
  if (lineright)
    vlineRGBA(screen,right,top,bottom,0,0,0,255);
  if (linetop)
    hlineRGBA(screen,left,right,top,0,0,0,255);
  if (linebottom)
    hlineRGBA(screen,left,right,bottom,0,0,0,255);
};

// Draw snapshots display
void FloDisplaySnapshots::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };
  const static SDL_Color cursorclr = { 0xEF, 0x11, 0x11, 0 };

  LockSnaps();

  if (numdisp == -1) {
    int height = TTF_FontHeight(font->font);
    numdisp = sy/height;
  }

  boxRGBA(screen,
          xpos,ypos,xpos+sx,ypos+sy,
          0,0,0,190);
  vlineRGBA(screen,xpos,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,xpos+sx,ypos,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,xpos,xpos+sx,ypos+sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,xpos+sx/2,ypos,titleclr,1,2);
  }

  // Draw items
  int cury = ypos+margin;
  int height = TTF_FontHeight(font->font);
  for (int i = firstidx; i < firstidx + numdisp; i++, cury += height) {
    const static int SNAP_NAME_LEN = 512;

    char buf[SNAP_NAME_LEN];
    Snapshot *sn = app->getSNAP(i);
    if (sn != 0) {
      RenameUIVars *rui = 0;
      char *nm = sn->name;

      if (renamer != 0 && i == rename_idx) {
        // Use current name from renamer
        rui = renamer->UpdateUIVars();
        nm = renamer->GetCurName();
      }

      if (nm != 0)
        snprintf(buf,SNAP_NAME_LEN,"%2d %s",i+1,nm);
      else if (sn->exists != 0)
        snprintf(buf,SNAP_NAME_LEN,"%2d **",i+1);
      else
        snprintf(buf,SNAP_NAME_LEN,"%2d",i+1);

      int sx, sy;
      VideoIO::draw_text(screen,font->font,
                         buf,xpos+margin,cury,titleclr,0,0,&sx,&sy);

      if (rui != 0 && rui->rename_cursor_toggle)
          boxRGBA(screen,
                  xpos+margin+sx,cury,
                  xpos+margin+sx+sy/2,cury+sy,
                  cursorclr.r,cursorclr.g,cursorclr.b,255);
    }
  }

  UnlockSnaps();
};
