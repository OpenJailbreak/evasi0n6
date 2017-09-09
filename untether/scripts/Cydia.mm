/* Cydia Substrate - Powerful Code Insertion Platform
 * Copyright (C) 2008-2011  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Substrate is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Substrate is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Substrate.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <Foundation/Foundation.h>

#include <mach/mach_init.h>
#include <mach/vm_map.h>

#include <stdio.h>

#include "Cydia.hpp"

void FinishCydia(const char *finish) {
    if (finish == NULL)
        return;

    const char *cydia(getenv("CYDIA"));
    if (cydia == NULL)
        return;


    // XXX: I think I'd like to rewrite this code using C++
    int fd([[[[NSString stringWithUTF8String:cydia] componentsSeparatedByString:@" "] objectAtIndex:0] intValue]);

    FILE *fout(fdopen(fd, "w"));
    fprintf(fout, "finish:%s\n", finish);
    fclose(fout);
}
