#ifndef __FWEELIN_MIDIIO_H
#define __FWEELIN_MIDIIO_H

#ifdef __MACOSX__
#include <CoreMIDI/MIDIServices.h>
// Small MIDI output buffer- must be large enough to hold any one MIDI message
#define OBUF_LEN 128	
#else
#include <alsa/asoundlib.h>
#endif

#include "fweelin_event.h"

class Fweelin;
class PatchItem;

class MidiIO : public EventProducer, public EventListener {
public:
  MidiIO (Fweelin *app);
  virtual ~MidiIO ();

  void ReceiveEvent(Event *ev, EventProducer *from);

  int activate ();
  void close ();

  void SetBenderTune(int tuning) { bendertune = tuning; };
  int GetBenderTune() { return bendertune; };
  void ResetBenderTune() { bendertune = 0; };

  // Sets MIDI echo to one or more ports and channels based on:
  // a default MIDI output port (def_port)
  // and the given patch. 
  //
  // Regular patches use the default MIDI output port and one channel, given
  // within the patch. 
  // 
  // Combi patches split the keyboard into multiple zones. 
  // Each zone can output to any channel, and can optionally override and send
  // to a different MIDI output port altogether. This allows one MIDI
  // controller to control many instruments, even on different softsynths,
  // at the same time. 
  
  // Zones can overlap, so that one key sends to multiple channels/ports
  void SetMIDIEcho (int def_port, PatchItem *patch);

  // Send bank and program change messages where appropriate for the given
  // patch
  void SendBankProgramChange (PatchItem *patch);

#ifndef __MACOSX__
  char IsActive () { return midithreadgo; };  
#else
  // Switch MIDI inputs (which system MIDI input source feeds Fweelin?)
  void SetMIDIInput (int idx);
#endif
  
  // Value to offset bender amounts by-- used to do tuning from
  // bender
  int bendertune;
  // Current bender value (before tuning adjustment)
  int curbender;

  // MIDI ECHO

  // Single channel echo-

  // Echo MIDI events out which MIDI port?
  // (0 is off, port #s ascending from 1 reference out_ports)
  int echoport;
  // Echo out which MIDI channel?
  // (-1 echoes without changing channel information)
  int echochan;

  PatchItem *curpatch; // Current patch- gives which channel(s) 
                       // to map to

  // Number of MIDI in/out ports
  int numins, numouts;

protected:
  // Core app
  Fweelin *app;

  // Open MIDI engine with num_in writeable ports and num_out readable ports.
  // Nonzero is returned on error
  char open_midi (int num_in, int num_out);
  
  // Listen/unlisten to events (used in startup & shutdown)
  void listen_events ();
  void unlisten_events ();
  
  // Methods that send one MIDI message to one system MIDI out
  void OutputController (int port, int chan, int ctrl, int val);
  void OutputProgramChange (int port, int chan, int val);
  void OutputPitchBend (int port, int chan, int val);
  void OutputNote (int port, int chan, char down, int notenum, int vel);
  void OutputStartOnPort ();       // First message for this port in this pass
  void OutputEndOnPort (int port); // Last message for this port in this pass

  // Echo MIDI event to a single port and channel
  // Return the port echoed to
  int EchoEventToPortChannel (Event *ev, int port, int channel);

  // Echo MIDI event back to MIDI outs, according to current patch settings
  void EchoEvent (Event *ev);

  // Receive incoming MIDI events from the system and broadcast FreeWheeling
  // MIDI events internally
  void ReceiveNoteOffEvent (int channel, int notenum, int vel);
  void ReceiveNoteOnEvent (int channel, int notenum, int vel);
  void ReceivePitchBendEvent (int channel, int value);
  void ReceiveProgramChangeEvent (int channel, int value);
  void ReceiveControlChangeEvent (int channel, int ctrl, int value);

  // Send bank and program change message to port/channel
  void SendBankProgramChangeToPortChannel (int bank, int program, 
					   int port, int channel);
 
#ifdef __MACOSX__
  MIDIClientRef client;
  MIDIPortRef *in_ports, *out_ports;
  MIDIEndpointRef *out_sources;
  MIDIEndpointRef dest;	// Deprecated
  int inputidx;		// Index of last selected MIDI input source
  
  MIDIPacket *curPacket;	
  MIDIPacketList *packetList;  
  Byte obuf[OBUF_LEN];	// Small MIDI event output buffer

  void static MidiInputProc (const MIDIPacketList *pktlist, void *refCon, void *connRefCon);  
#else
  // Midi event handler thread
  static void *run_midi_thread (void *ptr);

  snd_seq_t *seq_handle;
  int *in_ports, *out_ports;

  pthread_t midi_thread;
  char midithreadgo;
#endif
  
  // For each MIDI note on the scale, what default port and patch was the note
  // played with? This allows us to send note off(s) to the right place(s), 
  // even when the patch is changed while notes are held
  int *note_def_port;
  PatchItem **note_patch;
};

#endif
