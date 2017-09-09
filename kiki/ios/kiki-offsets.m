#include "kiki-offsets.h"

#include <sys/utsname.h>
#include <mach-o/loader.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <zlib.h> // for crc32

#include "patchfinder.h"
#include "findmemmove.h"
#include "kiki-log.h"
#include "watermark.h"

static struct offsets GlobalOffsets;
static BOOL OffsetsInitialized = NO;

struct memmove_cache
{
    char platform[20];
    char build[20];
    uint32_t memmove;
    uint32_t crc;
};

static void generate_memmove_crc(struct memmove_cache* o)
{
    o->crc = crc32(0, (const Bytef*)o, (intptr_t)&o->crc - (intptr_t)o);
}

static int validate_memmove_crc(struct memmove_cache* o)
{
    return o->crc == crc32(0, (const Bytef*)o, (intptr_t)&o->crc - (intptr_t)o);
}

static uint32_t create_memmove_cache(io_connect_t connect)
{
    if(!connect)
        return 0;

    struct memmove_cache m;
    strlcpy(m.platform, get_platform(), sizeof(m.platform));
    strlcpy(m.build, get_os_build_version(), sizeof(m.build));
    m.memmove = get_memmove2(connect);
    generate_memmove_crc(&m);
    FILE* f = fopen("/private/var/evasi0n/memmove.cache", "wb");
    fwrite(&m, 1, sizeof(m), f);
    fclose(f);
    return m.memmove;
}

static uint32_t get_cached_memmove(io_connect_t connect)
{
    FILE* f = fopen("/private/var/evasi0n/memmove.cache", "rb");
    if(!f)
        return create_memmove_cache(connect);

    struct memmove_cache m;
    if(fread(&m, 1, sizeof(m), f) != sizeof(m))
    {
        fclose(f);
        return create_memmove_cache(connect);
    }

    fclose(f);

    if(strcmp(m.platform, get_platform()) != 0 && strcmp(m.build, get_os_build_version()) != 0)
    {
        return create_memmove_cache(connect);
    }

    if(!validate_memmove_crc(&m))
    {
        return create_memmove_cache(connect);
    }

    return m.memmove;
}

struct offsets* get_offsets()
{
    if(OffsetsInitialized)
        return &GlobalOffsets;

    return NULL;
}

void initialize_offsets(io_connect_t connect)
{
    memset(&GlobalOffsets, 0, sizeof(GlobalOffsets));
    strlcpy(GlobalOffsets.platform, get_platform(), sizeof(GlobalOffsets.platform));
    strlcpy(GlobalOffsets.build, get_os_build_version(), sizeof(GlobalOffsets.build));
    GlobalOffsets.memmove = get_cached_memmove(connect);

    OffsetsInitialized = YES;
}

const char* get_os_build_version()
{
    static char buffer[256] = { 0 };

    if(buffer[0] != '\0')
        return buffer;

    CFURLRef url = CFURLCreateWithString(NULL, CFSTR("file:///System/Library/CoreServices/SystemVersion.plist"), NULL);
    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
    CFRelease(url);

    CFReadStreamOpen(stream);

    CFPropertyListRef propertyList = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(stream);

    CFStringRef productBuildVersion = CFDictionaryGetValue(propertyList, CFSTR("ProductBuildVersion"));
    CFStringGetCString(productBuildVersion, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(propertyList);

    return buffer;
}

const char* get_platform()
{
    static char buffer[256] = { 0 };

    if(buffer[0] != '\0')
        return buffer;

    struct utsname name;
    uname(&name);

    strlcpy(buffer, name.machine, sizeof(buffer));
    return buffer;
}

CFDictionaryRef OSKextCopyLoadedKextInfo(CFArrayRef kextIdentifiers, CFArrayRef infoKeys);
static uint32_t kernel_size()
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    NSArray* kextIdentifiers = [NSArray arrayWithObjects:@"__kernel__", @"com.apple.driver.AppleMobileFileIntegrity", @"com.apple.security.sandbox", nil];
    NSArray* infoKeys = [NSArray arrayWithObject:@"OSBundleMachOHeaders"];
    NSDictionary* bundles = (NSDictionary*) OSKextCopyLoadedKextInfo((CFArrayRef)kextIdentifiers, (CFArrayRef)infoKeys);

    uint32_t max_extent = 0;
    uint32_t min_extent = 0xFFFFFFFF;
    for(NSDictionary* header_data in [bundles allValues])
    {
        struct mach_header* kernel_header = (struct mach_header*)[[header_data objectForKey:@"OSBundleMachOHeaders"] bytes];
        struct load_command* command = (struct load_command*)((uintptr_t)kernel_header + sizeof(struct mach_header));

        for(uint32_t i = 0; i < kernel_header->ncmds; ++i)
        {
            if(command->cmd == LC_SEGMENT)
            {
                struct segment_command* segment = (struct segment_command*)command;
                if(strcmp(segment->segname, "__TEXT") == 0 || strcmp(segment->segname, "__DATA") == 0)
                {
                    uint32_t extent = segment->vmaddr + segment->vmsize;
                    if(extent > max_extent)
                        max_extent = extent;

                    if(segment->vmaddr < min_extent)
                        min_extent = segment->vmaddr;
                }
            }

            command = (struct load_command*)((uintptr_t)command + command->cmdsize);
        }
    }

    [pool release];

    return max_extent - (min_extent - 0x1000);
}

static void generate_offsets_crc(struct offsets* o)
{
    o->crc = crc32(0, (const Bytef*)o, (intptr_t)&o->crc - (intptr_t)o);
}

static int validate_offsets_crc(struct offsets* o)
{
    return o->crc == crc32(0, (const Bytef*)o, (intptr_t)&o->crc - (intptr_t)o);
}

#define RB_AUTOBOOT 0
int reboot(int);

static void load_offsets_internal(uint32_t region, void (*kernel_read)(void* context, uint32_t address, void* buffer, uint32_t size), void* context)
{
    size_t ksize = kernel_size();

#if KERNEL_DUMP
    ksize = 0xC00000;
#endif

    uint8_t* kdata = malloc(ksize);
    kernel_read(context, region, kdata, ksize);

#if KERNEL_DUMP
    char kdump_path[PATH_MAX];
    snprintf(kdump_path, sizeof(kdump_path), "/private/var/mobile/Media/kerneldump_%s_%s", get_platform(), get_os_build_version());
    FILE* kdump_file = fopen(kdump_path, "wb");
    fwrite(kdata, 1, ksize, kdump_file);
    fclose(kdump_file);
#endif

    if(*(uint32_t*)(kdata + 0x1000) != 0xfeedface)
    {
        // wtf? we fail
        reboot(RB_AUTOBOOT);
    }

    strlcpy(GlobalOffsets.platform, get_platform(), sizeof(GlobalOffsets.platform));
    strlcpy(GlobalOffsets.build, get_os_build_version(), sizeof(GlobalOffsets.build));
    GlobalOffsets.memmove = find_memmove(region, kdata, ksize);
    GlobalOffsets.str_r1_r2_bx_lr = find_str_r1_r2_bx_lr(region, kdata, ksize);
    GlobalOffsets.flush_dcache = find_flush_dcache(region, kdata, ksize);
    GlobalOffsets.invalidate_tlb = find_invalidate_tlb(region, kdata, ksize);
    GlobalOffsets.pmap_location = find_pmap_location(region, kdata, ksize);
    GlobalOffsets.proc_enforce = find_proc_enforce(region, kdata, ksize);
    GlobalOffsets.cs_enforcement_disable_amfi = find_cs_enforcement_disable_amfi(region, kdata, ksize);
    GlobalOffsets.cs_enforcement_disable_kernel = find_cs_enforcement_disable_kernel(region, kdata, ksize);
    GlobalOffsets.i_can_has_debugger_1 = find_i_can_has_debugger_1(region, kdata, ksize);
    GlobalOffsets.i_can_has_debugger_2 = find_i_can_has_debugger_2(region, kdata, ksize);
    GlobalOffsets.vm_map_enter_patch = find_vm_map_enter_patch(region, kdata, ksize);
    GlobalOffsets.vm_map_protect_patch = find_vm_map_protect_patch(region, kdata, ksize);
    GlobalOffsets.tfp0_patch = find_tfp0_patch(region, kdata, ksize);
    GlobalOffsets.sb_patch = find_sb_patch(region, kdata, ksize);
    GlobalOffsets.vn_getpath = find_vn_getpath(region, kdata, ksize);
    GlobalOffsets.memcmp = find_memcmp(region, kdata, ksize);
    GlobalOffsets.p_bootargs = find_p_bootargs(region, kdata, ksize);
    GlobalOffsets.zone_page_table = find_zone_page_table(region, kdata, ksize);
    GlobalOffsets.ipc_kmsg_destroy = find_ipc_kmsg_destroy(region, kdata, ksize);
    GlobalOffsets.syscall0 = find_syscall0(region, kdata, ksize);
    GlobalOffsets.io_free = find_io_free(region, kdata, ksize);
    GlobalOffsets.IOLog = find_IOLog(region, kdata, ksize);
    generate_offsets_crc(&GlobalOffsets);

    FILE* f = fopen("/private/var/evasi0n/cache", "wb");
    fwrite(&GlobalOffsets, 1, sizeof(GlobalOffsets), f);
    fclose(f);

    free(kdata);

    GlobalOffsets.memmove ^= watermark_generate();
    GlobalOffsets.memmove ^= watermark_static();
}

void load_offsets(uint32_t region, void (*kernel_read)(void* context, uint32_t address, void* buffer, uint32_t size), void* context)
{
    FILE* f = fopen("/private/var/evasi0n/cache", "rb");
    if(!f)
    {
        load_offsets_internal(region, kernel_read, context);
        return;
    }

    struct offsets o;
    size_t bytes = fread(&o, 1, sizeof(o), f);
    if(bytes != sizeof(o))
    {
        fclose(f);
        load_offsets_internal(region, kernel_read, context);
        return;
    }

    fclose(f);

    if(!validate_offsets_crc(&o))
    {
        load_offsets_internal(region, kernel_read, context);
        return;
    }

    if(strcmp(o.platform, get_platform()) != 0 || strcmp(o.build, get_os_build_version()) != 0)
    {
        load_offsets_internal(region, kernel_read, context);
        return;
    }

    memcpy(&GlobalOffsets, &o, sizeof(GlobalOffsets));
    GlobalOffsets.memmove ^= watermark_generate();
    GlobalOffsets.memmove ^= watermark_static();
}
