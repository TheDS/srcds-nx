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

#import "SPUCommandLineUserDriver.h"
#import <AppKit/AppKit.h>
#import <SparkleCore/SparkleCore.h>

#include "getch.h"

#define SCHEDULED_UPDATE_TIMER_THRESHOLD 2.0 // seconds

@interface SPUCommandLineUserDriver ()

@property (nonatomic, nullable, readonly) SUUpdatePermissionResponse *updatePermissionResponse;
@property (nonatomic, readonly) BOOL deferInstallation;
@property (nonatomic, readonly) BOOL verbose;
@property (nonatomic, readonly) SPUUserDriverCoreComponent *coreComponent;
@property (nonatomic) uint64_t bytesDownloaded;
@property (nonatomic) uint64_t bytesToDownload;
@property (nonatomic, readonly) NSString *currentVersion;

@end

@implementation SPUCommandLineUserDriver

@synthesize updatePermissionResponse = _updatePermissionResponse;
@synthesize deferInstallation = _deferInstallation;
@synthesize verbose = _verbose;
@synthesize coreComponent = _coreComponent;
@synthesize bytesDownloaded = _bytesDownloaded;
@synthesize bytesToDownload = _bytesToDownload;
@synthesize currentVersion = _currentVersion;

- (instancetype)initWithUpdatePermissionResponse:(nullable SUUpdatePermissionResponse *)updatePermissionResponse deferInstallation:(BOOL)deferInstallation verbose:(BOOL)verbose currentVersion:(nonnull NSString *)currentVersion
{
    self = [super init];
    if (self != nil) {
        _updatePermissionResponse = updatePermissionResponse;
        _deferInstallation = deferInstallation;
        _verbose = verbose;
        _coreComponent = [[SPUUserDriverCoreComponent alloc] init];
		_currentVersion = currentVersion;
    }
    return self;
}

- (void)showCanCheckForUpdates:(BOOL)canCheckForUpdates
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent showCanCheckForUpdates:canCheckForUpdates];
    });
}

- (void)showUpdatePermissionRequest:(SPUUpdatePermissionRequest *)__unused request reply:(void (^)(SUUpdatePermissionResponse *))reply
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.updatePermissionResponse == nil) {
            // We don't want to make this decision on behalf of the user.
            printf("Error: Asked to grant update permission. Exiting.\n");
            exit(EXIT_FAILURE);
        } else {
            if (self.verbose) {
                printf("Granting permission for automatic update checks with sending system profile %s...\n", self.updatePermissionResponse.sendSystemProfile ? "enabled" : "disabled");
            }
            reply(self.updatePermissionResponse);
        }
    });
}

- (void)showUserInitiatedUpdateCheckWithCompletion:(void (^)(SPUUserInitiatedCheckStatus))updateCheckStatusCompletion
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent registerUpdateCheckStatusHandler:updateCheckStatusCompletion];
        if (self.verbose) {
            printf("Checking for srcds updates...\n");
        }
    });
}

- (void)dismissUserInitiatedUpdateCheck
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent completeUpdateCheckStatus];
    });
}

- (void)displayReleaseNotes:(const char * _Nullable)releaseNotes
{
    if (releaseNotes != NULL) {
        printf("Release notes:\n");
        printf("%s\n", releaseNotes);
    }
	printf("Press any key to begin updating\n");
	getch();
}

- (void)displayHTMLReleaseNotes:(NSData *)releaseNotes
{
    // Note: this is the only API we rely on here that references AppKit
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithHTML:releaseNotes documentAttributes:nil];

    [self displayReleaseNotes:attributedString.string.UTF8String];
}

- (void)displayPlainTextReleaseNotes:(NSData *)releaseNotes encoding:(NSStringEncoding)encoding
{
    NSString *string = [[NSString alloc] initWithData:releaseNotes encoding:encoding];
    [self displayReleaseNotes:string.UTF8String];
}

- (void)showUpdateWithAppcastItem:(SUAppcastItem *)appcastItem updateAdjective:(NSString *)updateAdjective
{
    if (self.verbose) {
		printf("Installed version: %s\n", self.currentVersion.UTF8String);
		printf("Latest version: %s\n", appcastItem.displayVersionString.UTF8String);

        if (appcastItem.itemDescription != nil) {
            NSData *descriptionData = [appcastItem.itemDescription dataUsingEncoding:NSUTF8StringEncoding];
            if (descriptionData != nil) {
                [self displayHTMLReleaseNotes:descriptionData];
            }
		} else {
			printf("\nPress any key to begin updating");
			getch();
			printf("\n\n");
		}
    }
}

- (void)showUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem userInitiated:(BOOL)__unused userInitiated reply:(void (^)(SPUUpdateAlertChoice))reply
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self showUpdateWithAppcastItem:appcastItem updateAdjective:@"new"];
        reply(SPUInstallUpdateChoice);
    });
}

- (void)showDownloadedUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem userInitiated:(BOOL)__unused userInitiated reply:(void (^)(SPUUpdateAlertChoice))reply
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self showUpdateWithAppcastItem:appcastItem updateAdjective:@"downloaded"];
        reply(SPUInstallUpdateChoice);
    });
}

- (void)showResumableUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem userInitiated:(BOOL)__unused userInitiated reply:(void (^)(SPUInstallUpdateStatus))reply
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent registerInstallUpdateHandler:reply];
        [self showUpdateWithAppcastItem:appcastItem updateAdjective:@"resumable"];

        if (self.deferInstallation) {
            if (self.verbose) {
                printf("Deferring installation.\n");
            }
            [self.coreComponent installUpdateWithChoice:SPUDismissUpdateInstallation];
        } else {
            [self.coreComponent installUpdateWithChoice:SPUInstallAndRelaunchUpdateNow];
        }
    });
}

- (void)showInformationalUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem userInitiated:(BOOL)__unused userInitiated reply:(void (^)(SPUInformationalUpdateAlertChoice))reply
{
    dispatch_async(dispatch_get_main_queue(), ^{
        printf("Found information for new update: %s\n", appcastItem.infoURL.absoluteString.UTF8String);

        reply(SPUDismissInformationalNoticeChoice);
    });
}

- (void)showUpdateReleaseNotesWithDownloadData:(SPUDownloadData *)downloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            if (downloadData.MIMEType != nil && [downloadData.MIMEType isEqualToString:@"text/plain"]) {
                NSStringEncoding encoding;
                if (downloadData.textEncodingName == nil) {
                    encoding = NSUTF8StringEncoding;
                } else {
                    CFStringEncoding cfEncoding = CFStringConvertIANACharSetNameToEncoding((CFStringRef)downloadData.textEncodingName);
                    if (cfEncoding != kCFStringEncodingInvalidId) {
                        encoding = CFStringConvertEncodingToNSStringEncoding(cfEncoding);
                    } else {
                        encoding = NSUTF8StringEncoding;
                    }
                }
                [self displayPlainTextReleaseNotes:downloadData.data encoding:encoding];
            } else {
                [self displayHTMLReleaseNotes:downloadData.data];
            }
        }
    });
}

- (void)showUpdateReleaseNotesFailedToDownloadWithError:(NSError *)error
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("Error: Unable to download release notes: %s\n", error.localizedDescription.UTF8String);
        }
    });
}

- (void)showUpdateNotFoundWithAcknowledgement:(void (^)(void))__unused acknowledgement
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("No new update available!\n");
        }
        exit(EXIT_SUCCESS);
    });
}

- (void)showUpdaterError:(NSError *)error acknowledgement:(void (^)(void))__unused acknowledgement
{
    dispatch_async(dispatch_get_main_queue(), ^{
        printf("Error: Update has failed: %s\n", error.localizedDescription.UTF8String);
        exit(EXIT_FAILURE);
    });
}

- (void)showDownloadInitiatedWithCompletion:(void (^)(SPUDownloadUpdateStatus))downloadUpdateStatusCompletion
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent registerDownloadStatusHandler:downloadUpdateStatusCompletion];

        if (self.verbose) {
            printf("Downloading update...\n");
        }
    });
}

- (void)showDownloadDidReceiveExpectedContentLength:(uint64_t)expectedContentLength
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.bytesDownloaded = 0;
        self.bytesToDownload = expectedContentLength;
    });
}

- (void)showDownloadDidReceiveDataOfLength:(uint64_t)length
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.bytesDownloaded += length;

        // In case our expected content length was incorrect
        if (self.bytesDownloaded > self.bytesToDownload) {
            self.bytesToDownload = self.bytesDownloaded;
        }

        if (self.bytesToDownload > 0) {
			printf("Downloaded %llu out of %llu bytes (%.0f%%)\n", self.bytesDownloaded,
				   self.bytesToDownload, (self.bytesDownloaded * 100.0 / self.bytesToDownload));
		}
    });
}

- (void)showDownloadDidStartExtractingUpdate
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent completeDownloadStatus];
    });
}

- (void)showExtractionReceivedProgress:(double)progress
{
    dispatch_async(dispatch_get_main_queue(), ^{
		printf("Extracting update (%.0f%%)\n", progress * 100);
    });
}

- (void)showReadyToInstallAndRelaunch:(void (^)(SPUInstallUpdateStatus))installUpdateHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent registerInstallUpdateHandler:installUpdateHandler];

        if (self.deferInstallation) {
            if (self.verbose) {
                printf("Deferring installation.\n");
            }
            [self.coreComponent installUpdateWithChoice:SPUDismissUpdateInstallation];
        } else {
            [self.coreComponent installUpdateWithChoice:SPUInstallAndRelaunchUpdateNow];
        }
    });
}

- (void)showInstallingUpdate
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.verbose) {
            printf("Installing update...\n");
        }
    });
}

- (void)showUpdateInstallationDidFinishWithAcknowledgement:(void (^)(void))acknowledgement
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.coreComponent registerAcknowledgement:acknowledgement];

        if (self.verbose) {
           printf("Installation finished.\n");
        }

        [self.coreComponent acceptAcknowledgement];
    });
}

- (void)dismissUpdateInstallation
{
    dispatch_async(dispatch_get_main_queue(), ^{
        exit(EXIT_SUCCESS);
    });
}

- (void)showSendingTerminationSignal
{
    // We are already showing that the update is installing, so there is no need to do anything here
}

@end
