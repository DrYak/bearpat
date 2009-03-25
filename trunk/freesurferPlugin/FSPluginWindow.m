//
//  PluginWindow.m
//

/***************************************************************************
 *   Copyright (C) 2009 by Ivan Blagoev Topolsky   *
 *   ivan.topolsky@medecine.unige.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#import "FSPluginWindow.h"
#import "dicomFile.h"
#import "DCMPix.h"
#import "FSTask.h"

NSUserDefaults* fsPreferences = nil;

@implementation FSPluginWindow

@synthesize subjNames; // default getter & setter

// the viewerController property is used to hold which is the current 2D viewer.
@synthesize viewerController; // default reader
- (void) setViewerController: (ViewerController*) newcont {
	NSLog(@"setcont\n");
	NSLog(@"self: %@\n", self);
	if (! [newcont isKindOfClass:[ViewerController class]]) return;
	
	// reference counting : we hold a reference to the viewer, advertise it
	if (viewerController != nil) {
		[viewerController release];
	}
	viewerController =  newcont;
	if (viewerController != nil) {
		[viewerController retain];
		
		dcmPixList = [viewerController pixList];
	}
	NSLog(@"cont: %@\n", viewerController, dcmPixList);

	[self putName];
}


-(void) rescanNames {
	// free last copy
	if (subjNames != nil) {
		[subjNames release];
	}
	
	// get new list
	FSTask* freeSurfer = [[[FSTask alloc] initWithPath:[fsPreferences stringForKey:@"fsPath"] subjects:[fsPreferences stringForKey:@"subjPath"]] autorelease];
	subjNames = [freeSurfer subjList];	

	// update combo box
	[importname removeAllItems];
	[importname addItemsWithObjectValues:subjNames];
}



/*****************************
 *                           *
 *   GENERAL WINDOWS STUFF   *
 *                           *
 ****************************/


/*
 * Show window : additionnal stuff
 */
-(void) showWindow:(id)sender {
	[super showWindow:sender];
	
	// once the object exists we have to populate some of them with up to date data...
	
	// put a description
	[self putName];
	
	// load paths from fsPreferences
	[fsPath setURL:[NSURL fileURLWithPath:[fsPreferences stringForKey:@"fsPath"] isDirectory:YES]];
	[subjPath setURL:[NSURL fileURLWithPath:[fsPreferences stringForKey:@"subjPath"] isDirectory:YES]];
	
	// rescanNames
	[self rescanNames];
	
	return;
}


- (id) init {
	self = [super initWithWindowNibName:@"importator"];
	NSLog(@"init: %@\n", self);
	viewerController = nil;
	subjNames = nil;
	
//	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mainChange:) name:NSWindowDidBecomeMainNotification object:nil];

	// initialise fsPreferences
	if (fsPreferences == nil) {
		fsPreferences = [[NSUserDefaults standardUserDefaults] retain];
		NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
							  @"/Applications/freesurfer",	@"fsPath",
							  @"/Applications/freesurfer/subjects" ,	@"subjPath",
							  nil ]; // terminate the list
		[fsPreferences registerDefaults:dict];
	}
	
	return self;
}

- (void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];

	// references we were holding
	if (viewerController != nil) {
		[viewerController release];
	}
	
	[super dealloc];
}





/*******************
 *                 *
 *   IMPORT PANE   *
 *                 *
 *******************/



-(IBAction) import:(id)sender {
	// check if there are DICOMs currently selected
	if (dcmPixList == nil) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"No DICOMs to import"
										   defaultButton:@"Abort"
										 alternateButton:nil 
											 otherButton:nil
							
							   informativeTextWithFormat:@"Please select a series"	
							];
		[alert  beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:nil contextInfo:nil];
		return;
	}
	[self putName];
	
	NSString* name = [importname stringValue];

	// check if there's a subject name
	if ([name length] == 0) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"No Subject directory name given"
										   defaultButton:@"Abort"
										 alternateButton:nil 
											 otherButton:nil
							
							   informativeTextWithFormat:@"FreeSurfers stores each subject in a separate directory.\nPlease type a name in the 'Subject' field, or use the 'Suggest' button"	
							];
		[alert  beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:nil contextInfo:nil];
		return;
	}
	
	
	
	NSProgressIndicator *bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0,0,200,15)];
	[bar setUsesThreadedAnimation:TRUE];
	[bar setIndeterminate:FALSE];
	NSAlert *alert = [NSAlert alertWithMessageText:@"Importing DICOM"
									 defaultButton:@"Cancel"
								   alternateButton:nil 
									   otherButton:nil
					  
						 informativeTextWithFormat:@"Copying DICOMs into FreeSurfer subject"	
					  ];
	[alert setAlertStyle:NSInformationalAlertStyle];
	[alert setAccessoryView:bar];
	[alert  beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:nil contextInfo:nil];

	[bar setMinValue:0];
	[bar setMaxValue:[dcmPixList count]];
	[bar setDoubleValue:0.0];
	
	FSTask* freeSurfer = [[[FSTask alloc] initWithPath:[fsPreferences stringForKey:@"fsPath"] subjects:[fsPreferences stringForKey:@"subjPath"]] autorelease];

	[freeSurfer mkSubj:@"prout"];

/*
	for (DCMPix* slice in dcmPixList) {
		[bar setDoubleValue:[bar doubleValue] + 1.0];
		[bar display];
		NSLog(@"sli: %@ %dx%d %p\n", slice.srcFile, slice.pwidth, slice.pheight, freeSurfer);
		usleep(10000);
		
	}
*/
	[alert setAccessoryView:nil];
	[bar release];	
	[[[alert buttons] objectAtIndex:0] setTitle:@"Ok"];

	
	return;
}



/*
 * Writes a nice explanatory line
 * about the currently selected serie
 */
-(void) putName {
	if (dcmPixList == nil) return;
	
	DCMPix* dcmPix = [dcmPixList objectAtIndex:0];
	DicomFile* file = [[[DicomFile alloc] init:dcmPix.srcFile DICOMOnly:TRUE] autorelease];
	
	NSString* nfo = [NSString stringWithFormat:@"%@ %@ %@ %@ %@ %@x%@x%i", 
	 [file elementForKey:@"patientName"], 
	 [file elementForKey:@"patientID"],
	 [file elementForKey:@"modality"],
	 [file elementForKey:@"protocolName"],
	 [file elementForKey:@"studyDate"],
	 [file elementForKey:@"width"],
	 [file elementForKey:@"height"],
	 [dcmPixList count]];

	[importinfo setStringValue:nfo];
}


/*
 * This action is is called when pressing the "Suggest" button.
 * it makes a nice subject directory name out of the DICOM name field
 * and puts it into the "Subject name" field.
 */
-(IBAction) suggestName:(id)sender {
	if (dcmPixList == nil) return;
	
	DCMPix* dcmPix = [dcmPixList objectAtIndex:0];
	DicomFile* file = [[[DicomFile alloc] init:dcmPix.srcFile DICOMOnly:TRUE] autorelease];

	// lower case and without blank
	NSString* name = [[[file elementForKey:@"patientName"] stringByReplacingOccurrencesOfString:@" " withString:@"_"] lowercaseString];
	
	[importname setStringValue:name];
}





/************************
 *                      *
 *   PREFERENCES PANE   *
 *                      *
 ************************/
				

/*
 * Pick a path utility function
 */

-(void) pickPathName:(NSString*)label field:(NSPathControl*)path pref:(NSString*)key {
	int result;	
	NSOpenPanel *selector = [NSOpenPanel openPanel];
	
	// we want a direcory selector
	[selector setCanChooseDirectories:YES];
	[selector setCanCreateDirectories:NO]; // should not create a new directory, only pick FS
//	[selector setPrompt:label];
    [selector setTitle:@"Choose Directory"];
    [selector setMessage:label];
	[selector setCanChooseFiles:NO];
	[selector setAllowsMultipleSelection:NO];
	
	result = [selector runModalForDirectory:[[path URL] path] file:nil];
	
	
    if (result == NSOKButton) {
		// we only select 1 object
		NSString* strdir = [[selector filenames] objectAtIndex:0];
		NSURL* urldir = [[NSURL fileURLWithPath:strdir isDirectory:YES] autorelease];
		
		[path setURL:urldir];
		NSLog(@"pref- %@ %@ %@\n", fsPreferences, key, [fsPreferences stringForKey:key]);
		[fsPreferences setObject:strdir forKey:key];
		[fsPreferences synchronize];
		NSLog(@"pref- %@ %@ %@\n", fsPreferences, key, [fsPreferences stringForKey:key]);
    }
}


/*
 * User changed the FreeSurfer path
 *
 */
-(IBAction) fsPathPref:(id)sender {
	[self pickPathName:@"FreeSurfer installation folder" field:fsPath pref:@"fsPath"];
}
-(IBAction) subjPathPref:(id)sender{
	[self pickPathName:@"Subject folder" field:subjPath pref:@"subjPath"];
}


@end
