//
//  FSTask.m
//  FreeSurfer
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

#import "FSTask.h"
#include "dirent.h"


@implementation FSTask

@synthesize fsPath; // default setter
- (NSString*) fsPath {
	return (fsPath != nil)
			? fsPath
			: @"/Applications/freesurfer";
}

@synthesize subjPath; // default setter
- (NSString*) subjPath {
	return (subjPath != nil)
	? subjPath
	: [fsPath stringByAppendingString:@"/subjects"];
}


/*
 * Creates the task objects
 * @param path : Directory where FreeSurfer is installed
 * @param subj : Directory holding the subjects
 */
-(id)initWithPath:(NSString*)path subjects:(NSString*)subj {
	self = [self init];
	
	// get the paths
	fsPath = path;	 if (fsPath   != nil) [fsPath   retain];
	subjPath = subj; if (subjPath != nil) [subjPath retain];

	return self;
}

- (void) dealloc {
	if (fsPath   != nil) [fsPath   release];
	if (subjPath != nil) [subjPath release];
	
	[super dealloc];
}


/*
 * Get subject list
 */

-(NSArray*) subjList {
	NSMutableArray *lst = [[NSMutableArray array] autorelease];
	
	DIR* dir = opendir([[self subjPath] UTF8String]);
	struct dirent* ent = NULL;
	
	while ((ent = readdir(dir)) != NULL) 
		if (ent->d_type == DT_DIR && (ent->d_name[0] != '.')) {
			[lst addObject:[NSString stringWithFormat:@"%s", ent->d_name]];
		}	
	
	return [NSArray arrayWithArray:lst];
}


/*
 * Common helper code to prepare a task object, already filled with environment, etc...
 */
-(NSTask*) prepareTask:(NSString*)cmd {
	NSMutableDictionary* env = [NSMutableDictionary dictionaryWithDictionary:[[NSProcessInfo processInfo] environment]];
	[env setObject:fsPath forKey:@"FREESURFER_HOME"];
	[env setObject:subjPath	forKey:@"SUBJECTS_DIR"];

	NSLog(@"env%@\n", env);

	NSTask* task = [[NSTask alloc] init];
	[task setLaunchPath:@"/bin/bash"];
	[task setCurrentDirectoryPath: subjPath];
	[task setArguments:[NSArray arrayWithObjects:@"-c", [@". \"$FREESURFER_HOME/SetUpFreeSurfer.sh\" ;\n" stringByAppendingString:cmd], nil]];
	[task setEnvironment:env];
	return task;
}


/*
 * Create a new subjet
 */
-(void) mkSubj:(NSString*)name {
	NSString* cmd = [NSString stringWithFormat:@"mksubjdirs %@ ;\n", name];
	NSTask* task = [self prepareTask:cmd];
	NSLog(@"launch:>>>\n");
	[task launch];
}


@end
