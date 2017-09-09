#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <CoreFoundation/CoreFoundation.h>

void nvram_set(const char* name, const char* value)
{
    io_registry_entry_t options = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if(!options)
        return;

    CFStringRef cf_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    CFStringRef cf_value = CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);

    IORegistryEntrySetCFProperty(options, cf_name, cf_value);

    CFRelease(cf_name);
    CFRelease(cf_value);

    IOObjectRelease(options);
}

const char* nvram_get(const char* name)
{
    static char buffer[1024];
    io_registry_entry_t options = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if(!options)
        return NULL;

    CFStringRef cf_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);

    CFTypeRef cf_value = IORegistryEntryCreateCFProperty(options, cf_name, kCFAllocatorDefault, 0);
    if(!cf_value)
    {
        CFRelease(cf_name);
        IOObjectRelease(options);
        return NULL;
    }

    CFStringRef cf_value_str = CFCopyDescription(cf_value);
    if(!cf_value_str)
    {
        CFRelease(cf_value);
        CFRelease(cf_name);
        IOObjectRelease(options);
        return NULL;
    }

    if(!CFStringGetCString(cf_value_str, buffer, sizeof(buffer), kCFStringEncodingUTF8))
    {
        CFRelease(cf_value_str);
        CFRelease(cf_value);
        CFRelease(cf_name);
        IOObjectRelease(options);
        return NULL;
    }

    CFRelease(cf_value_str);
    CFRelease(cf_value);
    CFRelease(cf_name);

    IOObjectRelease(options);

    return buffer;
}

void nvram_unset(const char* name)
{
    nvram_set(kIONVRAMDeletePropertyKey, name);
}

void nvram_sync(const char* name)
{
    nvram_set(kIONVRAMSyncNowPropertyKey, name);
}
