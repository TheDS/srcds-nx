/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NX - Updater
 * Copyright (C) 2011-2017 Scott Ehlert and AlliedModders LLC.
 * All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "Steamworks SDK," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.
 */

#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>
#import "SPUCommandLineDriver.h"

int main(int argc, const char **argv) {
	@autoreleasepool {
		// If an update's release notes are embedded in the Sparkle appcast file or there are
		// external release notes which use HTML, then the use of NSAttributedString to parse that
		// somehow causes a dock icon to be created as long as the program is located in
		// <bundle>/Contents/MacOS. Moving it outside of Contents/MacOS has other drawbacks
		// however. For now, force this to be background process in order to remove the dock icon.
		// TODO: Find an alternative way of dealing with this.
		ProcessSerialNumber psn = { 0, kCurrentProcess };
		TransformProcessType(&psn, kProcessTransformToBackgroundApplication);

		NSString *bundlePath = [[NSBundle mainBundle] bundlePath];

		SPUCommandLineDriver *driver = [[SPUCommandLineDriver alloc]
										initWithUpdateBundlePath:bundlePath
										applicationBundlePath:@"/"
										allowedChannels:[NSSet set] customFeedURL:nil
										updatePermissionResponse:nil deferInstallation:NO
										interactiveInstallation:YES allowMajorUpgrades:YES
										verbose:YES];

		[driver runAndCheckForUpdatesNow:YES];
		[[NSRunLoop currentRunLoop] run];
	}

	return 0;
}
