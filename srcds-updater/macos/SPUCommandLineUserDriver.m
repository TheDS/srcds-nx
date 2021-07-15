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
#import <Sparkle/Sparkle.h>

#include "getch.h"

#define SCHEDULED_UPDATE_TIMER_THRESHOLD 2.0 // seconds

@interface SPUCommandLineUserDriver ()

@property (nonatomic, nullable, readonly) SUUpdatePermissionResponse *updatePermissionResponse;
@property (nonatomic, readonly) BOOL deferInstallation;
@property (nonatomic, readonly) BOOL verbose;
@property (nonatomic) uint64_t bytesDownloaded;
@property (nonatomic) uint64_t bytesToDownload;
@property (nonatomic, readonly) NSString *currentVersion;
@property (nonatomic) BOOL externalReleaseNotes;

@end

@implementation SPUCommandLineUserDriver

@synthesize updatePermissionResponse = _updatePermissionResponse;
@synthesize deferInstallation = _deferInstallation;
@synthesize verbose = _verbose;
@synthesize bytesDownloaded = _bytesDownloaded;
@synthesize bytesToDownload = _bytesToDownload;
@synthesize currentVersion = _currentVersion;
@synthesize externalReleaseNotes = _externalReleaseNotes;

- (instancetype)initWithUpdatePermissionResponse:(nullable SUUpdatePermissionResponse *)updatePermissionResponse deferInstallation:(BOOL)deferInstallation verbose:(BOOL)verbose currentVersion:(nonnull NSString *)currentVersion
{
    self = [super init];
    if (self != nil) {
        _updatePermissionResponse = updatePermissionResponse;
        _deferInstallation = deferInstallation;
        _verbose = verbose;
        _currentVersion = currentVersion;
        _externalReleaseNotes = NO;
    }
    return self;
}

- (void)showUpdatePermissionRequest:(SPUUpdatePermissionRequest *)__unused request reply:(void (^)(SUUpdatePermissionResponse *))reply
{
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
}

- (void)showUserInitiatedUpdateCheckWithCancellation:(void (^)(void))__unused cancellation
{
    if (self.verbose) {
        printf("Checking for srcds updates...\n");
    }
}

- (void)displayReleaseNotes:(const char * _Nullable)releaseNotes
{
    if (releaseNotes != NULL) {
        printf("Release notes:\n");
        printf("%s\n", releaseNotes);
    }

    if (self.externalReleaseNotes) {
        printf("Press any key to begin updating\n");
        getch();
        printf("Downloading update...\n");
    }
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
        } else if (appcastItem.releaseNotesURL != nil) {
            self.externalReleaseNotes = YES;
        }
    }

    if (!self.externalReleaseNotes) {
        printf("Press any key to begin updating\n");
        getch();
    }
}

- (void)showUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem state:(SPUUserUpdateState *)state reply:(void (^)(SPUUserUpdateChoice))reply
{
    if (appcastItem.informationOnlyUpdate) {
        printf("Found information for new update: %s\n", appcastItem.infoURL.absoluteString.UTF8String);
        reply(SPUUserUpdateChoiceDismiss);
    } else {
        switch (state.stage) {
            case SPUUserUpdateStageNotDownloaded:
                [self showUpdateWithAppcastItem:appcastItem updateAdjective:@"new"];
                reply(SPUUserUpdateChoiceInstall);
                break;
            case SPUUserUpdateStageDownloaded:
                [self showUpdateWithAppcastItem:appcastItem updateAdjective:@"downloaded"];
                reply(SPUUserUpdateChoiceInstall);
                break;
            case SPUUserUpdateStageInstalling:

                if (self.deferInstallation) {
                    if (self.verbose) {
                        printf("Deferring installation.\n");
                    }
                    reply(SPUUserUpdateChoiceDismiss);
                } else {
                    reply(SPUUserUpdateChoiceInstall);
                }
                break;
        }
    }
}

- (void)showUpdateInFocus
{
}

- (void)showUpdateReleaseNotesWithDownloadData:(SPUDownloadData *)downloadData
{
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
}

- (void)showUpdateReleaseNotesFailedToDownloadWithError:(NSError *)error
{
    if (self.verbose) {
        printf("Error: Unable to download release notes: %s\n", error.localizedDescription.UTF8String);
    }
}

- (void)showUpdateNotFoundWithError:(NSError *)error acknowledgement:(void (^)(void))__unused acknowledgement __attribute__((noreturn))
{
    if (self.verbose) {
        printf("No new update available!\n");
    }
    exit(EXIT_SUCCESS);
}

- (void)showUpdaterError:(NSError *)error acknowledgement:(void (^)(void))__unused acknowledgement __attribute__((noreturn))
{
    printf("Error: Update has failed: %s\n", error.localizedDescription.UTF8String);
    printf("For more information, check Console.app for logs related to Sparkle.\n");
    exit(EXIT_FAILURE);
}

- (void)showDownloadInitiatedWithCancellation:(void (^)(void))__unused cancellation
{
    if (self.verbose) {
        if (self.externalReleaseNotes) {
            printf("Downloading release notes...\n");
        } else {
            printf("Downloading update...\n");
        }
    }
}

- (void)showDownloadDidReceiveExpectedContentLength:(uint64_t)expectedContentLength
{
    self.bytesDownloaded = 0;
    self.bytesToDownload = expectedContentLength;
}

- (void)showDownloadDidReceiveDataOfLength:(uint64_t)length
{
    self.bytesDownloaded += length;

    // In case our expected content length was incorrect
    if (self.bytesDownloaded > self.bytesToDownload) {
        self.bytesToDownload = self.bytesDownloaded;
    }

    if (self.bytesToDownload > 0 && self.verbose) {
        printf("Downloaded %llu out of %llu bytes (%.0f%%)\n", self.bytesDownloaded,
               self.bytesToDownload, (self.bytesDownloaded * 100.0 / self.bytesToDownload));
    }
}

- (void)showDownloadDidStartExtractingUpdate
{

}

- (void)showExtractionReceivedProgress:(double)progress
{
    printf("Extracting update (%.0f%%)\n", progress * 100);
}

- (void)showReadyToInstallAndRelaunch:(void (^)(SPUUserUpdateChoice))installUpdateHandler
{
    if (self.deferInstallation) {
        if (self.verbose) {
            printf("Deferring installation.\n");
        }
        installUpdateHandler(SPUUserUpdateChoiceDismiss);
    } else {
        installUpdateHandler(SPUUserUpdateChoiceInstall);
    }
}

- (void)showInstallingUpdate
{
    if (self.verbose) {
        printf("Installing update...\n");
    }
}

- (void)showUpdateInstalledAndRelaunched:(BOOL)__unused relaunched acknowledgement:(void (^)(void))acknowledgement
{
    if (self.verbose) {
        printf("Installation finished.\n");
    }

    acknowledgement();
}

- (void)dismissUpdateInstallation __attribute__((noreturn))
{
    exit(EXIT_SUCCESS);
}

- (void)showSendingTerminationSignal
{
    // We are already showing that the update is installing, so there is no need to do anything here
}

@end
