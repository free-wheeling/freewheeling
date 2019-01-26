#ifndef __FWEELIN_PARAMSET_H
#define __FWEELIN_PARAMSET_H

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

class ParamSetParam {
public:
  ParamSetParam(char *name = 0, float value = 0) : name(0), value(value) {
    SetName(name);
  };

  void SetName(char *name) {
    if (this->name != 0)
      delete[] this->name;

    if (name != 0) {
      printf("(param name: %s) ",name);

      this->name = new char[strlen(name)+1];
      strcpy(this->name,name);
    }
  }

  char *name;   // Name of parameter
  float value;  // Value of parameter
};

class ParamSetBank {
public:
  ParamSetBank() : name(0), firstparamidx(0), maxvalue(1.0), params(0) {};
  ~ParamSetBank() {
    if (params != 0)
      delete[] params;
  };

  void Setup(char *name, int numparams, float maxvalue) {
    this->numparams = numparams;
    this->maxvalue = maxvalue;
    SetName(name);
    params = new ParamSetParam[numparams];

    printf("(bank %s with %d params) ",name,numparams);
  };

  void SetName(char *name) {
    if (this->name != 0)
      delete[] this->name;

    if (name != 0) {
      this->name = new char[strlen(name)+1];
      strcpy(this->name,name);
    }
  }

  char *name;       // Of bank
  int numparams,    // Number of parameters in bank
    firstparamidx;  // Index of first parameter currently shown
  float maxvalue;   // Maximum value of any parameter in this bank
                    // (used for scaling the visual)

  ParamSetParam *params;  // Array of parameters
};

class FloDisplayParamSet : public FloDisplay, public EventListener, public EventProducer {
public:
  FloDisplayParamSet (Fweelin *app, char *name, int iid, int numactiveparams, int numbanks, int sx, int sy) : FloDisplay(iid),
    app(app), name(name), numactiveparams(numactiveparams), invalidparam(0.), sx(sx), sy(sy), numbanks(numbanks), curbank(0) {
    this->name = new char[strlen(name)+1];
    strcpy(this->name,name);

    banks = new ParamSetBank[numbanks];
    activeparam = new UserVariable *[numactiveparams];

    // SetupSystemVariables();
  };
  ~FloDisplayParamSet() {
    app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowParamSetBank);
    app->getEMG()->UnlistenEvent(this,0,T_EV_VideoShowParamSetPage);
    app->getEMG()->UnlistenEvent(this,0,T_EV_ParamSetGetAbsoluteParamIdx);
    app->getEMG()->UnlistenEvent(this,0,T_EV_ParamSetSetParam);
    app->getEMG()->UnlistenEvent(this,0,T_EV_ParamSetGetParam);

    delete[] name;
    delete[] banks;
    delete[] activeparam;
  };

#if 0
  // Link system variables for each active parameter to the actual parameter
  // values, based on the current bank and parameter page.
  void LinkActiveParams();
  
  void SetupSystemVariables() {
    int slen = strlen(name) + 255;
    char tmp[slen+1];

    for (int i = 0; i < numactiveparams; i++) {
      snprintf(tmp,slen,"SYSTEM_paramset_%s_activeparam%d_value",name,i);
      activeparam[i] = app->getCFG()->AddEmptyVariable(tmp);
      printf("CONFIG: Add parameter set global variable: %s\n",tmp);
    }

    LinkActiveParams();
  };
#endif

  void ListenEvents() {
    app->getEMG()->ListenEvent(this,0,T_EV_VideoShowParamSetBank);
    app->getEMG()->ListenEvent(this,0,T_EV_VideoShowParamSetPage);
    app->getEMG()->ListenEvent(this,0,T_EV_ParamSetGetAbsoluteParamIdx);
    app->getEMG()->ListenEvent(this,0,T_EV_ParamSetGetParam);
    app->getEMG()->ListenEvent(this,0,T_EV_ParamSetSetParam);
  };

  virtual FloDisplayType GetFloDisplayType() { return FD_ParamSet; };

  virtual void Draw(SDL_Surface *screen);
  
  virtual void ReceiveEvent(Event *ev, EventProducer */*from*/);

  Fweelin *app;
  char *name;           // Of parameter set

  int numactiveparams;  // Number of parameters active & displayed at a time
  UserVariable **activeparam; // Array of pointers to active parameters,
                              // stored as system variables
  float invalidparam;   // Placeholder value for invalid parameter

  int sx, sy,           // Size of parameter set display
    margin;             // Margin for text

  int numbanks,         // Number of banks
    curbank;            // Currently active bank
  ParamSetBank *banks;  // Array of all banks
};

#endif
