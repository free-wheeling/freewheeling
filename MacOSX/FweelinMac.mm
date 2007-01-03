//
//  FweelinMac.mm
//  fweelin
//

#import <Cocoa/Cocoa.h>
#include "FweelinMac.h"
#include "SDLMain.h"

#include "fweelin_core.h"

void *FweelinMac::sdlmain = 0;
void *FweelinMac::fweelin = 0;

void FweelinMac::ClearMIDIInputList() {
	// printf("***SDLMAIN: %p\n",sdlmain);
	
	SDLMain *s = (SDLMain *)sdlmain;
	[s clearMIDIInputList];
};

void FweelinMac::AddMIDIInputSource(char *name) {
	SDLMain *s = (SDLMain *)sdlmain;
	[s addMIDIInputSource:[NSString stringWithUTF8String:name]];
};

void FweelinMac::SetMIDIInput(int idx) {
	// Connect MIDI 
	Fweelin *fw = (Fweelin *)fweelin;
	
	fw->getMIDI()->SetMIDIInput(idx);
};

void FweelinMac::SetDebugMode(int active) {
	Fweelin *fw = (Fweelin *)fweelin;

	ShowDebugInfoEvent *devt = (ShowDebugInfoEvent *) 
		Event::GetEventByType(T_EV_ShowDebugInfo);  
	devt->show = active;
	fw->getEMG()->BroadcastEventNow(devt, fw);
};

void FweelinMac::Quit() {
	Fweelin *fw = (Fweelin *)fweelin;

	ExitSessionEvent *evt = (ExitSessionEvent *) 
		Event::GetEventByType(T_EV_ExitSession);  
	fw->getEMG()->BroadcastEventNow(evt, fw);
};

void FweelinMac::ShowHelp() {
	Fweelin *fw = (Fweelin *)fweelin;
	
	VideoShowHelpEvent *evt = (VideoShowHelpEvent *) 
		Event::GetEventByType(T_EV_VideoShowHelp);  
	evt->page = 1;
	fw->getEMG()->BroadcastEventNow(evt, fw);
};

// Performs initialization on a new pthread to make it behave well with Cocoa
void FweelinMac::SetupCocoaThread() {
	// printf("MULTITHREADED: %d\n",[NSThread isMultiThreaded]);	
	autoreleasepool = [[NSAutoreleasePool alloc] init];
};

void FweelinMac::TakedownCocoaThread() {
	NSAutoreleasePool *tmp = (NSAutoreleasePool *) autoreleasepool;
	[tmp release];
};
