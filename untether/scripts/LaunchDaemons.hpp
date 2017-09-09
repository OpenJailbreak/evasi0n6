/* Cydia Evasi0n - Powerful Code Insertion Platform
 * Copyright (C) 2008-2011  Jay Freeman (saurik)
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

#ifndef SUBSTRATE_LAUNCHDAEMONS_HPP
#define SUBSTRATE_LAUNCHDAEMONS_HPP

#define Evasi0nLaunchDaemons_ "/System/Library/LaunchDaemons"
#define Evasi0nLaunchConfig_ "/etc/launchd.conf"
#define Evasi0nLaunchSocket_ "/var/evasi0n/sock"

#define Evasi0nRemount_ "bsexec .. /sbin/mount -u -o rw,suid,dev /"
#define Evasi0nSetEnv_ "setenv DYLD_INSERT_LIBRARIES /private/var/evasi0n/amfi.dylib"
#define Evasi0nLoadAmfid_ "load /System/Library/LaunchDaemons/com.apple.MobileFileIntegrity.plist"
#define Evasi0nLaunch_ "bsexec .. /private/var/evasi0n/evasi0n"
#define Evasi0nUnsetEnv_ "unsetenv DYLD_INSERT_LIBRARIES"
#define Evasi0nUnlinkLaunch_ "bsexec .. /bin/rm -f " Evasi0nLaunchSocket_
#define Evasi0nLinkLaunch_ "bsexec .. /bin/ln -f /var/tmp/launchd/sock " Evasi0nLaunchSocket_

#endif//SUBSTRATE_LAUNCHDAEMONS_HPP
