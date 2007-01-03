/*
   Control is such a trick--

   We can guide the ship
   but are we ever really in control
   of where we land?
*/

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

#include "fweelin_midiio.h"
#include "fweelin_core.h"

// ******** MIDI 

#define MIDI_CLIENT_NAME "FreeWheeling"

#ifdef __MACOSX__

// ******** MACOSX MIDI

void MidiIO::SetMIDIInput (int idx) {
  if (inputidx != -1) {
    // Deselect previous source
    MIDIEndpointRef src = MIDIGetSource(inputidx);
    if (src != 0) {
      //printf("DISCONNECT MIDI: %d\n",inputidx);
      MIDIPortDisconnectSource(in_ports[0], src);
    }
  }
  
  inputidx = idx;
  
  // Select new source
  MIDIEndpointRef src = MIDIGetSource(inputidx);
  if (src != 0) {
    //printf("CONNECT MIDI: %d\n",inputidx);
    MIDIPortConnectSource(in_ports[0], src, 0);
  }
};

// Open MIDI engine with num_in writeable ports and num_out readable ports.
// Nonzero is returned on error
char MidiIO::open_midi (int num_in, int num_out) {
  if (MIDIClientCreate(CFSTR(MIDI_CLIENT_NAME), 0, 0, &client) != noErr) {
    fprintf(stderr, "MIDI: Error opening MIDI client.\n");
    return -1;
  }
  
  int l1;
  char portname[64];
  
  numins = num_in;
  numouts = num_out;
  in_ports = new MIDIPortRef[num_in];
  out_ports = new MIDIPortRef[num_out];
  out_sources = new MIDIEndpointRef[num_out];
  
  for (l1 = 0; l1 < num_in; l1++) {
    sprintf(portname, MIDI_CLIENT_NAME " IN %d", l1+1);
    if (MIDIInputPortCreate(client, CFStringCreateWithCString(0,portname,kCFStringEncodingUTF8), MidiInputProc, this, &in_ports[l1]) != noErr) {
      fprintf(stderr, "MIDI: Error creating MIDI port.\n");
      return -1;
    }
  }
  for (l1 = 0; l1 < num_out; l1++) {
    sprintf(portname, MIDI_CLIENT_NAME " OUT %d", l1+1);
    if (MIDIOutputPortCreate(client, CFStringCreateWithCString(0,portname,kCFStringEncodingUTF8), &out_ports[l1]) != noErr) {	
      fprintf(stderr, "MIDI: Error creating MIDI port.\n");
      return -1;
    }
  }
    
  // Make list of input sources
  int n = MIDIGetNumberOfSources();
  printf("MIDI: Input possible from %d sources.\n", n);
  CFStringRef endname;
  char cendname[1024];
  
  FweelinMac::ClearMIDIInputList();
  for (int i = 0; i < n; ++i) {
    MIDIEndpointRef src = MIDIGetSource(i);
    MIDIObjectGetStringProperty(src, kMIDIPropertyName, &endname);
    CFStringGetCString(endname, cendname, 1024, kCFStringEncodingUTF8);
    printf("  MIDI: source name: %s\n",cendname);
    
    FweelinMac::AddMIDIInputSource(cendname);
  }
  
  // Connect first input
  SetMIDIInput(0);
  
#if 0
  // We no longer use MIDISend to send to destinations--
  // because many apps connect to sources, we instead create virtual sources for each output
  // and use MIDIReceived to send to them
  
  // Discover a place to send MIDI data
  n = MIDIGetNumberOfDestinations();
  printf("MIDI: Output possible to %d destinations.\n",n);
  if (n > 0)
    dest = MIDIGetDestination(n-1);
  
  if (dest == 0) {
    printf("MIDI: WARNING: No place to send MIDI.\n");
    return 0; // No place to send data
  }
  
  // Get name of destination we are sending to
  CFStringRef pName;
  char name[64];
  MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &pName);
  CFStringGetCString(pName, name, sizeof(name), 0);
  CFRelease(pName);
  printf("MIDI: Sending MIDI to destination: %s\n", name);

  // MIDI transmit using destinations
#define MIDI_TRANSMIT(idx) MIDISend(out_ports[idx], dest, packetList)

#else
  dest = 0;

  // Create virtual sources
  char sourcename[64];
  for (int i = 0; i < num_out; i++) {
    sprintf(sourcename, MIDI_CLIENT_NAME " OUT %d", i+1);
    if (MIDISourceCreate(client,CFStringCreateWithCString(0,sourcename,kCFStringEncodingUTF8),
			 &out_sources[i]) != noErr) {
      fprintf(stderr, "MIDI: Error creating virtual MIDI source.\n");
      return -1;
    }
  }

// MIDI transmit using virtual sources
#define MIDI_TRANSMIT(idx) MIDIReceived(out_sources[idx], packetList)

#endif
  
  return 0;
}

int MidiIO::activate() {
  FloConfig *fs = app->getCFG();

  // Open up MIDI
  if (open_midi(1, fs->GetNumMIDIOuts())) {
    fprintf(stderr, "MIDI: Can't open MIDI client.\n");
    exit(1);
  }
  
  listen_events();

  // Request initial patch from patch browser
  PatchBrowser *br = (PatchBrowser *) app->getBROWSER(B_Patch);
  if (br != 0)
    br->SetMIDIEcho();

  return 0;
}

void MidiIO::close() {
  printf("MIDI: begin close...\n");
  
  unlisten_events();

  printf("MIDI: end\n");
}

void MidiIO::MidiInputProc (const MIDIPacketList *pktlist, void *refCon, void *connRefCon) {
  MidiIO *inst = static_cast<MidiIO *>(refCon);
 
  MIDIPacket *packet = (MIDIPacket *) pktlist->packet;
  for (int j = 0; j < pktlist->numPackets; j++) {
    if ((packet->data[0] >= 0x80 && packet->data[0] <= 0x8F) ||
	(packet->data[0] >= 0x90 && packet->data[0] <= 0x9F &&
	 packet->data[2] == 0)) {			// Interpret NoteOn velocity 0 as note off
      // Note Off
      int base = (packet->data[0] >= 0x90 ? 0x90 : 0x80);
      inst->ReceiveNoteOffEvent(packet->data[0] - base,
				packet->data[1],	
				packet->data[2]);
    } else if (packet->data[0] >= 0x90 && packet->data[0] <= 0x9F) {
      // Note On
      inst->ReceiveNoteOnEvent(packet->data[0] - 0x90,
			       packet->data[1],	
			       packet->data[2]);
    } else if (packet->data[0] >= 0xB0 && packet->data[0] <= 0xBF) {
      // Control Change
      inst->ReceiveControlChangeEvent(packet->data[0] - 0xB0,
				      packet->data[1],
				      packet->data[2]);
    } else if (packet->data[0] >= 0xC0 && packet->data[0] <= 0xCF) {
      // Program Change
      inst->ReceiveProgramChangeEvent(packet->data[0] - 0xC0,
				      packet->data[1]);
    } else if (packet->data[0] >= 0xE0 && packet->data[0] <= 0xEF) {
      // Pitch Bend
      int lsb = packet->data[1],
	msb = packet->data[2],
	benderval = msb << 8 + lsb;
      
      inst->ReceivePitchBendEvent(packet->data[0] - 0xE0,
				  benderval);
    }   
    
    // Advance
    packet = MIDIPacketNext(packet);
  }  
}

void MidiIO::OutputController (int port, int chan, int ctrl, int val) {
  unsigned char msg[3];	
  msg[0] = 0xB0 + chan;
  msg[1] = (unsigned char) ctrl;
  msg[2] = (unsigned char) val;
  
  curPacket = MIDIPacketListAdd(packetList,sizeof(obuf),
				curPacket,0 /*Now*/,3,msg);
};

void MidiIO::OutputProgramChange (int port, int chan, int val) {
  unsigned char msg[2];	
  msg[0] = 0xC0 + chan;
  msg[1] = (unsigned char) val;
  
  curPacket = MIDIPacketListAdd(packetList,sizeof(obuf),
				curPacket,0 /*Now*/,2,msg);
};

void MidiIO::OutputPitchBend (int port, int chan, int val) {
  unsigned char msg[3];	
  msg[0] = 0xE0 + chan;
  msg[1] = (unsigned char) (val & 0x00FF);	// LSB
  msg[2] = (unsigned char) (val >> 8);	        // MSB
  
  curPacket = MIDIPacketListAdd(packetList,sizeof(obuf),
				curPacket,0 /*Now*/,3,msg);
};

void MidiIO::OutputNote (int port, int chan, char down, int notenum, int vel) {
  unsigned char msg[3];	
  if (down)
    msg[0] = 0x90 + chan; // NoteOn
  else
    msg[0] = 0x80 + chan; // NoteOff
  msg[1] = (unsigned char) notenum;
  msg[2] = (unsigned char) vel;
  
  curPacket = MIDIPacketListAdd(packetList,sizeof(obuf),
				curPacket,0 /*Now*/,3,msg);
};

void MidiIO::OutputStartOnPort () {
  // Init output packet
  curPacket = MIDIPacketListInit(packetList);
};

void MidiIO::OutputEndOnPort (int port) {
  // Send MIDI packet- all messages for this port
  MIDI_TRANSMIT(port);
};

#else

// ******** LINUX MIDI

// Open MIDI engine with num_in writeable ports and num_out readable ports.
// Nonzero is returned on error
char MidiIO::open_midi (int num_in, int num_out) {
  int l1;
  char portname[64];
  
  numins = num_in;
  numouts = num_out;
  in_ports = new int[num_in];
  out_ports = new int[num_out];
  
  if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    fprintf(stderr, "MIDI: Error opening ALSA MIDI.\n");
    return -1;
  }
  snd_seq_set_client_name(seq_handle, MIDI_CLIENT_NAME);
  for (l1 = 0; l1 < num_in; l1++) {
    sprintf(portname, MIDI_CLIENT_NAME " IN %d", l1+1);
    if ((in_ports[l1] = snd_seq_create_simple_port(seq_handle, portname,
						   SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
						   SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
      fprintf(stderr, "MIDI: Error creating MIDI port.\n");
      return -1;
    }
  }
  for (l1 = 0; l1 < num_out; l1++) {
    sprintf(portname, MIDI_CLIENT_NAME " OUT %d", l1+1);
    if ((out_ports[l1] = snd_seq_create_simple_port(seq_handle, portname,
						    SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
						    SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
      fprintf(stderr, "MIDI: Error creating MIDI port.\n");
      return -1;
    }
  }
  
  return 0;
}

int MidiIO::activate() {
  // Linux MIDI handling using ALSA seq on its own thread
  printf("MIDI: Starting MIDI thread..\n");

  const static size_t STACKSIZE = 1024*64;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("MIDI: Stacksize: %d.\n",STACKSIZE);

  midithreadgo = 1;
  int ret = pthread_create(&midi_thread,
			   &attr,
			   run_midi_thread,
			   this);
  if (ret != 0) {
    printf("(start) pthread_create failed, exiting");
    return 1;
  }

  // Setup high priority threads
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  // MIDI thread at SCHED_FIFO- yes! High priority!
  //schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
  //schp.sched_priority = sched_get_priority_min(SCHED_FIFO);
  schp.sched_priority = sched_get_priority_max(SCHED_FIFO);
  printf("MIDI: HiPri Thread %d\n",schp.sched_priority);
  if (pthread_setschedparam(midi_thread, SCHED_FIFO /* OTHER */, &schp) != 0) {    
    printf("MIDI: Can't set realtime thread, will use nonRT!\n");
  }

  // Request initial patch from patch browser
  PatchBrowser *br = (PatchBrowser *) app->getBROWSER(B_Patch);
  if (br != 0)
    br->SetMIDIEcho();

  return 0;
}

void MidiIO::close() {
  printf("MIDI: begin close...\n");
  midithreadgo = 0;
  pthread_join(midi_thread,0);
  printf("MIDI: end\n");
}

void *MidiIO::run_midi_thread(void *ptr)
{
  MidiIO *inst = static_cast<MidiIO *>(ptr);
  int npfd;
  struct pollfd *pfd;
  
  FloConfig *fs = inst->app->getCFG();
  
  printf("MIDIthread start..\n");
  
  // Open up ALSA MIDI client
  if (inst->open_midi(1, fs->GetNumMIDIOuts())) {
    fprintf(stderr, "MIDI: Can't open ALSA MIDI.\n");
    exit(1);
  }
  npfd = snd_seq_poll_descriptors_count(inst->seq_handle, POLLIN);
  pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  snd_seq_poll_descriptors(inst->seq_handle, pfd, npfd, POLLIN);
  
  inst->listen_events();
  
  // Main MIDI loop
  while (inst->midithreadgo) {
    // Every second, come out of poll-- so we don't get locked up
    // if no MIDI events come in
    if (poll(pfd, npfd, 1000) > 0) {
      // There's an event here! Read!
      snd_seq_event_t *ev;
      //      static int cnt = 0;
      do {
	snd_seq_event_input(inst->seq_handle, &ev);
	switch (ev->type) {
#if 0
	  case SND_SEQ_EVENT_QFRAME:
	    printf("MTC quarterframe\n");
	    break;
	    
	  case SND_SEQ_EVENT_CLOCK:
	    if (cnt % 24 == 0)
	      printf("clock: %d\n",cnt);
	    cnt++;
	    //printf("clock: %d\n",ev->data.time.tick);
	    //printf("clock: %d\n",ev->data.time.time.tv_sec);
	    //printf("clock: %d\n",ev->data.queue.param.time.time.tv_sec);
	    break;
	    
	  case SND_SEQ_EVENT_TICK:
	    printf("MIDI: 'tick' not yet implemented\n");
	    break;
	    
	  case SND_SEQ_EVENT_TEMPO:
	    printf("MIDI: 'tempo' not yet implemented\n");
	    break;
#endif
	    
	  case SND_SEQ_EVENT_CONTROLLER: 
	    // Control Change
	    inst->ReceiveControlChangeEvent(ev->data.control.channel,
					    ev->data.control.param,
					    ev->data.control.value);
	    break;
	    
	  case SND_SEQ_EVENT_PGMCHANGE:
	    // Program Change
	    inst->ReceiveProgramChangeEvent(ev->data.control.channel,
					    ev->data.control.value);
	    break;
	    
	  case SND_SEQ_EVENT_PITCHBEND:
	    // Pitch Bend
	    inst->ReceivePitchBendEvent(ev->data.control.channel,
					ev->data.control.value);
	    break;
	    
	  case SND_SEQ_EVENT_NOTEON:
	    // Note On
	    inst->ReceiveNoteOnEvent(ev->data.control.channel,
				     ev->data.note.note,
				     ev->data.note.velocity);
	    break;
	    
	  case SND_SEQ_EVENT_NOTEOFF: 
	    // Note Off
	    inst->ReceiveNoteOffEvent(ev->data.control.channel,
				      ev->data.note.note,
				      ev->data.note.velocity);
	    break;
	}
	
	snd_seq_free_event(ev);
      } while (snd_seq_event_input_pending(inst->seq_handle, 0) > 0);
    }
  }
  
  inst->unlisten_events();
  
  printf("MIDI: thread done\n");
  return 0;
}

void MidiIO::OutputController (int port, int chan, int ctrl, int val) {
  snd_seq_event_t outev;
  snd_seq_ev_set_subs(&outev);
  snd_seq_ev_set_direct(&outev);
  snd_seq_ev_set_source(&outev,out_ports[port]);
  snd_seq_ev_set_controller(&outev,chan,ctrl,val);
  snd_seq_event_output_direct(seq_handle, &outev);
};

void MidiIO::OutputProgramChange (int port, int chan, int val) {
  snd_seq_event_t outev;
  snd_seq_ev_set_subs(&outev);
  snd_seq_ev_set_direct(&outev);
  snd_seq_ev_set_source(&outev,out_ports[port]);
  snd_seq_ev_set_pgmchange(&outev,chan,val);
  snd_seq_event_output_direct(seq_handle, &outev);
};

void MidiIO::OutputPitchBend (int port, int chan, int val) {
  snd_seq_event_t outev;
  snd_seq_ev_set_subs(&outev);
  snd_seq_ev_set_direct(&outev);
  snd_seq_ev_set_source(&outev,out_ports[port]);
  snd_seq_ev_set_pitchbend(&outev,chan,val);
  snd_seq_event_output_direct(seq_handle, &outev);
};

void MidiIO::OutputNote (int port, int chan, char down, int notenum, int vel) {
  snd_seq_event_t outev;
  snd_seq_ev_set_subs(&outev);
  snd_seq_ev_set_direct(&outev);
  snd_seq_ev_set_source(&outev,out_ports[port]);
  if (down)
    snd_seq_ev_set_noteon(&outev,chan,notenum,vel);
  else
    snd_seq_ev_set_noteoff(&outev,chan,notenum,vel);
  snd_seq_event_output_direct(seq_handle, &outev);
};

// Events are sent directly- no buffer emptying necessary
void MidiIO::OutputStartOnPort () {};
void MidiIO::OutputEndOnPort (int port) {};

#endif

// ******** CROSS-PLATFORM MIDI CODE

MidiIO::MidiIO (Fweelin *app) : bendertune(0), curbender(0), 
				echoport(1), echochan(-1), curpatch(0), 
				numins(0), numouts(0), app(app), 
#ifdef __MACOSX__
				client(0), in_ports(0), out_ports(0), out_sources(0), dest(0), 
				inputidx(-1), curPacket(0),
				packetList((MIDIPacketList *) obuf)
#else
                                seq_handle(0), in_ports(0), out_ports(0),
                                midithreadgo(0) 
#endif
{
  note_def_port = new int[MAX_MIDI_NOTES];
  note_patch = new PatchItem *[MAX_MIDI_NOTES];
  memset(note_def_port,0,sizeof(int) * MAX_MIDI_NOTES);
  memset(note_patch,0,sizeof(PatchItem *) * MAX_MIDI_NOTES);  
};

void MidiIO::listen_events () {
  FloConfig *fs = app->getCFG();

  app->getEMG()->ListenEvent(this,0,T_EV_SetMidiTuning);
  app->getEMG()->ListenEvent(this,0,T_EV_SetMidiEchoPort);
  app->getEMG()->ListenEvent(this,0,T_EV_SetMidiEchoChannel);
  app->getEMG()->ListenEvent(this,fs->GetInputMatrix(),
				  T_EV_Input_MIDIKey);
  app->getEMG()->ListenEvent(this,fs->GetInputMatrix(),
				  T_EV_Input_MIDIController);
  app->getEMG()->ListenEvent(this,fs->GetInputMatrix(),
				  T_EV_Input_MIDIProgramChange);
  app->getEMG()->ListenEvent(this,fs->GetInputMatrix(),
				  T_EV_Input_MIDIPitchBend);
}

void MidiIO::unlisten_events () {
  FloConfig *fs = app->getCFG();

  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMidiTuning);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMidiEchoPort);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMidiEchoChannel);
  app->getEMG()->UnlistenEvent(this,fs->GetInputMatrix(),
				    T_EV_Input_MIDIKey);
  app->getEMG()->UnlistenEvent(this,fs->GetInputMatrix(),
				    T_EV_Input_MIDIController);
  app->getEMG()->UnlistenEvent(this,fs->GetInputMatrix(),
				    T_EV_Input_MIDIProgramChange);
  app->getEMG()->UnlistenEvent(this,fs->GetInputMatrix(),
				    T_EV_Input_MIDIPitchBend);
}

void MidiIO::ReceiveNoteOffEvent (int channel, int notenum, int vel) {
  // Note Off
  MIDIKeyInputEvent *mevt = (MIDIKeyInputEvent *)
  Event::GetEventByType(T_EV_Input_MIDIKey);
  mevt->down = 0;
  mevt->channel = channel;
  mevt->notenum = notenum;
  if (mevt->notenum < 0 || mevt->notenum >= MAX_MIDI_NOTES) {
    printf("MIDI: Bad MIDI note #%d!\n",mevt->notenum);
    mevt->notenum = 0;
  }
  mevt->vel = vel;
  mevt->outport = note_def_port[mevt->notenum];
  if (app->getCFG()->IsDebugInfo())
    printf("MIDI: Note %d off, channel %d velocity %d\n",
	   mevt->notenum, mevt->channel, mevt->vel);
  app->getEMG()->BroadcastEventNow(mevt, this);
};

void MidiIO::ReceiveNoteOnEvent (int channel, int notenum, int vel) {
  MIDIKeyInputEvent *mevt = (MIDIKeyInputEvent *)
  Event::GetEventByType(T_EV_Input_MIDIKey);
  mevt->channel = channel;
  mevt->notenum = notenum;
  if (mevt->notenum < 0 || mevt->notenum >= MAX_MIDI_NOTES) {
    printf("MIDI: Bad MIDI note #%d!\n",mevt->notenum);
    mevt->notenum = 0;
  }
  mevt->vel = vel;
  if (vel > 0) {
    mevt->down = 1;
    note_def_port[mevt->notenum] = mevt->outport = echoport;
    note_patch[mevt->notenum] = curpatch;
  } else {
    mevt->down = 0;
    mevt->outport = note_def_port[mevt->notenum];
  }
  if (app->getCFG()->IsDebugInfo())
    printf("MIDI: Note %d %s, channel %d velocity %d\n",
	   mevt->notenum,
	   (mevt->down ? "on" : "off"),
	   mevt->channel, mevt->vel);
  app->getEMG()->BroadcastEventNow(mevt, this);
}

void MidiIO::ReceivePitchBendEvent (int channel, int value) {
  // Store incoming bender value
  curbender = value;
  // Perform tune
  value += bendertune;
  
  MIDIPitchBendInputEvent *mevt = (MIDIPitchBendInputEvent *)
    Event::GetEventByType(T_EV_Input_MIDIPitchBend);
  mevt->outport = echoport;
  mevt->channel = channel;
  mevt->val = value;
  if (app->getCFG()->IsDebugInfo())
    printf("MIDI: Pitchbend channel %d value %d\n",
	   mevt->channel,mevt->val);
  app->getEMG()->BroadcastEventNow(mevt, this);
}	  

void MidiIO::ReceiveProgramChangeEvent (int channel, int value) {
  MIDIProgramChangeInputEvent *mevt = (MIDIProgramChangeInputEvent *)
    Event::GetEventByType(T_EV_Input_MIDIProgramChange);
  mevt->outport = echoport;
  mevt->channel = channel;
  mevt->val = value;
  if (app->getCFG()->IsDebugInfo())
    printf("MIDI: Program change channel %d value %d\n",
	   mevt->channel,mevt->val);
  app->getEMG()->BroadcastEventNow(mevt, this);
}	  

void MidiIO::ReceiveControlChangeEvent (int channel, int ctrl, int value) {
  MIDIControllerInputEvent *mevt = (MIDIControllerInputEvent *)
    Event::GetEventByType(T_EV_Input_MIDIController);
  mevt->outport = echoport;
  mevt->channel = channel;
  mevt->ctrl = ctrl;
  mevt->val = value;
  if (app->getCFG()->IsDebugInfo())
    printf("MIDI: Controller %d channel %d value %d\n",mevt->ctrl,
	   mevt->channel,mevt->val);
  app->getEMG()->BroadcastEventNow(mevt, this);
}

MidiIO::~MidiIO() {
  delete[] note_def_port;
  delete[] note_patch;
  if (in_ports != 0)
    delete[] in_ports;
  if (out_ports != 0)
    delete[] out_ports;

#ifdef __MACOSX__
  if (out_sources != 0)
    delete[] out_sources;
#endif
}

void MidiIO::SetMIDIEcho (int def_port, PatchItem *patch) {
  if (CRITTERS)
    printf("MIDI: Set Echo Port: %d Patch: %s\n",def_port,
	   (patch != 0 ? patch->name : "(none)"));

  // Save default port
  char outportschanged = 0,
    usefluid = 0; // Enable FluidSynth?
  if (def_port >= 0 && def_port <= numouts) {
    if (def_port != echoport) {
      echoport = def_port;
      outportschanged = 1;
      if (echoport == 0)
	usefluid = 1;
    }
  } else
    printf("MIDI: Invalid port #%d (valid range 0-%d)\n",
	   def_port,numouts);

  if (patch != 0) {
    if (patch->IsCombi()) {
      // Store reference to patch, to get zones later
      if (patch != curpatch) {
	curpatch = patch;

	// Check if FluidSynth port is referenced-
	outportschanged = 1;
	usefluid = 0;
	for (int i = 0; i < patch->numzones && !usefluid; i++) {
	  CombiZone *z = patch->GetZone(i);
	  if ((!z->port_r && echoport == 0) || // Port 0 is FluidSynth
	      (z->port_r && z->port == 0))   
	    usefluid = 1;
	}

	if (CRITTERS)
	  printf("MIDI: COMBI PATCH!\n");
      }
    } else {
      // Easy, single channel patch
      curpatch = patch;
      echochan = patch->channel;
    }
  }

  if (outportschanged) { // If ports changed
    // Enable/Disable FluidSynth engine
    FluidSynthEnableEvent *fsevt = (FluidSynthEnableEvent *)
      Event::GetEventByType(T_EV_FluidSynthEnable);
    fsevt->enable = usefluid;
    app->getEMG()->BroadcastEvent(fsevt, this);
  }
}

void MidiIO::ReceiveEvent(Event *ev, EventProducer *from) {
  // Handle special events for MIDI
  switch (ev->GetType()) {
  case T_EV_SetMidiTuning :
    {
      SetMidiTuningEvent *tev = (SetMidiTuningEvent *) ev;
      
      // OK!
      if (CRITTERS)
	printf("MIDI: Received SetMidiTuning "
	       "(new tuning: %d)\n", tev->tuning);
      SetBenderTune(tev->tuning);
    }
    return;

#if 0
  case T_EV_SetMidiEchoPort :
    {
      SetMidiEchoPortEvent *pev = (SetMidiEchoPortEvent *) ev;
      
      // OK!
      if (CRITTERS)
	printf("MIDI: Received SetMidiEchoPort "
	       "(port #: %d)\n", pev->echoport);
      if (pev->echoport >= 0 && pev->echoport <= numouts)
	echoport = pev->echoport;
      else
	printf("MIDI: Invalid port #%d (valid range 0-%d)\n",
	       pev->echoport,numouts);
    }
    return;

  case T_EV_SetMidiEchoChannel :
    {
      SetMidiEchoChannelEvent *cev = (SetMidiEchoChannelEvent *) ev;
      
      // OK!
      if (CRITTERS)
	printf("MIDI: Received SetMidiEchoChannel "
	       "(channel #: %d)\n", cev->echochannel);
      echochan = cev->echochannel;
    }
    return;
#endif
    
  default:
    break;
  }

  // Unhandled event?-- echo back to MIDI outs
  EchoEvent(ev);
}

int MidiIO::EchoEventToPortChannel (Event *ev, int port, int channel) {
  FloConfig *fs = app->getCFG(); 

  // DEBUG
  // printf("ECHO EVENT -> Port: %d Channel: %d\n",port,channel);

  int ret = -1;
  switch (ev->GetType()) {
  case T_EV_Input_MIDIController :
    {
      MIDIControllerInputEvent *mcev = (MIDIControllerInputEvent *) ev;
      OutputController(ret = (port == -1 ? mcev->outport-1 : port),
		       (channel == -1 ? mcev->channel : channel),
		       mcev->ctrl,
		       mcev->val);
    }
    break;
    
  case T_EV_Input_MIDIProgramChange :
    {
      MIDIProgramChangeInputEvent *mpev = (MIDIProgramChangeInputEvent *) ev;
      OutputProgramChange(ret = (port == -1 ? mpev->outport-1 : port),
			  (channel == -1 ? mpev->channel : channel),
			  mpev->val);
    } 
    break;
    
  case T_EV_Input_MIDIPitchBend :
    {
      MIDIPitchBendInputEvent *mpev = (MIDIPitchBendInputEvent *) ev;
      OutputPitchBend(ret = (port == -1 ? mpev->outport-1 : port),
		      (channel == -1 ? mpev->channel : channel),
		      mpev->val);
    }
    break;
    
  case T_EV_Input_MIDIKey :
    {
      MIDIKeyInputEvent *mkev = (MIDIKeyInputEvent *) ev;      
      OutputNote(ret = (port == -1 ? mkev->outport-1 : port),
		 (channel == -1 ? mkev->channel : channel),
		 mkev->down,
		 mkev->notenum + fs->transpose,
		 mkev->vel);
      if (!mkev->down) // Double up on Note Offs
	OutputNote(ret = (port == -1 ? mkev->outport-1 : port),
		   (channel == -1 ? mkev->channel : channel),
		   mkev->down,
		   mkev->notenum + fs->transpose,
		   mkev->vel);
    }
    break;
    
  default:
    break;
  }

  return ret;
};

void MidiIO::EchoEvent (Event *ev) {
  // Handle MIDI event echos..

  if (!ev->echo) {
    // This event is generated from the configuration explicitly- so don't
    // send it through our current patch settings-- send to the exact port
    // and channel specified

    OutputStartOnPort();
    int port = EchoEventToPortChannel(ev,-1,-1);
    OutputEndOnPort(port);
  } else {
    // Echo of unused MIDI event- use patch logic
    
    // Handle special note off case- ensure note off sent to right place(s)
    PatchItem *ev_patch = 0;
    int ev_port = 0;
    if (ev->GetType() == T_EV_Input_MIDIKey &&
	!((MIDIKeyInputEvent *) ev)->down &&
	note_patch[((MIDIKeyInputEvent *) ev)->notenum] != 0) {
      // Use patch which was used when note was pressed
      ev_patch = note_patch[((MIDIKeyInputEvent *) ev)->notenum];
      ev_port = note_def_port[((MIDIKeyInputEvent *) ev)->notenum];
    } else {
      // Use current patch for all other events
      ev_patch = curpatch;
      ev_port = echoport;
    }
    
    // DEBUG
    // printf("MIDI: Echo port: %d Patch: %p\n",ev_port,ev_patch);
    
    if (ev_patch != 0 && ev_patch->IsCombi()) {
      // Combi patch- keyboard split into zones, possible multichannel output
      
      int curport = -1;
      for (int i = 0; i < ev_patch->numzones; i++) {
	CombiZone *z = ev_patch->GetZone(i);
	
	int notenum = -1;
	MIDIKeyInputEvent *note = 0;
	if (ev->GetType() == T_EV_Input_MIDIKey) {
	  note = (MIDIKeyInputEvent *) ev;
	  notenum = note->notenum;
	}
	
	if (notenum == -1 || (notenum >= z->kr_lo && notenum <= z->kr_hi)) {
	  int z_port = (z->port_r ? z->port : echoport);
	  if (curport == -1 || z_port != curport) {
	    // Starting with a new port
	    if (curport > 0 && curport <= numouts)
	      OutputEndOnPort(curport-1);
	    
	    if (z_port > 0 && z_port <= numouts)
	      OutputStartOnPort();
	    curport = z_port;
	  }
	  
	  if (curport > 0 && curport <= numouts)
	    EchoEventToPortChannel(ev,curport-1,z->channel);
	  else if (curport == 0) {
#if USE_FLUIDSYNTH
	    app->getFLUIDP()->ReceiveMIDIEvent(ev);
#endif
	  }
	}
      }
    } else {   
      if (ev_port > 0 && ev_port <= numouts) {
	// Single channel output- easy case
	OutputStartOnPort();
	EchoEventToPortChannel(ev,ev_port-1,
			       (echochan != -1 && ev_patch != 0 ? 
				ev_patch->channel : echochan));
	OutputEndOnPort(ev_port-1);
      } else if (ev_port == 0) {
#if USE_FLUIDSYNTH
	app->getFLUIDP()->ReceiveMIDIEvent(ev);
#endif
      }
    }
  }
};

void MidiIO::SendBankProgramChangeToPortChannel (int bank, int program, 
						 int port, int channel) {
  if (bank != -1) {
    OutputController(port,channel,MIDI_BANKCHANGE_MSB,bank / 128);
    OutputController(port,channel,MIDI_BANKCHANGE_MSB,bank % 128);
  }
  if (program != -1)
    OutputProgramChange(port,channel,program);
};

void MidiIO::SendBankProgramChange (PatchItem *patch) {
  printf("MIDI: Program Change\n");

  if (patch != 0) {
    if (patch->IsCombi()) {
      // Combi, send individual zone bank/patch changes
      for (int i = 0; i < patch->numzones; i++) {
	CombiZone *z = patch->GetZone(i);
	
	int z_port = (z->port_r ? z->port : echoport);
	if (z_port > 0 && z_port <= numouts)
	  SendBankProgramChangeToPortChannel(z->bank, z->prog,
					     z_port-1, z->channel);
      }
    } else {
      // Single patch, send bank/patch change
      if (echoport > 0 && echoport <= numouts) 
	SendBankProgramChangeToPortChannel(patch->bank, patch->prog,
					   echoport-1, patch->channel);
    }
  }  
};
