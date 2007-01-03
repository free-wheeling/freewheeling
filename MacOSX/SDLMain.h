/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#import <Cocoa/Cocoa.h>

@interface SDLMain : NSObject
{
	IBOutlet NSPopUpButton *midiInputSel;
}
- (IBAction)setMIDIInput:(id)sender;
- (IBAction)toggleDebugMode:(id)sender;
- (IBAction)quitFW:(id)sender;
- (IBAction)showHelp:(id)sender;

- (void)clearMIDIInputList;
- (void)addMIDIInputSource:(NSString *)name;
@end
