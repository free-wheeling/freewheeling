//
//  FweelinMac.h
//  fweelin
//

#ifndef __FWEELIN_MAC_H__
#define __FWEELIN_MAC_H__

// Interface class between Objective-C Mac OS X code and C++ Fweelin code
class FweelinMac {
public:
	
	// Performs setup/takedown on a new pthread to make it behave well with Cocoa
	void SetupCocoaThread();
	void TakedownCocoaThread();

	void *autoreleasepool;

	// MIDI input list
	static void ClearMIDIInputList();
	static void AddMIDIInputSource(char *name);
	static void SetMIDIInput(int idx);
	
	// Menu shortcuts to commands
	static void SetDebugMode(int active);
	static void Quit();
	static void ShowHelp();
	
	static void LinkSDLMain (void *m) { sdlmain = m; };
	static void *sdlmain;			// Pointer to instance of SDLMain

	static void LinkFweelin (void *fw) { fweelin = fw; };
	static void *fweelin;			// Pointer to instance of Fweelin
};

#endif