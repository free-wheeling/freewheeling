/* 
   Art is the beginning.
*/

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
#include "fweelin_logo.h"

#ifdef __MACOSX__
#define CAPITAL_FILLED_PIE
#endif

// Different versions of sdl-gfx have different naming
#ifdef CAPITAL_FILLED_PIE
#define FILLED_PIE filledPieRGBA
#else
#define FILLED_PIE filledpieRGBA
#endif

double mygettime(void) {
  static struct timeval mytv;
  gettimeofday(&mytv,NULL);
  return(mytv.tv_sec+mytv.tv_usec/1000000.0);
}

int round(float num) {
  if (num-(long)num < 0.5)
    return (int) floor(num);
  else
    return (int) ceil(num);
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
		 app->getRP()->GetLimiterVolume(),0)) {
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
  static char tmp[255];

  if (font != 0 && font->font != 0 && i != 0) {
    switch (i->GetType()) {
    case B_Patch :
      {
	PatchItem *p = (PatchItem *) i;
	
	// Current patch
	snprintf(tmp,255,"%02d: %s",
		 p->id,
		 p->name);
	tmp[254] = '\0';
	VideoIO::draw_text(screen,font->font,tmp,x,y,white);
      }
      break;

    case B_Division :
      break;

    default : 
      {
	if (i->GetType() == B_Loop)
	  snprintf(tmp,255,"%s-",FWEELIN_OUTPUT_LOOP_NAME);
	else if (i->GetType() == B_Scene)
	  snprintf(tmp,255,"%s-",FWEELIN_OUTPUT_SCENE_NAME);
	else
	  tmp[0] = '\0';

	if (i == cur && renamer != 0) {
	  RenameUIVars *rui = renamer->UpdateUIVars();

	  strncat(tmp,renamer->GetCurName(),255);
	  tmp[254] = '\0';
	  
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
	  strncat(tmp,i->name,255);
	  tmp[254] = '\0';
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
};

// Draw text display
void FloDisplayBarSwitch::Draw(SDL_Surface *screen) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color barclr[2] = { { 0xEF, 0xAF, 0xFF, 0 },
				       { 0xCF, 0x4F, 0xFC, 0 } };
  
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

CircularMap::CircularMap(SDL_Surface *in,
			 int map_xs, int map_ys,
			 int in_xs, int in_ys,
			 int rinner, int rsize) :
  in(in), map_xs(map_xs), map_ys(map_ys), in_xs(in_xs), in_ys(in_ys),
  rinner(rinner), rsize(rsize), next(0) {
  // Allocate memory for maps
  map = new Uint8 *[map_xs * map_ys];
  // 4 scan run lengths per scanline
  scanmap = new int[4 * map_ys];

  // Compute constants
  int map_xc = map_xs/2;
  int map_yc = map_ys/2;
  int pitch = in->pitch;
  int bpp = in->format->BytesPerPixel;
  Uint8 *in_base = (Uint8 *) in->pixels;

  // Now generate, interating across output range
  for (int y = 0; y < map_ys; y++) {
    int runnum = 0,
      // Count of consecutive written/notwritten pixels
      // Positive is written, negative is notwritten
      pixelscount = 0;
    for (int x = 0; x < map_xs; x++) {
      //printf("%d %d\n", x, y);
      int yofs = y-map_yc,
	xofs = map_xc-x;
      float theta = atan2(yofs,xofs),
	in_x = in_xs*(theta+M_PI)/(2*M_PI);

      // Now that we know x mapping, based on calculated theta (see theta)
      // Let's get y mapping
      float in_y;
      if (sin(theta) == 0) {
	// This fixes an annoying horizontal crack in the map
	in_y = (xofs-rinner)*in_ys/rsize;
	//printf("xofs: %d yofs: %d inx: %f iny: %f\n",xofs,yofs,in_x,in_y);
      }
      else
	in_y = (yofs/sin(theta)-rinner)*in_ys/rsize;

      // Are we in range, honey?
      int idx = y*map_xs + x;
      if (in_x >= 0 & in_y >= 0 &&
	  in_x < in_xs && in_y < in_ys) {
        /*printf("%d %d\n", x, y);
	  printf(" in[%d,%d]\n",(int)in_x,(int)in_y);*/

	// Yup-- write the map
	map[idx] = in_base + round(in_y)*pitch + round(in_x)*bpp;
	
	// Generate scan map
	if (pixelscount <= 0) {
	  // OK start of run

	  // Write number of empty pixels from last run to here
	  scanmap[y*4 + runnum] = -pixelscount;
	  //printf("Y:%d r:%d cnt: %d\n",y,runnum,-pixelscount);

	  runnum++;
	  pixelscount = 0;
	}
	pixelscount++;
      } else {
	//printf(" none\n");

	// Mapping is empty for this location
	map[idx] = 0;

	// Generate scan map
	if (pixelscount > 0) {
	  // OK end of run

	  // Write number of pixels written from beginning of run to here
	  scanmap[y*4 + runnum] = pixelscount;
	  //printf("Y:%d r:%d cnt: %d\n",y,runnum,pixelscount);
	  
	  runnum++;
	  pixelscount = 0;
	}
	pixelscount--;
      }
    }

    // Now write the rest of scan map
    if (pixelscount > 0) {
      // OK end of run

      // Write number of pixels written from beginning of run to here
      scanmap[y*4 + runnum] = pixelscount;
      //printf("Y:%d r:%d cnt: %d\n",y,runnum,pixelscount);

      runnum++;
    }

    //printf("EOL- r:%d\n",runnum);
    //usleep(100000);

    for (; runnum < 4; runnum++) 
      scanmap[y*4 + runnum] = -1;
  }
}

CircularMap::~CircularMap() {
  delete[] scanmap;
  delete[] map;
};

// Map flat scope onto circle
// Return nonzero on error
char CircularMap::Map(SDL_Surface *out, int dstx, int dsty) {
  int bpp;
  int *tmpscan = scanmap;
  
  // Get surface format 
  bpp = out->format->BytesPerPixel;
  if (bpp != in->format->BytesPerPixel) {
    printf("VIDEO: ERROR: Temporary buffer & video screen not "
	   "matching depth (in: %d out: %d).\n", 
	   in->format->BytesPerPixel,bpp);
    return 1;
  }

  // Check for clipping
  if (dstx < 0 || dsty < 0 || dstx+map_xs >= out->w ||
      dsty+map_ys >= out->h)
    return 1; // Don't handle clip case-- just don't draw!

  /*
   * Lock the surface 
   */
  if (SDL_MUSTLOCK(out)) {
    if (SDL_LockSurface(out) < 0) {
      return 1;
    }
  }

  int out_pitch = out->pitch;

  Uint8 **ptr = map,
    **ptr2,
    *optr = (Uint8 *) out->pixels + dsty*out_pitch + dstx*bpp,
    *optr2;

  Uint8 *op, *ip;
  int ofs,
    scanleft; 
  for (int y = 0; y < map_ys; y++, ptr += map_xs, optr += out_pitch,
	 tmpscan += scanleft) {
    scanleft = 4;
    ptr2 = ptr;
    optr2 = optr;

    // What's the first x location on this scanline we should look at?
    while ((ofs = *tmpscan) != -1 && scanleft) {
      tmpscan++;

      // Starting position on this scanline
      ptr2 += ofs;
      optr2 += ofs*bpp;
      
      //printf("wy:%d sofs:%d ",y,ofs);

      // And the number of bytes in the run on this scanline
      ofs = *(tmpscan++);

      //printf("len:%d\n",ofs);

      // Depending on the number of bytes per pixel, handle this scanline
      // differently-- optimized subroutines follow..
      switch (bpp) {
      case 1:
	{
	  for (int i = 0; i < ofs; i++, ptr2++, optr2 += bpp)
	    // Copy 1 byte pixels
	    *optr2 = **ptr2;
	}
	break;
      case 2:
	{
	  for (int i = 0; i < ofs; i++, ptr2++, optr2 += bpp)
	    // Copy 2 byte pixels
	    *((Uint16 *) optr2) = *((Uint16 *) (*ptr2));
	}
	break;
      case 3:
	{
	  // 3 byte pixels
	  for (int i = 0; i < ofs; i++, ptr2++, optr2 += bpp) {
	    // Access the map to find offset into input
	    ip = *ptr2;
	    // And output pointer is right there
	    op = optr2;

	    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	      ip += 2;
	      // Copy, reversing bit order for this pixel
	      for (int n = 0; n < bpp; n++)
		*(op++) = *(ip--);
	    }
	    else {
	      // Copy, regular bit order for this pixel
	      for (int n = 0; n < bpp; n++)
		*(op++) = *(ip++);
	    }
	  }
	}
	break;
      case 4:
	{
	  for (int i = 0; i < ofs; i++, ptr2++, optr2 += bpp)
	    // Copy 4 byte pixels
	    *((Uint32 *) optr2) = *((Uint32 *) (*ptr2));
	}
	break;
      }
      
      // Ok, 1 scan run complete, so 2 endpoints less on scanline
      scanleft -= 2;
    }
  }

  /*
   * Unlock surface 
   */
  if (SDL_MUSTLOCK(out)) {
    SDL_UnlockSurface(out);
  }

  return 0;
};

// ******** VIDEO HANDLER

int VideoIO::activate() {
#ifdef __MACOSX__
  // On Mac OS X, prime video in main thread
  SetVideoMode(0);
#endif
  
#if 1
  printf("VIDEO: Starting handler..\n");

  pthread_mutex_init (&video_thread_lock,0);

  const static size_t STACKSIZE = 1024*128;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("VIDEO: Stacksize: %d.\n",STACKSIZE);

  int ret = pthread_create(&video_thread,
			   &attr,
			   run_video_thread,
			   this);
  if (ret != 0) {
    printf("VIDEO: (start) pthread_create failed, exiting");
    return 1;
  }

  // Setup low priority thread
#if 0
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  schp.sched_priority = sched_get_priority_min(SCHED_OTHER);
  printf("VIDEO: Low priority thread %d\n",schp.sched_priority);
  if (pthread_setschedparam(video_thread, SCHED_OTHER, &schp) != 0)
    printf("VIDEO: Can't set thread priority, will use regular!\n");
#endif
  
  // Listen for events
  if (app->getEMG() == 0) {
    printf("VIDEO: Error: Event Manager not yet active!\n");
    exit(1);
  }

  app->getEMG()->ListenEvent(this,0,T_EV_VideoShowLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_VideoShowDisplay);
  app->getEMG()->ListenEvent(this,0,T_EV_VideoShowLayout);
  app->getEMG()->ListenEvent(this,0,T_EV_VideoShowHelp);
  app->getEMG()->ListenEvent(this,0,T_EV_VideoFullScreen);

  app->getEMG()->ListenEvent(this,0,T_EV_Input_MouseButton);
  app->getEMG()->ListenEvent(this,0,T_EV_Input_MouseMotion);
#endif
  
  return 0;
}

void VideoIO::close() {
  app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowDisplay);
  app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowLayout);
  app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowHelp);
  app->getEMG()->UnlistenEvent(this,0,T_EV_VideoFullScreen);

  app->getEMG()->UnlistenEvent(this,0,T_EV_Input_MouseButton);
  app->getEMG()->UnlistenEvent(this,0,T_EV_Input_MouseMotion);

  videothreadgo = 0;
  pthread_join(video_thread,0);
  pthread_mutex_destroy (&video_thread_lock);
  printf("VIDEO: end\n");
}

void VideoIO::ReceiveEvent(Event *ev, EventProducer *from) {
  switch (ev->GetType()) {
  case T_EV_Input_MouseMotion :
    {
      MouseMotionInputEvent *mev = (MouseMotionInputEvent *) ev;
      
      // Notify all visible browsers of mouse move
      FloDisplay *curdisplay = app->getCFG()->GetDisplays();
      char skip = 0;
      while (curdisplay != 0 && !skip) {
	if (curdisplay->show &&
	    curdisplay->GetFloDisplayType() == FD_Browser)
	  skip = ((Browser *) curdisplay)->MouseMotion(mev);

	curdisplay = curdisplay->next;
      }
    }
    break;

  case T_EV_Input_MouseButton :
    {
      MouseButtonInputEvent *mev = (MouseButtonInputEvent *) ev;
      
      // Notify all visible browsers of button press
      FloDisplay *curdisplay = app->getCFG()->GetDisplays();
      char skip = 0;
      while (curdisplay != 0 && !skip) {
	if (curdisplay->show &&
	    curdisplay->GetFloDisplayType() == FD_Browser)
	  skip = ((Browser *) curdisplay)->MouseButton(mev);

	curdisplay = curdisplay->next;
      }

      if (!skip) { // Only continue if a browser hasn't eaten the event
	// Has the mouse button been pressed inside any of the 
	// on-screen layout elements?
	FloLayout *cur = app->getCFG()->GetLayouts();
	while (cur != 0) {
	  if (cur->show) {
	    int firstid = cur->loopids.lo, 
	      curid = firstid,
	      maxid = cur->loopids.hi;
	    
	    // Check each element in this layout
	    FloLayoutElement *curel = cur->elems;
	    while (curel != 0 && curid <= maxid) {
	      if (curel->Inside(mev->x,mev->y)) {
		// Mouse button inside element-
		
		// Issue 'loop clicked' event
		LoopClickedEvent *lcevt = (LoopClickedEvent *) 
		  Event::GetEventByType(T_EV_LoopClicked);
		
		lcevt->button = mev->button;
		lcevt->down = mev->down;
		lcevt->loopid = firstid + curel->id;
		lcevt->in = 1; // In=1 means clicked in on-screen layout
		app->getEMG()->BroadcastEventNow(lcevt, this);
		
		if (CRITTERS) 
		  printf("MOUSE: Button #%d %s in element: %s\n",
			 mev->button,
			 (mev->down ? "pressed" : "released"),
			 curel->name);
	      }
	      
	      curid++;
	      curel = curel->next;
	    }	    
	  }

	  cur = cur->next;
	}
      }
    }
    break;

  case T_EV_VideoShowLoop :
    {
      VideoShowLoopEvent *vev = (VideoShowLoopEvent *) ev;
      if (CRITTERS)
	printf("VIDEO: Show loop (layout %d): %d>%d\n",
	       vev->layoutid,vev->loopid.lo,vev->loopid.hi);

      // Error check range
      if (vev->loopid.lo < 0 || vev->loopid.hi < 0 ||
	  vev->loopid.lo >= app->getCFG()->GetNumTriggers() ||
	  vev->loopid.hi >= app->getCFG()->GetNumTriggers() ||
	  vev->loopid.hi < vev->loopid.lo) {
	printf("VIDEO: Invalid loopid range for layout %d: %d>%d\n",
	       vev->layoutid,vev->loopid.lo,vev->loopid.hi);
      } else {
	// Find the right layout
	int id = vev->layoutid;
	FloLayout *cur = app->getCFG()->GetLayouts();
	char found = 0;
	while (cur != 0) {
	  if (id == cur->id) {
	    // Match-- change the range of displayed loops for
	    // this layout
	    cur->loopids = vev->loopid;
	    found = 1;
	  }
	  
	  cur = cur->next;
	}

	if (!found)
	  printf("VIDEO: Invalid layoutid %d.\n",vev->layoutid);
      }
    }
    break;

  case T_EV_VideoShowDisplay :
    {
      VideoShowDisplayEvent *vev = (VideoShowDisplayEvent *) ev;
      if (CRITTERS)
	printf("VIDEO: %s display (id %d)\n",
	       (vev->show ? "show" : "hide"),
	       vev->displayid);
      
      // Find the right display
      FloDisplay *cur = app->getCFG()->GetDisplayById(vev->displayid);
      if (cur != 0)
	cur->show = vev->show;
      else
	printf("VIDEO: Invalid display (id %d).\n",vev->displayid);
    }
    break;

  case T_EV_VideoShowLayout :
    {
      VideoShowLayoutEvent *vev = (VideoShowLayoutEvent *) ev;
      if (CRITTERS)
	printf("VIDEO: %s layout (i%d) %s\n",
	       (vev->show ? "show" : "hide"),
	       vev->layoutid,
	       (vev->hideothers ? "(hide others)" : ""));
      
      // Find the right layout
      int id = vev->layoutid;
      FloLayout *cur = app->getCFG()->GetLayouts();
      char found = 0;
      while (cur != 0) {
	if (id == cur->id) {
	  // Match!
	  // Set show or hide
	  cur->show = vev->show;
	  found = 1;
	} else if (vev->hideothers) {
	  // Hide other layouts
	  cur->show = 0;
	}
	
	cur = cur->next;
      }

      if (!found)
	printf("VIDEO: Invalid layoutid %d.\n",vev->layoutid);
    }
    break;

  case T_EV_VideoShowHelp :
    {
      VideoShowHelpEvent *vev = (VideoShowHelpEvent *) ev;
      if (CRITTERS)
	printf("VIDEO: show help page - %d%s\n",
	       vev->page,(vev->page ? "" : " - off"));

      if (vev->page >= 0 && vev->page <= numhelppages)
	showhelppage = vev->page;
      else
	printf("VIDEO: invalid help page - %d (valid range: 0-%d)\n",
	       vev->page,numhelppages);
    }
    break;

  case T_EV_VideoFullScreen :
    {
      VideoFullScreenEvent *vev = (VideoFullScreenEvent *) ev;
      if (CRITTERS)
	printf("VIDEO: set full screen = %s\n",
	       (vev->fullscreen ? "on" : "off"));

      SetVideoMode(vev->fullscreen);
    }
    break;

  default:
    break;
  }
};

// This is a custom surface blitter that doesn't use large block
// memory writes and thus avoids the strange video glitch of introducing
// audio pops on some machines-- some loss in performance!
void VideoIO::Custom_BlitSurface(SDL_Surface *in, SDL_Surface *out,
				 SDL_Rect *dstrect) {
  int opitch = out->pitch,
    ipitch = in->pitch,
    bpp = out->format->BytesPerPixel;
  if (bpp != in->format->BytesPerPixel) {
    printf("VIDEO: ERROR: Buffer and screen different format!\n");
    return;
  }

  /*
   * Lock the surface 
   */
  if (SDL_MUSTLOCK(out)) {
    if (SDL_LockSurface(out) < 0) {
      return;
    }
  }

  Uint8 *opixels = (Uint8 *)out->pixels + dstrect->y*opitch + dstrect->x*bpp,
    *ipixels = (Uint8 *)in->pixels;
  int xrun = (dstrect->x+dstrect->w <= out->w ? dstrect->w : 
	      out->w-dstrect->x),
    yrun = (dstrect->y+dstrect->h <= out->h ? dstrect->h : out->h-dstrect->y),
    ojump = opitch-xrun*bpp,
    ijump = ipitch-xrun*bpp;
  switch (bpp) {
  case 1 :
    {
      for (int i = 0; i < yrun; i++, opixels += ojump, ipixels += ijump)
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *opixels = *ipixels;
    }
    break;
  case 2 :
    {
      for (int i = 0; i < yrun; i++, opixels += ojump, ipixels += ijump)
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *((Uint16 *) opixels) = *((Uint16 *) ipixels);
    }
    break;
  case 3 :
    {
      for (int i = 0; i < yrun; i++, opixels += ojump, ipixels += ijump)
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp) {
	  *(opixels++) = *(ipixels++);
	  *(opixels++) = *(ipixels++);
	  *(opixels++) = *(ipixels++);
	}
    }
    break;
  case 4 :
    {
      for (int i = 0; i < yrun; i++, opixels += ojump, ipixels += ijump)
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *((Uint32 *) opixels) = *((Uint32 *) ipixels);
    }
    break;
  }

  /*
   * Unlock surface 
   */
  if (SDL_MUSTLOCK(out)) {
    SDL_UnlockSurface(out);
  }
};

void VideoIO::Squeeze_BlitSurface(SDL_Surface *in, SDL_Surface *out,
				  SDL_Rect *dstrect) {
  int opitch = out->pitch,
    ipitch = in->pitch,
    bpp = out->format->BytesPerPixel;
  if (bpp != in->format->BytesPerPixel) {
    printf("VIDEO: ERROR: Buffer and screen different format!\n");
    return;
  }

  if (dstrect->w == 0 || dstrect->h == 0)
    return;
  if (dstrect->h > in->h)
    dstrect->h = in->h;

  /*
   * Lock the surface 
   */
  if (SDL_MUSTLOCK(out)) {
    if (SDL_LockSurface(out) < 0) {
      return;
    }
  }

  Uint8 *opixels = (Uint8 *)out->pixels + dstrect->y*opitch + dstrect->x*bpp,
    *ipixels = (Uint8 *)in->pixels;
  float yscale = (float) in->h/dstrect->h;
  int xrun = (dstrect->x+dstrect->w <= out->w ? dstrect->w : 
	      out->w-dstrect->x),
    yh = (dstrect->y+dstrect->h <= out->h ? 
	  dstrect->h : out->h-dstrect->y),
    yrun = (int) (yh*yscale),
    ojump = opitch-xrun*bpp,
    ijump = ipitch-xrun*bpp,
    ilinejump = ipitch;

  /* printf("xrun: %d yrun: %d w: %d h: %d yscale: %.2f ijump: %d\n",
	 xrun,yrun,
	 in->w,in->h,
	 yscale,ijump); */
  switch (bpp) {
  case 1 :
    {
      for (float i = 0; i < yrun; opixels += ojump) {
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *opixels = *ipixels;
	int oldi = (int)i;
	i += yscale;
	ipixels += ijump+ilinejump*((int)i-(int)oldi-1);
      }
    }
    break;
  case 2 :
    {
      for (float i = 0; i < yrun; opixels += ojump) {
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *((Uint16 *) opixels) = *((Uint16 *) ipixels);
	int oldi = (int)i;
	i += yscale;
	ipixels += ijump+ilinejump*((int)i-(int)oldi-1);
      }
    }
    break;
  case 3 :
    {
      for (float i = 0; i < yrun; opixels += ojump) {
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp) {
	  *(opixels++) = *(ipixels++);
	  *(opixels++) = *(ipixels++);
	  *(opixels++) = *(ipixels++);
	}
	int oldi = (int)i;
	i += yscale;
	ipixels += ijump+ilinejump*((int)i-(int)oldi-1);
      }
    }
    break;
  case 4 :
    {
      for (float i = 0; i < yrun; opixels += ojump) {
	for (int j = 0; j < xrun; j++, opixels += bpp, ipixels += bpp)
	  *((Uint32 *) opixels) = *((Uint32 *) ipixels);
	int oldi = (int)i;
	i += yscale;
	ipixels += ijump+ilinejump*((int)i-(int)oldi-1);
      }
    }
    break;
  }

  /*
   * Unlock surface 
   */
  if (SDL_MUSTLOCK(out)) {
    SDL_UnlockSurface(out);
  }
};

// Draws text on the given surface
// Justify is 0 for default justify, 1 for center, and 2 for opposite side
// Returns size of text drawn in sx and sy (optionally)
int VideoIO::draw_text(SDL_Surface *out, TTF_Font *font,
		       char *str, int x, int y, SDL_Color clr, 
		       char justifyx, char justifyy, int *sx, int *sy) {
  SDL_Surface *text;
  SDL_Rect dstrect;
  SDL_Color black = { 0, 0, 0, 0 };

  text = TTF_RenderText_Shaded(font, str, clr, black);
  if ( text != NULL ) {
    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = text->w;
    dstrect.h = text->h;

    if (sx != 0)
      *sx = dstrect.w;
    if (sy != 0)
      *sy = dstrect.h;
    
    if (justifyx)
      dstrect.x -= (justifyx == 1 ? dstrect.w/2 : dstrect.w);
    if (justifyy)
      dstrect.y -= (justifyy == 1 ? dstrect.h/2 : dstrect.h);

    SDL_SetColorKey(text, SDL_SRCCOLORKEY|SDL_RLEACCEL, 0);
    SDL_BlitSurface(text, NULL, out, &dstrect);
    SDL_FreeSurface(text);
  }
  else {
    if (sx != 0)
      *sx = 0;
    if (sy != 0)
      *sy = 0;
    return 1;
  }

  return 0;
}

char VideoIO::DrawLoop(LoopManager *loopmgr, int i, 
		       SDL_Surface *screen, SDL_Surface *lscopepic,
		       SDL_Color *loopcolors, float colormag,
		       FloConfig *fs, FloLayoutElement *curel,

		       CircularMap *direct_map, int direct_xpos,
		       int direct_ypos,

		       float lvol, 
		       char drawtext) {
  const static SDL_Color txtclr = { 0xFF, 0x50, 0x20, 0 },
    cursorclr = { 0xEF, 0x11, 0x11, 0 },
    txtclr2 = { 0xEF, 0xAF, 0xFF, 0 };

  // Color for selected loops
  static SDL_Color selcolor[4] = { { 0xF9, 0xE6, 0x13, 0 }, 
				   { 0x62, 0x62, 0x62, 0 },
				   { 0xFF, 0xFF, 0xFF, 0 },
				   { 0xE0, 0xDA, 0xD5, 0 } };
  
  const float cpeak_mul = 2.0, // For pulsing loops - magnitude of pulse
    cpeak_base = 0.5; // For pulsing loops - base size of loop 
  const int lscope_maxmag = OCY(15),
    lscopemag = OCY(20),
    looppiemag = OCX(20);

  BED_PeaksAvgs *pa;
  nframes_t plen = 0;
  float curpeak = 1.0, ispd;
  sample_t *peakbuf, *avgbuf;

  static int liney = -1; // How high is a line of text?

  // Lockup loops so that loop manager doesn't go and delete one while we
  // are drawing it
  loopmgr->LockLoops();

  Loop *l = 0;
  if (loopmgr->GetStatus(i) == T_LS_Recording) {
    // Use current record length for visual size
    pa = (BED_PeaksAvgs *) ((RecordProcessor *) loopmgr->
			    GetProcessor(i))->
      GetFirstRecordedBlock()->GetExtendedData(T_BED_PeaksAvgs);
    if (pa != 0) {
      PeaksAvgsManager *pa_mgr = ((RecordProcessor *) loopmgr->
				  GetProcessor(i))->GetPAMgr();
      if (pa_mgr != 0)
	plen = pa_mgr->GetPeaksI()->GetTotalLength2Cur();
    }
  } else {
    l = loopmgr->GetSlot(i);
    if (l != 0) { // Double check the loop is still there
      // Use loop length for visual size
      pa = (BED_PeaksAvgs *) l->blocks->GetExtendedData(T_BED_PeaksAvgs);
    } else
      pa = 0;

    if (pa != 0)
      plen = pa->peaks->GetTotalLen();
  }
  
  char selected = 0;
  if (l != 0 && l->selcnt > 0) {
    selected = 1;
    loopcolors = selcolor; // Color loop selected
  }

  // Draw peaks & avgs
  float loopvol = 1.0, // Loop volume
    loopdvol = 1.0; // Loop volume delta
  if (pa != 0) {
    // Background
    SDL_FillRect(lscopepic,NULL,
		 SDL_MapRGB(lscopepic->format,
			    (int) (loopcolors[0].r*colormag),
			    (int) (loopcolors[0].g*colormag),
			    (int) (loopcolors[0].b*colormag)));
	      
    if (plen > 0) {
      // Access buffers
      peakbuf = pa->peaks->buf;
      avgbuf = pa->avgs->buf;
      loopvol = loopmgr->GetLoopVolume(i);
      loopdvol = loopmgr->GetLoopdVolume(i);
      nframes_t idx = loopmgr->GetCurCnt(i)/
	fs->loop_peaksavgs_chunksize;
		
      // Compute current peak
      curpeakidx[i] = idx;
      if (curpeakidx[i] == lastpeakidx[i])
	curpeak = oldpeak[i]; // No new peak data since last update!
      else {
	nframes_t j = lastpeakidx[i];
	if (curpeakidx[i] < lastpeakidx[i])
	  j = 0;
	curpeak = 0;
	for (; j < curpeakidx[i]; j++)
	  curpeak = MAX(curpeak,peakbuf[j]*loopvol);
	curpeak *= cpeak_mul;
	curpeak += cpeak_base;
	oldpeak[i] = curpeak;
	lastpeakidx[i] = curpeakidx[i];
      }
		
      float cmag = lvol*loopvol*lscopemag*curpeak;
      
      float rv1 = loopcolors[1].r*colormag,
	gv1 = loopcolors[1].g*colormag,
	bv1 = loopcolors[1].b*colormag,
	rv2 = loopcolors[2].r*colormag,
	gv2 = loopcolors[2].g*colormag,
	bv2 = loopcolors[2].b*colormag;
      
      int midpt = lscopepic->h/2;
      // Ratio of visual size to audio scope buffer length
      ispd = (float) lscopepic->w / plen;
      
      // Write into the buffer on sliding position to give animated
      // scope effect
      float pos = -(float)idx*ispd;
      if (pos < 0.)
	pos += (float)lscopepic->w;
      
      for (nframes_t j = 0; j < plen; j++, pos += ispd) {
	float pbj = peakbuf[j];
	int peakd = (int) (cmag*pbj);
	if (peakd > lscope_maxmag)
	  peakd = lscope_maxmag;
	if (pos >= (float)lscopepic->w)
	  pos = 0.;
		  
	float peaky = avgbuf[j]/(pbj*pbj + 0.00000001)*2;
	if (peaky > 1.0)
	  peaky = 1.0;
	float rv = rv1 * peaky + rv2 * (1.-peaky),
	  gv = gv1 * peaky + gv2 * (1.-peaky),
	  bv = bv1 * peaky + bv2 * (1.-peaky);
	//if (rv > 255) rv = 255;
	//if (gv > 255) gv = 255;
	//if (bv > 255) bv = 255;
	
	if (ispd >= 1.) {
	  int pos1 = (int) pos,
	    pos2 = (int) (pos+ispd);
	  
	  boxRGBA(lscopepic,
		  (int) pos1,
		  midpt-peakd,
		  (int) pos2,
		  midpt+peakd,
		  (int) rv, (int) gv, (int) bv, 255);
	}
	else
	  vlineRGBA(lscopepic,
		    (int) pos,
		    midpt-peakd,
		    midpt+peakd,
		    (int) rv, (int) gv, (int) bv, 255);
      }
    }
  }

  // loopmgr->UnlockLoops();

  // Map flat scope onto circle
  CircularMap *loopmap;
  int dispx, dispy;
  if (curel != 0) {
    // Show in layout
    loopmap = curel->loopmap;
    dispx = curel->loopx;
    dispy = curel->loopy;
  } else {
    // Show direct on screen
    loopmap = direct_map;
    dispx = direct_xpos;
    dispy = direct_ypos;
  }
  int fullx = loopmap->map_xs,
    fully = loopmap->map_ys,
    halfx = fullx/2,
    halfy = fully/2;
  if (curel != 0) {
    // Layout specifies center of loop position.. compensate
    dispx -= halfx;
    dispy -= halfy;
  }

  // Show scope
  if (!loopmap->Map(screen,dispx,dispy)) {
    circleRGBA(screen,dispx+halfx,dispy+halfy,halfx, 
	       0,0,0, 255); // Outline
    // Show portion played in semitranslucent
    int pieradius = MIN((int) (lvol*looppiemag*curpeak),70);
    FILLED_PIE(screen,dispx+halfx,dispy+halfy,
	       pieradius,0,
	       (int) (360*loopmgr->GetPos(i)),
	       (int) (loopcolors[3].r*colormag),
	       (int) (loopcolors[3].g*colormag),
	       (int) (loopcolors[3].b*colormag),127);
    
    // Show volume
    float loop_volmag = fullx*0.9/2, // For loop volume bar
      loop_dvolmag = fullx*250; // For loop volume delta bar
    int magbar = (int) (loop_volmag*loopmgr->GetTriggerVol(i));
    boxRGBA(screen,dispx+halfx-magbar,
	    dispy+halfy-halfy/5,
	    dispx+halfx+magbar,
	    dispy+halfy+halfy/5,
	    (int) (loopcolors[3].r*colormag),0,0, 127);
    // Show volume delta
    magbar = (int) ((loopdvol-1.0)*loop_dvolmag);
    boxRGBA(screen,dispx+halfx,
	    dispy+halfy+halfy/4,
	    dispx+halfx+magbar,
	    dispy+halfy+halfy/2,
	    (int) (loopcolors[3].r*colormag),0,0, 127);
    
    // Show if overdub
    if (loopmgr->GetStatus(i) == T_LS_Overdubbing)
      draw_text(screen,mainfont,"O",dispx+halfx,dispy+halfy,txtclr,1,1);
    
    if (drawtext) {
      // Show loop name
      ItemRenamer *renamer = loopmgr->GetRenamer();
      if (renamer != 0 && l == loopmgr->GetRenameLoop()) {
	// This loop is being renamed- show current name
	RenameUIVars *rui = renamer->UpdateUIVars();

	if (liney == -1)
	  TTF_SizeText(smallfont,VERSION,0,&liney);
	
	// Draw text with cursor
	int sx, sy;
	int txty = dispy+fully;
	char *curn = renamer->GetCurName();
	if (*curn != '\0')
	  VideoIO::draw_text(screen,smallfont,curn,
			     dispx,txty,txtclr2,0,2,&sx,&sy);
	else {
	  sx = 0;
	  sy = liney;
	}

	if (rui->rename_cursor_toggle)
	  boxRGBA(screen,
		  dispx+sx,txty,
		  dispx+sx+sy/2,txty-sy,
		  cursorclr.r,cursorclr.g,cursorclr.b,255);
      } else if (l != 0 && l->name != 0)
	// Show name
	draw_text(screen,smallfont,l->name,
		  dispx,dispy+fully,txtclr2,0,2);

      // Show last recorded loop #
      int cnt = 0;
      for (cnt = 0; cnt < LAST_REC_COUNT && loopmgr->lastrecidx[cnt] != i; 
	   cnt++);
      if (cnt < LAST_REC_COUNT) {
	char tmp[50];
	snprintf(tmp,50,"L%d",cnt+1);
	draw_text(screen,smallfont,tmp,
		  dispx+fullx,dispy,txtclr2,2,0);
      }

#if 0 
      // Old way to label loop in 'show-all-loops' mode
      if (curel == 0) {
	char tmp[50];
	
	// Show direct-- so label the loop
	Pulse *a = loopmgr->GetPulse(i);
	long len;
	if (a != 0)
	  len = loopmgr->GetRoundedLength(i)/a->len;
	else
	  len = loopmgr->GetRoundedLength(i);
	if (len != 0)
	  snprintf(tmp,50,"%03d %ld",i,len);
	else
	  snprintf(tmp,50,"%03d",i);
	
	SDL_Color tmpclr = { (int) (loopcolors[3].r*colormag), 
			     (int) (loopcolors[3].g*colormag), 
			     (int) (loopcolors[3].b*colormag), 0 };
	draw_text(screen,mainfont,tmp,dispx,dispy+textyofs,tmpclr);
      }
#endif
    }

    loopmgr->UnlockLoops();
    return 0;
  } else {
    loopmgr->UnlockLoops();
    return 1;
  }
};

// If no suitable map exists in list 'cmaps', creates a planar>circular map
// of diameter 'sz', mapping from the given surface. 
CircularMap *VideoIO::CreateMap(SDL_Surface *lscopepic, int sz) {
  // OK, scan to see if a fitting map already exists
  CircularMap *nw;
  if (cmaps != 0)
    nw = cmaps->Scan(sz);
  else 
    nw = 0;
  if (nw == 0) {
    // No fitting map found, generate one at the right size
    int lscope_crinner = (int) (sz*0.13),
      lscope_crsize = sz/2 - lscope_crinner;

    printf("VIDEO: Generating planar->circular map @ size %d\n",sz);
    nw = new CircularMap(lscopepic, 
			 sz,sz,
			 lscopewidth,lscopeheight,
			 lscope_crinner, lscope_crsize);
    // Store a copy in our list
    if (cmaps == 0)
      cmaps = nw;
    else
      cmaps->Link(nw);
  }

  return nw;
};

// This is the video event loop
void VideoIO::video_event_loop ()
{
  FloConfig *fs = app->getCFG();
  LoopManager *loopmgr = app->getLOOPMGR();

  int XSIZE = fs->GetVSize()[0];

  const static SDL_Color red = { 0xFF, 0x50, 0x20, 0 },
    //blue = { 0x30, 0x20, 0xEF, 0 },
      white = { 0xEF, 0xAF, 0xFF, 0 },
	truewhite = { 0xFF, 0xFF, 0xFF, 0 },
	  gray = { 0x77, 0x88, 0x99, 0 },
	    yellow = { 0xDF, 0xEF, 0x20, 0 },
	      infobarclr = red;

  int nt = app->getCFG()->GetNumTriggers();

  const static int num_loopcolors = 4;
  SDL_Color loopcolors[num_loopcolors][4] = { { { 0x5F, 0x7C, 0x2B, 0 },
						{ 0xD3, 0xFF, 0x82, 0 },
						{ 0xFF, 0xFF, 0xFF, 0 },
						{ 0xDE, 0xE2, 0xD5, 0 } },
					      { { 0x8E, 0x75, 0x62, 0 }, 
						{ 0xFF, 0x9C, 0x4C, 0 },
						{ 0xFF, 0xFF, 0xFF, 0 },
						{ 0xE0, 0xDA, 0xD5, 0 } },
					      { { 0x62, 0x8C, 0x85, 0 },
						{ 0x43, 0xF2, 0xD5, 0 },
						{ 0xFF, 0xFF, 0xFF, 0 },
						{ 0xA9, 0xC6, 0xC1, 0 } },
					      { { 0x69, 0x4B, 0x89, 0 },
						{ 0xA8, 0x56, 0xFF, 0 },
						{ 0xFF, 0xFF, 0xFF, 0 },
						{ 0xDF, 0xCB, 0xF4, 0 } } };

  char tmp[255];

  curpeakidx = new nframes_t[nt];
  lastpeakidx = new nframes_t[nt];
  oldpeak = new float[nt];
  memset(curpeakidx,0,sizeof(nframes_t) * nt);
  memset(lastpeakidx,0,sizeof(nframes_t) * nt);
  for (int i = 0; i < nt; i++)
    oldpeak[i] = 1.0;  

  // Video coordinates & settings
  const int scopemag = OCY(40),
    metermag = OCY(25), 
    iscopey = OCY(350),
    //oscopex = 260,
    //oscopey = iscopey,
    //oscopemag = 100,    
    patchx = OCX(35),
    patchy = OCY(460),

    pulsex = OCX(600),
    pulsey = OCY(20),
    pulsespc = OCY(40),
    pulsepiemag = OCX(10),

    // Input scope width
    scopewidth = XSIZE,
    // Y position of time marker ticks
    tmarky = iscopey + OCY(100),

    progressbar_x = OCX(20),
    progressbar_y = OCY(400),
    progressbar_xs = OCX(640)-progressbar_x*2,
    progressbar_ys = OCY(20);

  // Flat loop scope dimensions--
  lscopewidth = OCX(320);
  lscopeheight = OCY(30);

  const static float loop_colorbase = 0.5;

  nframes_t scopelen = app->getCFG()->GetScopeSampleLen();

  // Loop scope bitmap
  Uint8 video_bpp = screen->format->BitsPerPixel;
  Uint32 Rmask = screen->format->Rmask,
    Gmask = screen->format->Gmask,
    Bmask = screen->format->Bmask,
    Amask = screen->format->Amask;
  printf("VIDEO: Creating temporary buffers at %d bits\n",video_bpp);
  lscopepic = 
    SDL_CreateRGBSurface(SDL_HWSURFACE, 
			 lscopewidth, lscopeheight, video_bpp, 
			 Rmask, 
			 Gmask, 
			 Bmask, 
			 Amask); // Flat

  // Generate circular maps for all the different sized layout elements
  // as defined in config

  // Mappings from flat to circular for loops shown at different sizes
  cmaps = 0;

  FloLayout *curlayout = fs->GetLayouts();
  while (curlayout != 0) {
    FloLayoutElement *curel = curlayout->elems;
    while (curel != 0) {
      int sz = curel->loopsize;
      if (sz > 0)
	curel->loopmap = CreateMap(lscopepic,sz);
 
      curel = curel->next;
    }

    curlayout = curlayout->next;
  }

#if 0
  // Load title image
  printf("VIDEO: Loading title bitmap.\n");
  SDL_Surface *titlepic = SDL_LoadBMP(FWEELIN_TITLE_IMAGE);
  if (titlepic == 0) {
    printf("Couldn't load title image from: %s\n"
	   "Did you run 'make install'?\n",
	   FWEELIN_TITLE_IMAGE);
    return;
  }
  SDL_Surface *titletemppic = 
    SDL_CreateRGBSurface(SDL_HWSURFACE, 
			 titlepic->w, titlepic->h, video_bpp, 
			 Rmask, 
			 Gmask, 
			 Bmask, 
			 Amask); // Flat
  // Draw final title image with fweelin version
  SDL_BlitSurface(titlepic, 0, titletemppic, 0);
  int ver_x, ver_y;
  TTF_SizeText(mainfont,VERSION,&ver_x,&ver_y);
  draw_text(titletemppic,mainfont,VERSION,
	    titletemppic->w-ver_x-15,titletemppic->h-ver_y-15,truewhite);  
  SDL_FreeSurface(titlepic);
#endif

  // Logo image
  SDL_Surface *logopic = 0;
  if (fweelin_logo.bytes_per_pixel != 4)
    printf("VIDEO: Warning: Logo image must be 32-bit.\n");
  else {
    logopic = 
      SDL_CreateRGBSurface(SDL_HWSURFACE, 
			   fweelin_logo.width, fweelin_logo.height, 32, 
			   0x000000FF, 
			   0x0000FF00, 
			   0x00FF0000, 
			   0xFF000000);
    
    memcpy(logopic->pixels,fweelin_logo.pixel_data,
	   fweelin_logo.width * fweelin_logo.height * 
	   fweelin_logo.bytes_per_pixel);
  }

  // Help setup
  const static int helpx = OCX(0),
    helpy = OCY(10),
    helpmaxy2 = OCY(460),
    maxhelppages = 255; // Maximum # of help pages
  int helpstartidx[maxhelppages], helpendidx[maxhelppages], curstartidx;
  int helpx2 = helpx,
    curhelpy2 = helpy,
    helpy2 = helpy,
    helpmaxcol1 = 0,
    helpmaxcol2 = 0;
  int x, y1, y2;

  // Size up and paginate help
  numhelppages = 0;
  curstartidx = 0;
  for (int i = 0; i < fs->GetNumHelpLines(); i++) {
    char *s1 = fs->GetHelpLine(i,0),
      *s2 = fs->GetHelpLine(i,1);
    if (s1 != 0) {
      TTF_SizeText(helpfont,s1,&x,&y1);	
      if (s2 != 0) {
	helpmaxcol1 = MAX(helpmaxcol1,x);
	TTF_SizeText(helpfont,s2,&x,&y2);
	helpmaxcol2 = MAX(helpmaxcol2,x);
      } else {
	y2 = 0;
      }
      
      if (curhelpy2 + MAX(y1,y2) >= helpmaxy2) {
	// New page
        helpstartidx[numhelppages] = curstartidx;
	helpendidx[numhelppages] = i-1;
	curstartidx = i;
	numhelppages++;
	if (numhelppages >= maxhelppages) {
	  printf("VIDEO: ERROR: Too many help pages!\n");
	  exit(1);
	}
	  
	helpy2 = MAX(helpy2,curhelpy2);
	curhelpy2 = helpy + MAX(y1,y2);
      } else {
	// Same page
	curhelpy2 += MAX(y1,y2);
      }
    }
  }
  helpstartidx[numhelppages] = curstartidx;
  helpendidx[numhelppages] = fs->GetNumHelpLines()-1;
  numhelppages++; // 1 is considered first help page
  helpx2 = helpx + helpmaxcol1 + helpmaxcol2;

  video_time = 0;
  double video_start = mygettime();
  while (videothreadgo) {
    // This video thread eats CPU
    // So for slower machines I advise using a higher delay time
    usleep(app->getCFG()->GetVDelay());

    //double t1 = mygettime();

    float lvol = app->getRP()->GetLimiterVolume();
    sample_t *peakbuf, *avgbuf;

    // Lock video space from changes
    pthread_mutex_lock (&video_thread_lock);

    // Clear screen
    SDL_FillRect(screen,NULL,0);

    // Draw layouts
#if 1
    FloLayout *curlayout = fs->GetLayouts();
    while (curlayout != 0) {
      // Draw this layout, if active
      if (curlayout->show && curlayout->elems != 0) {
	// Draw each element in this layout
	FloLayoutElement *curel = curlayout->elems;
	int firstid = curlayout->loopids.lo, 
	  curid = firstid,
	  maxid = curlayout->loopids.hi;

	while (curel != 0 && curid <= maxid) {
	  // Calculate the actual loop ID that corresponds to this element
	  int i = firstid + curel->id;

	  // What color should this element be?
	  int clrnum = i % num_loopcolors;

	  float colormag;
	  char loopexists;
	  if (loopmgr->GetSlot(i) || loopmgr->IsActive(i)) {
	    loopexists = 1;
	    colormag = loop_colorbase + loopmgr->GetTriggerVol(i);
	    if (colormag > 1.0)
	      colormag = 1.0;
	  }
	  else {
	    loopexists = 0;
	    colormag = loop_colorbase;
	  }

	  SDL_Color elclr;
	  Loop *l = loopmgr->GetSlot(i);
	  if (l != 0 && l->selcnt > 0) {
	    // Selected loop
 	    static SDL_Color selcolor[4] = { { 0xF9, 0xE6, 0x13, 0 }, 
					     { 0x62, 0x62, 0x62, 0 },
					     { 0xFF, 0xFF, 0xFF, 0 },
					     { 0xE0, 0xDA, 0xD5, 0 } };
	    elclr.r = selcolor[0].r;
	    elclr.g = selcolor[0].g;
	    elclr.b = selcolor[0].b;
	    elclr.unused = 0;
	  } else {
	    elclr.r = (int) (loopcolors[clrnum][0].r*colormag);
	    elclr.g = (int) (loopcolors[clrnum][0].g*colormag);
	    elclr.b = (int) (loopcolors[clrnum][0].b*colormag);
	    elclr.unused = 0;
	  }

	  // Draw each geometry of this element
	  FloLayoutElementGeometry *curgeo = curel->geo;
	  while (curgeo != 0) {
	    curgeo->Draw(screen,elclr);
	    curgeo = curgeo->next;
	  }

	  // Draw loop for this element
	  if (loopexists)
	    DrawLoop(loopmgr,i,screen,lscopepic,
		     loopcolors[clrnum],colormag,
		     fs,curel,0,0,0,
		     lvol);

	  // Label this element
	  if (curlayout->showelabel)
	    draw_text(screen,mainfont,curel->name,curel->nxpos,curel->nypos,
		      white);

	  curel = curel->next;
	  curid++;
	}
	
	// Label the layout
	if (curlayout->showlabel)
	  draw_text(screen,mainfont,curlayout->name,
		    curlayout->nxpos,curlayout->nypos,white);
      }

      curlayout = curlayout->next;
    }
#endif
    
    // Show pies for pulses
    int curpulsey = pulsey;
    for (int i = 0; i < MAX_PULSES; i++) {
      Pulse *a = loopmgr->GetPulseByIndex(i);
      if (a != 0) {
	if (loopmgr->GetCurPulseIndex() == i) 
	  // Selected pulse twice as big!
	  FILLED_PIE(screen,pulsex,curpulsey,pulsepiemag * 2,0,
		     (int) (360*a->GetPct()),127,127,127,255);
        else
	  FILLED_PIE(screen,pulsex,curpulsey,pulsepiemag,0,
		     (int) (360*a->GetPct()),127,127,127,255);

	sprintf(tmp,"%d",i+1);
	draw_text(screen,mainfont,tmp,pulsex-pulsepiemag,curpulsey-
		  pulsepiemag,red);

	curpulsey += pulsespc;
      }
    }

    // Draw input scope
#if 0
    peakbuf = app->getAMPEAKS()->buf,
    avgbuf = app->getAMAVGS()->buf;
    int midpt = iscopey;
    float mul = scopemag*lvol,
      // Ratio of visual size to audio scope buffer length
      ispd = scopewidth / scopelen;

    // Write into the buffer on sliding position to give animated scope effect
    float pos = -(float)app->getAMPEAKSI()->GetTotalLength2Cur()*ispd;
    if (pos < 0.)
      pos += (float)scopewidth;

    for (nframes_t i = 0; i < scopelen; i++, pos += ispd) {
      float pbi = peakbuf[i];
      int peakd = (int) (mul*pbi);
      //avgd = (int) (mul*1.5*avgbuf[i]);
      if (pos >= (float)scopewidth)
	pos = 0.;

      float peaky = avgbuf[i]/(pbi*pbi + 0.00000001)*2;
      if (peaky > 1.0)
      	peaky = 1.0;
      float rv = 0xC2 * peaky + 0xF9 * (1.-peaky),
	gv = 0x7E * peaky + 0xBB * (1.-peaky),
      	bv = 0xDD * peaky + 0x2A * (1.-peaky);
      //if (rv > 255) rv = 255;
      //if (gv > 255) gv = 255;
      //if (bv > 255) bv = 255;
    
      vlineRGBA(screen,
		(int) pos,
		midpt-peakd,
		midpt+peakd,
		(int) rv, (int) gv, (int) bv, 255);
    }

    // Scope meter marks
    BED_MarkerPoints *mp = app->getAMPEAKSPULSE();
    TimeMarker *cur = 0;
    if (mp != 0)
      cur = mp->markers;
    while (cur != 0) {      
      pos = (float) ((signed int) cur->markofs - 
		     (signed int) app->getAMPEAKSI()->GetTotalLength2Cur())
	*ispd; 
      if (pos < 0.)
	pos += (float)scopewidth;

      vlineRGBA(screen,
		(int) pos,
		tmarky-metermag,
		tmarky,
		255,255,255,255);
     
      cur = cur->next;
    }
#endif

    // Output scope
#if 0
    int ox = -1,
      oy = -1;
    for (nframes_t i = 0; i < app->getSCOPELEN(); i++) {
      int x = oscopex+i*2,
	y = oscopey+(int) (oscopemag*app->getSCOPE()[i]);
      if (ox != -1) 
	lineRGBA(screen,ox,oy,x,y,127,127,255,255);
      ox = x;
      oy = y;
    }
#endif

    // ** These two hardcoded displays could be made configurable

    // Stream output status
    char *writename = app->getSTREAMOUTNAME();
    if (strlen(writename) == 0)
      strcpy(tmp,"stream off"); // No output
    else
      sprintf(tmp,"%s   %.1f mb",
	      writename,
	      app->getSTREAMER()->GetOutputSize()/(1024.*1024));
    draw_text(screen,mainfont,tmp,patchx,patchy-OCY(22),gray);

    // Scene name
    SceneBrowserItem *curscene = app->getCURSCENE();
    if (curscene != 0)
      draw_text(screen,mainfont,curscene->name,0,0,gray);

    // **

    // Draw displays
#if 1
    FloDisplay *curdisplay = fs->GetDisplays();
    while (curdisplay != 0) {
      if (curdisplay->show)
	curdisplay->Draw(screen);
      curdisplay = curdisplay->next;
    }
#endif

    // Show save/load progress bar
    char draw_progress = 0;
    int progress_size = 0;

    if (loopmgr->GetNumSave() != 0) {
      draw_progress = 1;
      progress_size = (int) ((float)loopmgr->GetCurSave()/
			     loopmgr->GetNumSave()*
			     progressbar_xs);
    }
    if (loopmgr->GetNumLoad() != 0) {
      draw_progress = 1;
      progress_size = (int) ((float)loopmgr->GetCurLoad()/
			     loopmgr->GetNumLoad()*
			     progressbar_xs);
    }

    if (draw_progress) {
      boxRGBA(screen,progressbar_x+progress_size,progressbar_y,
	      progressbar_x+progressbar_xs,
	      progressbar_y+progressbar_ys,
	      50,50,10,255);
      boxRGBA(screen,progressbar_x,progressbar_y,
	      progressbar_x+progress_size,
	      progressbar_y+progressbar_ys,
	      255,255,30,255);
      hlineRGBA(screen,progressbar_x,progressbar_x+progress_size,
		progressbar_y,40,40,40,255);
      hlineRGBA(screen,progressbar_x,progressbar_x+progress_size,
		progressbar_y+progressbar_ys,40,40,40,255);
      vlineRGBA(screen,progressbar_x,progressbar_y,
		progressbar_y+progressbar_ys,40,40,40,255);
      vlineRGBA(screen,progressbar_x+progress_size,progressbar_y,
		progressbar_y+progressbar_ys,40,40,40,255);
    }

    // Show help on top
    if (showhelppage) {
      int spacey1, spacey2, curhelpy = helpy;

      // Dim the background
      // This doesn't work for large regions- bug in SDL_gfx?
      // boxRGBA(screen,helpx,helpy,helpx2,helpy2,255,255,255,0);
      for (int i = helpy; i <= helpy2; i++) 
	hlineRGBA(screen,helpx,helpx2,i,0,0,0,127);
      hlineRGBA(screen,helpx,helpx2,helpy,255,255,255,127);
      hlineRGBA(screen,helpx,helpx2,helpy2,255,255,255,127);
      vlineRGBA(screen,helpx,helpy,helpy2,255,255,255,127);
      vlineRGBA(screen,helpx2,helpy,helpy2,255,255,255,127);

      // Now, draw help
      for (int i = helpstartidx[showhelppage-1]; 
	   i <= helpendidx[showhelppage-1]; i++) {
	char *s1 = fs->GetHelpLine(i,0),
	  *s2 = fs->GetHelpLine(i,1);
	if (s1 != 0)
	  draw_text(screen,helpfont,s1,helpx,curhelpy,yellow,0,0,0,&spacey1);
	else 
	  spacey1 = 0;
	if (s2 != 0)
	  draw_text(screen,helpfont,s2,helpx+helpmaxcol1,curhelpy,
		    truewhite,0,0,0,&spacey2);
	else
	  spacey2 = 0;
	curhelpy += MAX(spacey1,spacey2);
      }
    }

#if 0
    // Old title image drawing--

    // Draw title image
    double video_elapsed = mygettime() - video_start;
    float titlepct = video_elapsed/0.5;
    /* if (titletemppic->format->BytesPerPixel == 4) {
       int sz = titletemppic->w*titletemppic->h;
       char *pix = (char *) titletemppic->pixels;
       char vid = (char) (video_elapsed*255);
       for (int i = 0; i < sz; i++, pix += 4)
       if (*(pix+1) != 0 || *(pix+2) != 0 || *(pix+3) != 0) {
       //	  int val = (int) ((float)rand()/RAND_MAX*vid);
       //if (val > 255)
       //  val = 255;
       *pix = vid;
       }
       else 
       *pix = 0;
       } */
    if (titlepct < 1.0) {
      int titlepos = (int) (titlepct*titletemppic->h);
      SDL_Rect dst;
      dst.x = screen->w/2-titletemppic->w/2;
      dst.y = screen->h/2-titlepos/2;
      dst.w = titletemppic->w;
      dst.h = titlepos;
      Squeeze_BlitSurface(titletemppic,screen,&dst);
    } else if (titlepct >= 1.0 && titlepct <= 4.0) {
      int titlepos = (int) (1.0*titletemppic->h);
      SDL_Rect dst;
      dst.x = screen->w/2-titletemppic->w/2;
      dst.y = screen->h/2-titlepos/2;
      dst.w = titletemppic->w;
      dst.h = titlepos;
      Squeeze_BlitSurface(titletemppic,screen,&dst);
    } else if (titlepct > 4.0 && titlepct < 5.0) {
      int titlepos = (int) ((5.0-titlepct)*titletemppic->h);
      SDL_Rect dst;
      dst.x = screen->w/2-titletemppic->w/2;
      dst.y = screen->h/2-titlepos/2;
      dst.w = titletemppic->w;
      dst.h = titlepos;
      Squeeze_BlitSurface(titletemppic,screen,&dst);
    } else if (titletemppic != 0) {
      SDL_FreeSurface(titletemppic);
      titletemppic = 0;
    }
#endif

    // Draw logo
#if 1
    if (logopic != 0)
    {
      double video_elapsed = mygettime() - video_start;
      float titlepct = video_elapsed;

      float t_floatin = 2.0,
	t_floatout = 4.0;
      if (titlepct < 1.0) {
	SDL_Rect dst;
	dst.x = screen->w-logopic->w;
	dst.y = (int) (-logopic->h + screen->h*titlepct);
	dst.w = logopic->w;
	dst.h = logopic->h;
	SDL_BlitSurface(logopic, NULL, screen, &dst);	
      } else if (titlepct > t_floatin && titlepct < t_floatin+1.0) {
	SDL_Rect dst;
	dst.x = screen->w-logopic->w;
	dst.y = screen->h-logopic->h;
	dst.w = logopic->w;
	dst.h = logopic->h;
	SDL_BlitSurface(logopic, NULL, screen, &dst);
	int ver_x, ver_y;
	TTF_SizeText(mainfont,VERSION,&ver_x,&ver_y);
	draw_text(screen,mainfont,VERSION,
		  (int) (screen->w-(titlepct-t_floatin)*(ver_x+5)),
		  screen->h-ver_y-5,truewhite);
      } else if (titlepct >= t_floatin && titlepct <= t_floatout) {
	SDL_Rect dst;
	dst.x = screen->w-logopic->w;
	dst.y = screen->h-logopic->h;
	dst.w = logopic->w;
	dst.h = logopic->h;
	SDL_BlitSurface(logopic, NULL, screen, &dst);
	int ver_x, ver_y;
	TTF_SizeText(mainfont,VERSION,&ver_x,&ver_y);
	draw_text(screen,mainfont,VERSION,
		  screen->w-(ver_x+5),
		  screen->h-ver_y-5,truewhite);
      } else if (titlepct > t_floatout && titlepct < t_floatout+1.0) {
	SDL_Rect dst;
	dst.x = screen->w-logopic->w;
	dst.y = screen->h-logopic->h;
	dst.w = logopic->w;
	dst.h = logopic->h;
	SDL_BlitSurface(logopic, NULL, screen, &dst);
	int ver_x, ver_y;
	TTF_SizeText(mainfont,VERSION,&ver_x,&ver_y);
	draw_text(screen,mainfont,VERSION,
		  (int) (screen->w-(1.0-(titlepct-t_floatout))*(ver_x+5)),
		  screen->h-ver_y-5,truewhite);
      } else {
	SDL_Rect dst;
	dst.x = screen->w-logopic->w;
	dst.y = screen->h-logopic->h;
	dst.w = logopic->w;
	dst.h = logopic->h;
	SDL_BlitSurface(logopic, NULL, screen, &dst);
      }
    }
#endif

    // Now update screen!
    SDL_UpdateRect(screen, 0, 0, 0, 0);
    //video_time = mygettime() - t1;

    // Unlock video space from changes
    pthread_mutex_unlock (&video_thread_lock);
  }

  delete[] curpeakidx;
  delete[] lastpeakidx;
  delete[] oldpeak;

  // Erase circular maps
  CircularMap *cur = cmaps;
  while (cur != 0) {
    CircularMap *tmp = cur->next; 
    delete cur;
    cur = tmp;
  }
    
  // Close things up
  if (logopic != 0) 
    SDL_FreeSurface(logopic);
  SDL_FreeSurface(lscopepic);
}

void VideoIO::SetVideoMode(char fullscreen) {
  const SDL_VideoInfo *info;
  Uint8  video_bpp;
  Uint32 videoflags;

  pthread_mutex_lock (&video_thread_lock);

  if (screen != 0)
    // Free existing screen
    SDL_FreeSurface(screen);
  screen = 0;

  /* Alpha blending doesn't work well at 8-bit color */
  info = SDL_GetVideoInfo();
  if ( info->vfmt->BitsPerPixel > 8 ) {
    video_bpp = info->vfmt->BitsPerPixel;
  } else {
    video_bpp = 16;
  }
  printf("VIDEO: SetVideoMode: Using %d-bit color\n", video_bpp);

  // Disabled (slower) options:
  /*| SDL_SRCALPHA | SDL_RESIZABLE |*/
  videoflags = SDL_HWSURFACE | SDL_DOUBLEBUF;
  this->fullscreen = fullscreen;
  if (fullscreen) 
    videoflags |= SDL_FULLSCREEN;

  /* Set 640x480 video mode */
  int XSIZE = app->getCFG()->GetVSize()[0],
    YSIZE = app->getCFG()->GetVSize()[1];
  if ( (screen=SDL_SetVideoMode(XSIZE,YSIZE,video_bpp,videoflags)) == NULL ) {
    printf("VIDEO: Couldn't set %ix%i video mode: %s\n",XSIZE,YSIZE,
	   SDL_GetError());
    exit(1);
  }

  /* Use alpha blending */
  //SDL_SetAlpha(inst->screen, SDL_SRCALPHA, 0);
 
  /* Set title for window */
  SDL_WM_SetCaption("FreeWheeling","FreeWheeling");

  pthread_mutex_unlock (&video_thread_lock);
}

void *VideoIO::run_video_thread(void *ptr)
{
  VideoIO *inst = static_cast<VideoIO *>(ptr);

  printf("VIDEO: Thread start..\n");

#ifdef __MACOSX__
  inst->cocoa.SetupCocoaThread();
#endif
  
#if 0
  // This init was moved to fweelin_core because SDL is required
  // for keyboard and config file reading as well..

  if ( SDL_InitSubSystem(SDL_INIT_VIDEO) < 0 ) {
    fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
    return 0;
  }
#endif

  // Initialize the font library
  if ( TTF_Init() < 0 ) {
    fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
    return 0;
  }
  atexit(TTF_Quit);

  // Load all fonts
  inst->mainfont = 0;
  inst->helpfont = 0;
  inst->smallfont = 0;
  {
    FloFont *cur = inst->app->getCFG()->GetFonts();
    char tmp[255];
    while (cur != 0) {
      if (cur->name != 0 && cur->filename != 0 && cur->size != 0) {
	snprintf(tmp,255,"%s/%s",FWEELIN_DATADIR,cur->filename);
	printf("VIDEO: Loading font %s: %s (%d pt)\n",cur->name,tmp,cur->size);

	struct stat st;
	if (stat(tmp,&st) != 0) {
	  printf("VIDEO: Couldn't find font file: %s\n"
		 "Did you run 'make install'?\n",tmp);
	  exit(1);
	}
	  
	cur->font = TTF_OpenFont(tmp, cur->size);
	if (cur->font == 0) {
	  printf("VIDEO: Couldn't load %d pt font from: %s\n"
		 "Did you run 'make install'?\n",
		 cur->size, tmp);
	  exit(1);
	}
	TTF_SetFontStyle(cur->font, TTF_STYLE_NORMAL);
	
	// Check if this is a font we use
	if (!strcmp(cur->name,"main")) 
	  inst->mainfont = cur->font;
	else if (!strcmp(cur->name,"help"))
	  inst->helpfont = cur->font;
	else if (!strcmp(cur->name,"small"))
	  inst->smallfont = cur->font;
      }
      
      cur = cur->next;
    }
  }

  if (inst->mainfont == 0) {
    printf("VIDEO: Error, no 'main' font loaded!\n");
    exit(1);
  }
  if (inst->helpfont == 0) {
    printf("VIDEO: Error, no 'help' font loaded!\n");
    exit(1);
  }
  if (inst->smallfont == 0) {
    printf("VIDEO: Error, no 'small' font loaded!\n");
    exit(1);
  }

  inst->videothreadgo = 1;
  printf("VIDEO: SDL Ready!\n");

  // Wait until we are actually running!
  while (!inst->app->IsRunning())
    usleep(10000);

#ifndef __MACOSX__
  // On Mac OS X, this is done in the main thread
  // Set video mode / window size
  inst->SetVideoMode(0);
#endif
  
  // Start main loop
  inst->video_event_loop();

  // Close all fonts
  {
    FloFont *cur = inst->app->getCFG()->GetFonts();
    while (cur != 0) {
      if (cur->font != 0)
	TTF_CloseFont(cur->font);
      cur = cur->next;
    }
  }

  // Close things up
  SDL_QuitSubSystem(SDL_INIT_VIDEO);

#ifdef __MACOSX__
  inst->cocoa.TakedownCocoaThread();
#endif
  
  printf("VIDEO: thread done\n");
  return 0;
}
