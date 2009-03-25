//
//  FSPluginWindow.h
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

#import <Cocoa/Cocoa.h>
#import "ViewerController.h"


@interface FSPluginWindow : NSWindowController {
	IBOutlet NSTextField*  importinfo;
	IBOutlet NSComboBox*  importname;
	
	IBOutlet NSPathControl* fsPath;
	IBOutlet NSPathControl* subjPath;

	ViewerController* viewerController;
	NSArray* dcmPixList; // array of DCMPix object

	
	NSArray* subjNames;
}

@property(readwrite,assign) ViewerController* viewerController;
@property(readwrite,assign) NSArray* subjNames;


-(void) rescanNames;

-(IBAction) import:(id)sender;
-(IBAction) suggestName:(id)sender;


-(void) pickPathName:(NSString*)label field:(NSPathControl*) path pref:(NSString*)key;
-(IBAction) fsPathPref:(id)sender;
-(IBAction) subjPathPref:(id)sender;

-(void) putName;

@end
