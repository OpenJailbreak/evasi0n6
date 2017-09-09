#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

void wdt_set(uint32_t value)
{
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOWatchDogTimer"));
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    IORegistryEntrySetCFProperties(service, n);
    CFRelease(n);
    IOObjectRelease(service);
}

void wdt_reset(uint32_t timeout)
{
    wdt_set(timeout);
}

void wdt_off()
{
    wdt_set(600);
}
