//
//  jailbreak.c
//  utility
//
//  Created by Yiduo Wang on 10/31/12.
//  Copyright (c) 2012 Yiduo Wang. All rights reserved.
//

#include <stdio.h>
#include "untar.h"
#include "jailbreakinstaller.h"

#define RB_AUTOBOOT 0
int reboot(int);

CFTypeRef MGCopyAnswer(CFStringRef key, int unknown);

int main(int argc, const char* argv[])
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    prepare_jailbreak_install();
    untar("/Developer/Cydia.tar", "/");
    untar("/Developer/packagelist.tar", "/");
    untar("/Developer/hack.tar", "/");
    uicache();

    CFStringRef udid = MGCopyAnswer(CFSTR("UniqueDeviceID"), 0);
    [(NSString *)udid writeToFile:@"/private/var/kiki/udid" atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    CFRelease(udid);

    reboot(RB_AUTOBOOT);
    [pool release];
}
