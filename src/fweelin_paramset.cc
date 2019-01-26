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
#include "fweelin_paramset.h"

// Link system variables for each active parameter to the actual parameter
// values, based on the current bank and parameter page.
#if 0
void FloDisplayParamSet::LinkActiveParams() {
  for (int i = 0; i < numactiveparams; i++) {
    activeparam[i]->type = T_float;

    ParamSetBank *b = &banks[curbank];
    if (b->firstparamidx + i < b->numparams)
      activeparam[i]->value = (char *) &(b->params[b->firstparamidx].value);
    else
      activeparam[i]->value = (char *) &invalidparam;
  }
};
#endif

void FloDisplayParamSet::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
    case T_EV_ParamSetGetAbsoluteParamIdx :
      {
        ParamSetGetAbsoluteParamIdxEvent *psev = (ParamSetGetAbsoluteParamIdxEvent *) ev;

        if (psev->interfaceid == iid && psev->displayid == id) {
          // Message is for this param set display

          if (psev->absidx == 0 || psev->absidx->IsSystemVariable())
            printf(" ParamSetGetAbsoluteParamIdxEvent: The variable provided for the 'absidx' parameter is missing or invalid\n");
          else {
            if (psev->absidx->GetType() != T_int)
              printf("PARAMSET: ParamSetGetAbsoluteParamIdxEvent: 'absidx' parameter requires a variable of type int.");
            else {
              ParamSetBank *b = &banks[curbank];
              int idx = b->firstparamidx + psev->paramidx;
              if (idx < 0)
                idx = 0;
              else if (idx >= b->numparams)
                idx = b->numparams-1;

              *psev->absidx = idx;

              if (CRITTERS)
                printf("PARAMSET: Received ParamSetGetAbsoluteParamIdx (interfaceid: %d displayid: %d) Relative index: %d Returned absolute index: %d\n",
                    psev->interfaceid,psev->displayid,psev->paramidx,idx);
            }
          }
        }
      }
      break;

    case T_EV_ParamSetSetParam :
      {
        ParamSetSetParamEvent *psev = (ParamSetSetParamEvent *) ev;

        if (psev->interfaceid == iid && psev->displayid == id) {
          // Message is for this param set display

          ParamSetBank *b = &banks[curbank];
          int idx = b->firstparamidx + psev->paramidx;
          if (idx >= 0 && idx < b->numparams)
            b->params[idx].value = psev->value;

          if (CRITTERS) {
            printf("PARAMSET: Received ParamSetSetParam (interfaceid: %d displayid: %d) Relative index: %d Absolute index: %d Value: %f",
                psev->interfaceid,psev->displayid,psev->paramidx,idx,psev->value);
            printf("\n");
          }
        }
      }
      break;

    case T_EV_ParamSetGetParam :
      {
        ParamSetGetParamEvent *psev = (ParamSetGetParamEvent *) ev;

        if (psev->interfaceid == iid && psev->displayid == id) {
          // Message is for this param set display

          if (psev->var == 0 || psev->var->IsSystemVariable())
            printf(" ParamSetGetParamEvent: Invalid variable!\n");
          else {
            if (psev->var->GetType() != T_float)
              printf("PARAMSET: ParamSetGetParamEvent: 'var' must be of type float.");
            else {
              ParamSetBank *b = &banks[curbank];
              int idx = b->firstparamidx + psev->paramidx;
              if (idx >= 0 && idx < b->numparams)
                *psev->var = b->params[idx].value;
              else
                *psev->var = (float) 0.;

              if (CRITTERS) {
                printf("PARAMSET: Received ParamSetGetParam (interfaceid: %d displayid: %d) Relative index: %d Absolute index: %d Val: ",
                    psev->interfaceid,psev->displayid,psev->paramidx,idx);
                psev->var->Print();
                printf("\n");
              }
            }
          }
        }
      }
      break;

    case T_EV_VideoShowParamSetBank :
      {
        VideoShowParamSetBankEvent *psev = (VideoShowParamSetBankEvent *) ev;

        if (psev->interfaceid == iid && psev->displayid == id) {
          // Message is for this param set display

          // Adjust bank
          int newbank = curbank + psev->bank;
          if (newbank < 0)
            newbank = 0;
          if (newbank >= numbanks)
            newbank = numbanks-1;
          curbank = newbank;

          // LinkActiveParams();

          if (CRITTERS)
            printf("PARAMSET: Received VideoShowParamSetBank(interfaceid: %d displayid: %d bank: %d) - Bank # is now %d\n",
                psev->interfaceid,psev->displayid,psev->bank,curbank);
        }
      }
      break;

    case T_EV_VideoShowParamSetPage :
    {
      VideoShowParamSetPageEvent *psev = (VideoShowParamSetPageEvent *) ev;

      if (psev->interfaceid == iid && psev->displayid == id) {
        // Message is for this param set display

        // Adjust page in the bank
        int newfirstidx = banks[curbank].firstparamidx + numactiveparams * psev->page;
        if (newfirstidx < 0)
          newfirstidx = 0;
        if (newfirstidx >= banks[curbank].numparams)
          newfirstidx = banks[curbank].firstparamidx;

        banks[curbank].firstparamidx = newfirstidx;

        // LinkActiveParams();

        if (CRITTERS)
          printf("PARAMSET: Received VideoShowParamSetPage(interfaceid: %d displayid: %d page: %d) - First parameter index is now %d\n",
              psev->interfaceid,psev->displayid,psev->page,newfirstidx);
      }
      break;
    }

    default:
      break;
  }
}
