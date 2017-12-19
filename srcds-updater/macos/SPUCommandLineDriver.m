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

// Code originally created by Mayur Pawashe (Copyright © 2016) from the Sparkle project

#import "SPUCommandLineDriver.h"
#import <SparkleCore/SparkleCore.h>
#import "SPUCommandLineUserDriver.h"

@interface SPUCommandLineDriver () <SPUUpdaterDelegate>

@property (nonatomic, readonly) SPUUpdater *updater;
@property (nonatomic, readonly) BOOL verbose;
@property (nonatomic) BOOL probingForUpdates;
@property (nonatomic, readonly) BOOL interactive;
@property (nonatomic, copy, readonly, nullable) NSString *customFeedURL;

@end

@implementation SPUCommandLineDriver

@synthesize updater = _updater;
@synthesize verbose = _verbose;
@synthesize probingForUpdates = _probingForUpdates;
@synthesize interactive = _interactive;
@synthesize customFeedURL = _customFeedURL;

- (instancetype)initWithUpdateBundlePath:(NSString *)updateBundlePath applicationBundlePath:(nullable NSString *)applicationBundlePath customFeedURL:(nullable NSString *)customFeedURL updatePermissionResponse:(nullable SUUpdatePermissionResponse *)updatePermissionResponse deferInstallation:(BOOL)deferInstallation interactiveInstallation:(BOOL)interactiveInstallation verbose:(BOOL)verbose
{
    self = [super init];
    if (self != nil) {
        NSBundle *updateBundle = [NSBundle bundleWithPath:updateBundlePath];
        if (updateBundle == nil) {
            return nil;
        }

        NSBundle *applicationBundle = nil;
        if (applicationBundlePath == nil) {
            applicationBundle = updateBundle;
        } else {
            applicationBundle = [NSBundle bundleWithPath:(NSString * _Nonnull)applicationBundlePath];
            if (applicationBundle == nil) {
                return nil;
            }
        }

        NSString *currentVersion = [[applicationBundle infoDictionary] objectForKey:@"CFBundleShortVersionString"];

        _verbose = verbose;
        _interactive = interactiveInstallation;
        _customFeedURL = [customFeedURL copy];

        id <SPUUserDriver> userDriver = [[SPUCommandLineUserDriver alloc] initWithUpdatePermissionResponse:updatePermissionResponse deferInstallation:deferInstallation verbose:verbose currentVersion:currentVersion];
        _updater = [[SPUUpdater alloc] initWithHostBundle:updateBundle applicationBundle:applicationBundle userDriver:userDriver delegate:self];
    }
    return self;
}

// Because the user driver dispatches to the main queue asynchronously, we should do so here too
// to preserve the order of handled events

- (void)updater:(SPUUpdater *)__unused updater willScheduleUpdateCheckAfterDelay:(NSTimeInterval)delay
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("Last update check occurred too soon. Try again after %0.0f second(s).", delay);
        }
        exit(EXIT_SUCCESS);
    });
}

- (void)updaterWillIdleSchedulingUpdates:(SPUUpdater *)__unused updater
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("Automatic update checks are disabled. Exiting.\n");
        }
        exit(EXIT_SUCCESS);
    });
}

// If the installation is interactive, we can show an authorization prompt for requesting additional privileges,
// along with allowing the installer to show UI when installing
- (BOOL)updater:(SPUUpdater *)__unused updater shouldAllowInstallerInteractionForScheduledChecks:(SPUUpdateCheck)updateCheck
{
    switch (updateCheck) {
        case SPUUpdateCheckUserInitiated:
        case SPUUpdateCheckBackgroundScheduled:
            return self.interactive;
    }
}

- (nullable NSString *)feedURLStringForUpdater:(SPUUpdater *)__unused updater
{
    return self.customFeedURL;
}

// In case we find an update during probing, otherwise we leave this to the user driver
- (void)updater:(SPUUpdater *)__unused updater didFindValidUpdate:(SUAppcastItem *)__unused item
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.probingForUpdates) {
            if (self.verbose) {
                printf("Update available!\n");
            }
            exit(EXIT_SUCCESS);
        }
    });
}

- (void)updaterDidNotFindUpdate:(SPUUpdater *)__unused updater
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("No update available!\n");
        }
        exit(EXIT_SUCCESS);
    });
}

- (void)updater:(SPUUpdater *)__unused updater didAbortWithError:(NSError *)error
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("Aborted update with error (%ld): %s\n", (long)error.code, error.localizedDescription.UTF8String);
        }
        exit(EXIT_FAILURE);
    });
}

- (BOOL)updaterShouldDownloadReleaseNotes:(SPUUpdater *)__unused updater
{
    return self.verbose;
}

- (void)startUpdater
{
    NSError *updaterError = nil;
    if (![self.updater startUpdater:&updaterError]) {
        printf("Error: Failed to initialize updater with error (%ld): %s\n", updaterError.code, updaterError.localizedDescription.UTF8String);
        exit(EXIT_FAILURE);
    }
}

- (void)runAndCheckForUpdatesNow:(BOOL)checkForUpdatesNow
{
    if (checkForUpdatesNow) {
        // When we start the updater, this scheduled check will start afterwards too
        [self.updater checkForUpdates];
    }

    [self startUpdater];
}

- (void)probeForUpdates
{
    // When we start the updater, this info check will start afterwards too
    self.probingForUpdates = YES;
    [self.updater checkForUpdateInformation];
    [self startUpdater];
}

@end
