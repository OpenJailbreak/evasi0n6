/* Cydia Corona - Powerful Code Insertion Platform
 * Copyright (C) 2011  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Corona is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Corona is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Corona.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <Foundation/Foundation.h>

#include <stdlib.h>

#include "LaunchDaemons.hpp"

int main(int argc, char *argv[]) {
    if (argc < 2 || (
        strcmp(argv[1], "abort-install") != 0 &&
        strcmp(argv[1], "remove") != 0 &&
    true)) return 0;

    NSAutoreleasePool *pool([[NSAutoreleasePool alloc] init]);

    NSFileManager *manager([NSFileManager defaultManager]);
    NSError *error;

    if (NSString *config = [NSString stringWithContentsOfFile:@ Evasi0nLaunchConfig_ encoding:NSNonLossyASCIIStringEncoding error:&error]) {
        NSArray *lines([config componentsSeparatedByString:@"\n"]);

        NSMutableArray *copy([NSMutableArray array]);
        for (NSString *line in lines)
            if ([line rangeOfString:@"evasi0n"].location == NSNotFound)
                [copy addObject:line];

        [copy removeObject:@""];

        [copy removeObject:@ Evasi0nRemount_];
        [copy removeObject:@ Evasi0nSetEnv_];
        [copy removeObject:@ Evasi0nLoadAmfid_];
        [copy removeObject:@ Evasi0nLaunch_];
        [copy removeObject:@ Evasi0nUnsetEnv_];
        [copy removeObject:@ Evasi0nUnlinkLaunch_];
        [copy removeObject:@ Evasi0nLinkLaunch_];

        if ([copy count] == 0)
            [manager removeItemAtPath:@ Evasi0nLaunchConfig_ error:&error];
        else {
            [copy addObject:@""];

            if (![copy isEqualToArray:lines])
                [[copy componentsJoinedByString:@"\n"] writeToFile:@ Evasi0nLaunchConfig_ atomically:YES encoding:NSNonLossyASCIIStringEncoding error:&error];
        }
    }

    [pool release];
    return 0;
}
