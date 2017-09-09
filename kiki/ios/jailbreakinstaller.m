//
//  jailbreakinstaller.c
//  utility
//
//  Created by Yiduo Wang on 10/31/12.
//  Copyright (c) 2012 Yiduo Wang. All rights reserved.
//

#include <dlfcn.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>
#import <Foundation/Foundation.h>
#include <sys/utsname.h>

char*** _NSGetEnviron();

int run(char **argv, char **envp) {
    if(envp == NULL) envp = *(_NSGetEnviron());
    fprintf(stderr, "run:");
    for(char **p = argv; *p; p++) {
        fprintf(stderr, " %s", *p);
    }
    fprintf(stderr, "\n");

    pid_t pid;
    int stat;
    if(posix_spawn(&pid, argv[0], NULL, NULL, argv, envp)) return 255;
    if(pid != waitpid(pid, &stat, 0)) return 254;
    if(!WIFEXITED(stat)) return 253;
    return WEXITSTATUS(stat);
}


// returns whether the plist existed
static bool modify_plist(NSString *filename, void (^func)(id)) {
    NSData *data = [NSData dataWithContentsOfFile:filename];
    if(!data) {
        return false;
    }
    NSPropertyListFormat format;
    NSError *error;
    id plist = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListMutableContainersAndLeaves format:&format error:&error];

    func(plist);

    NSData *new_data = [NSPropertyListSerialization dataWithPropertyList:plist format:format options:0 error:&error];

    [new_data writeToFile:filename atomically:YES];

    return true;
}

static void dok48() {
    char model[32];
    size_t model_size = sizeof(model);
    sysctlbyname("hw.model", model, &model_size, NULL, 0);

    NSString *filename = [NSString stringWithFormat:@"/System/Library/CoreServices/SpringBoard.app/%s.plist", model];
    modify_plist(filename, ^(id plist) {
        [[plist objectForKey:@"capabilities"] setObject:[NSNumber numberWithBool:false] forKey:@"hide-non-default-apps"];
    });
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

static void add_afc2() {
    modify_plist(@"/System/Library/Lockdown/Services.plist", ^(id services) {
        NSDictionary *args = [NSDictionary dictionaryWithObjectsAndKeys:
                              [NSArray arrayWithObjects:@"/usr/libexec/afcd",
                               @"--lockdown",
                               @"-d",
                               @"/",
                               nil], @"ProgramArguments",
                              [NSNumber numberWithBool:true], @"AllowUnauthenticatedServices",
                              @"com.apple.afc2",              @"Label",
                              nil];
        [services setValue:args forKey:@"com.apple.afc2"];
    });
}

@interface LSApplicationWorkspace { }
+(LSApplicationWorkspace *)defaultWorkspace;
-(BOOL)registerApplication:(id)application;
-(BOOL)unregisterApplication:(id)application;
@end

void uicache() {
    // I am not using uicache because I want loc_s to do the reloading

    // probably not safe:
    NSMutableDictionary *cache = [NSMutableDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Caches/com.apple.mobile.installation.plist"];
    if(cache) {
        NSMutableDictionary *cydia = [NSMutableDictionary dictionaryWithContentsOfFile:@"/Applications/Cydia.app/Info.plist"];
        [cydia setObject:@"/Applications/Cydia.app" forKey:@"Path"];
        [cydia setObject:@"System" forKey:@"ApplicationType"];
        id system = [cache objectForKey:@"System"];
        if([system respondsToSelector:@selector(addObject:)])
            [system addObject:cydia];
        else
            [system setObject:cydia forKey:@"com.saurik.Cydia"];
        [cache writeToFile:@"/var/mobile/Library/Caches/com.apple.mobile.installation.plist" atomically:YES];
    }

    DIR* dir = opendir("/private/var/mobile/Library/Caches");
    if(!dir)
        return;

    struct dirent* dp;
    while((dp = readdir(dir)) != NULL)
    {
        if(strncmp(dp->d_name, "com.apple.LaunchServices", sizeof("com.apple.LaunchServices") - 1) != 0)
            continue;

        if(!strstr(dp->d_name, ".csstore"))
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/private/var/mobile/Library/Caches/%s", dp->d_name);
        unlink(path);
    }
}

static int recursive_delete(const char *path, const struct stat *status, int flag, struct FTW *buf)
{
    switch (flag) {
        case FTW_DNR:
        case FTW_DP:
            rmdir(path);
            break;
        case FTW_F:
        case FTW_NS:
        case FTW_SL:
        case FTW_SLN:
            unlink(path);
            break;
    }

    return 0;
}

void delete_dir(const char *path)
{
    nftw(path, recursive_delete, 5, FTW_DEPTH | FTW_PHYS);
}

void prepare_jailbreak_install()
{
    run((char *[]) {"/sbin/mount", "-u", "-o", "rw,suid,dev", "/", NULL}, NULL);
    NSString *string = [NSString stringWithContentsOfFile:@"/etc/fstab" encoding:NSUTF8StringEncoding error:NULL];
    string = [string stringByReplacingOccurrencesOfString:@",nosuid,nodev" withString:@""];
    string = [string stringByReplacingOccurrencesOfString:@" ro " withString:@" rw "];
    [string writeToFile:@"/etc/fstab" atomically:YES encoding:NSUTF8StringEncoding error:NULL];

    dok48();
    add_afc2();
}
