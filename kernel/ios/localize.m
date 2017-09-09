#import <Foundation/Foundation.h>
#include <mach-o/getsect.h>

#include "localize.h"

NSDictionary* get_master_language_table()
{
    static NSDictionary* language_table = nil;
    if(language_table)
        return language_table;

    unsigned long languages_size;
    void* languages_data = getsectdata("__DATA", "languages", &languages_size);
    if(!languages_data)
        return nil;

    NSError* error = nil;
    language_table = [NSPropertyListSerialization propertyListWithData:[NSData dataWithBytesNoCopy:languages_data length:languages_size freeWhenDone:NO] options:kCFPropertyListImmutable format:NULL error:&error];
    return language_table;
}

NSDictionary* get_language_table()
{
    static NSDictionary* language_table = nil;
    if(language_table)
        return language_table;

    @try
    {
        language_table = [get_master_language_table() objectForKey:[[[NSDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Preferences/.GlobalPreferences.plist"] objectForKey:@"AppleLanguages"] objectAtIndex:0]];
    } @catch(NSException* e)
    {
        language_table = nil;
        return nil;
    }

    return language_table;
}

CFStringRef localize(const char* str)
{
    NSDictionary* language_table = get_language_table();
    if(!language_table)
        return (CFStringRef)[NSString stringWithUTF8String:str];

    NSString* localized = [language_table objectForKey:[NSString stringWithUTF8String:str]];
    if(localized)
        return (CFStringRef)localized;

    return (CFStringRef)[NSString stringWithUTF8String:str];
}
