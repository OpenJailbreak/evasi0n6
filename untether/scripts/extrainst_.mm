/* Cydia Evasi0n - Powerful Code Insertion Platform
 * Copyright (C) 2011  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Evasi0n is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Evasi0n is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Evasi0n.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <string.h>
#include <stdint.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include "Cydia.hpp"
#include "LaunchDaemons.hpp"

extern "C" {
    CFTypeRef MGCopyAnswer(CFStringRef key, int unknown);
}

void hide_weather_on_ipads()
{
    struct utsname name;
    uname(&name);

    if(strncmp(name.machine, "iPad", sizeof("iPad") - 1) == 0)
    {
        NSMutableDictionary* info = [NSMutableDictionary dictionaryWithContentsOfFile:@"/Applications/Weather.app/Info.plist"];
        NSMutableArray* tags = [[[info objectForKey:@"SBAppTags"] mutableCopy] autorelease];
        if(!tags)
            tags = [NSMutableArray array];
        [tags addObject:@"hidden"];
        [info setObject:tags forKey:@"SBAppTags"];
        [info writeToFile:@"/Applications/Weather.app/Info.plist" atomically:YES];
    }
}

void disable_ota_updates()
{
    NSMutableDictionary* info = [NSMutableDictionary dictionaryWithContentsOfFile:@"/System/Library/LaunchDaemons/com.apple.mobile.softwareupdated.plist"];
    [info setObject:[NSNumber numberWithBool:YES] forKey:@"Disabled"];
    [info writeToFile:@"/System/Library/LaunchDaemons/com.apple.mobile.softwareupdated.plist" atomically:YES];

    info = [NSMutableDictionary dictionaryWithContentsOfFile:@"/System/Library/LaunchDaemons/com.apple.softwareupdateservicesd.plist"];
    [info setObject:[NSNumber numberWithBool:YES] forKey:@"Disabled"];
    [info writeToFile:@"/System/Library/LaunchDaemons/com.apple.softwareupdateservicesd.plist" atomically:YES];
}

int main(int argc, char *argv[]) {
    if (argc < 2 || (
        strcmp(argv[1], "install") != 0 &&
        strcmp(argv[1], "upgrade") != 0 &&
    true)) return 0;

    NSAutoreleasePool *pool([[NSAutoreleasePool alloc] init]);

    NSFileManager *manager([NSFileManager defaultManager]);
    NSError *error;

    if ([manager fileExistsAtPath:@ Evasi0nLaunchSocket_])
        [manager removeItemAtPath:@ Evasi0nLaunchSocket_ error:&error];

    NSString *config([NSString stringWithContentsOfFile:@ Evasi0nLaunchConfig_ encoding:NSNonLossyASCIIStringEncoding error:&error]);
    // XXX: if the file fails to load, it might not be missing: it might be unreadable for some reason
    if (config == nil)
        config = @"";

    NSArray *lines([config componentsSeparatedByString:@"\n"]);

    NSMutableArray *copy([NSMutableArray array]);
    for (NSString *line in lines)
        if ([line rangeOfString:@"evasi0n"].location == NSNotFound)
            [copy addObject:line];

    [copy removeObject:@""];

    [copy insertObject:@ Evasi0nLinkLaunch_ atIndex:0];
    [copy insertObject:@ Evasi0nUnlinkLaunch_ atIndex:0];
    [copy insertObject:@ Evasi0nUnsetEnv_ atIndex:0];
    [copy insertObject:@ Evasi0nLaunch_ atIndex:0];
    [copy insertObject:@ Evasi0nLoadAmfid_ atIndex:0];
    [copy insertObject:@ Evasi0nSetEnv_ atIndex:0];
    [copy insertObject:@ Evasi0nRemount_ atIndex:0];

    [copy addObject:@""];

    if (![copy isEqualToArray:lines])
        [[copy componentsJoinedByString:@"\n"] writeToFile:@ Evasi0nLaunchConfig_ atomically:YES encoding:NSNonLossyASCIIStringEncoding error:&error];

    CFStringRef udid = (CFStringRef) MGCopyAnswer(CFSTR("UniqueDeviceID"), 0);
    [(NSString *)udid writeToFile:@"/private/var/evasi0n/udid" atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    CFRelease(udid);

    hide_weather_on_ipads();
    disable_ota_updates();

    struct stat stat_buf;

    // Solve the case where the directory is not there at all after evasi0n.
    mkdir("/private/var/db/timezone", 0777);

    // Solve the case if someone created the directory but it has the wrong permissions.
    chmod("/private/var/db/timezone", 0777);

    // Solve the case if someone created the directory, but put localtime as a regular file instead of a symlink.
    if(lstat("/private/var/db/timezone/localtime", &stat_buf) == 0)
    {
        if((stat_buf.st_mode & S_IFMT) != S_IFLNK)
            unlink("/private/var/db/timezone/localtime");
    }

    [pool release];

    // XXX: in general, this return 0 happens way too often
    return 0;
}
