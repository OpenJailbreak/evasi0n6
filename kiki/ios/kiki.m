// By planetbeing. IOUSBDeviceInterface bug found by comex.

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#include <IOKit/IOUSBDeviceControllerLib.h>
#include <mach/mach.h>

#include <mach-o/getsect.h>

#include "kiki-offsets.h"
#include "exploit.h"
#include "zone_fix.h"
#include "untar.h"
#include "jailbreakinstaller.h"
#include "framebuffer.h"
#include "tfp.h"
#include "kiki-log.h"
#include "localize.h"

#define USE_NVRAM 1
#define JB_STATE_FILE "/private/var/mobile/Media/jb_state"

static pid_t launchctl_pid = 0;
static int is_test = 0;

static void kernel_write_dword(io_connect_t connect, uint32_t address, uint32_t value)
{
    call_direct(connect, get_kernel_region(connect) + get_offsets()->str_r1_r2_bx_lr, value, address);
}

static uint32_t kernel_read_dword(io_connect_t connect, uint32_t address)
{
    uint32_t value = 0;
    kernel_read(connect, get_offsets()->memmove, address, &value, sizeof(value));
    return value;
}

static void kernel_write_uint16(io_connect_t connect, uint32_t address, uint16_t value)
{
    uint32_t dword_address = address & ~3;
    uint32_t dword = kernel_read_dword(connect, dword_address);
    if(address == dword_address)
        dword = (dword & 0xFFFF0000) | value;
    else
        dword = (dword & 0x0000FFFF) | (value << 16);
    kernel_write_dword(connect, dword_address, dword);
}

static void set_kernel_page_writable(io_connect_t connect, uint32_t page)
{
    static int is_first_run = 1;

    static uint32_t kernel_region;
    static uint32_t pmap_location;
    static uint32_t virtual_start;
    static uint32_t first_level_page_table_location;
    static uint32_t first_level_page_table_physical_location;
    static uint32_t first_level_page_table_entries;
    static uint32_t physical_start;
    static uint32_t* first_level_page_table;

    if(is_first_run)
    {
        kernel_region = get_kernel_region(connect);
        pmap_location = kernel_read_dword(connect, kernel_region + get_offsets()->pmap_location);
        virtual_start = kernel_region;

        uint32_t pmap_data[2];
        kernel_read(connect, get_offsets()->memmove, pmap_location, pmap_data, sizeof(pmap_data));
        first_level_page_table_location = pmap_data[0];
        first_level_page_table_physical_location = pmap_data[1];

        first_level_page_table_entries = kernel_read_dword(connect, pmap_location + 0x54);

        physical_start = first_level_page_table_physical_location - (first_level_page_table_location - virtual_start);

        first_level_page_table = (uint32_t*) malloc(first_level_page_table_entries * sizeof(uint32_t));
        memset(first_level_page_table, 0, first_level_page_table_entries * sizeof(uint32_t));

        is_first_run = 0;
    }

    uint32_t i = page >> 20;
    uint32_t entry = first_level_page_table[i];
    if(entry == 0)
    {
        first_level_page_table[i] = entry = kernel_read_dword(connect, first_level_page_table_location + (sizeof(entry) * i));
    }

    {
        if((entry & 0x3) == 2)
        {
            if((i << 20) == ((page >> 20) << 20))
            {
                entry &= ~(1 << 15);
                kernel_write_dword(connect, first_level_page_table_location + (sizeof(entry) * i), entry);
                goto end;
            }
        } else if((entry & 0x3) == 1)
        {
            uint32_t page_table_address = (entry >> 10) << 10;
            uint32_t virtual_page_table_address = page_table_address - physical_start + virtual_start;

            int j = (page >> 12) & 0xFF;
            uint32_t second_level_entry = kernel_read_dword(connect, virtual_page_table_address + (sizeof(second_level_entry) * j));
            {
                if((second_level_entry & 0x3) == 1)
                {
                    if(((i << 20) + (j << 12)) == page)
                    {
                        second_level_entry &= ~(1 << 9);
                        kernel_write_dword(connect, virtual_page_table_address + (sizeof(second_level_entry) * j), second_level_entry);
                        goto end;
                    }
                } else if((second_level_entry & 0x2) == 2)
                {
                    if(((i << 20) + (j << 12)) == page)
                    {
                        second_level_entry &= ~(1 << 9);
                        kernel_write_dword(connect, virtual_page_table_address + (sizeof(second_level_entry) * j), second_level_entry);
                        goto end;
                    }
                }
            }
        }
    }

end:
    call_direct(connect, kernel_region + get_offsets()->flush_dcache, 0, 0);
    call_direct(connect, kernel_region + get_offsets()->invalidate_tlb, 0, 0);
}

static void patch_proc_enforce()
{
    kernel_write_dword_tfp(get_kernel_region_tfp() + get_offsets()->proc_enforce, 0);
}

static void patch_cs_enforcement_disable_amfi()
{
    kernel_write_dword_tfp(get_kernel_region_tfp() + get_offsets()->cs_enforcement_disable_amfi, 1);
}

static void patch_cs_enforcement_disable_kernel()
{
    kernel_write_dword_tfp(get_kernel_region_tfp() + get_offsets()->cs_enforcement_disable_kernel, 1);
}

static void patch_i_can_has_debugger()
{
    kernel_write_dword_tfp(get_kernel_region_tfp() + get_offsets()->i_can_has_debugger_1, 1);
    kernel_write_dword_tfp(get_kernel_region_tfp() + get_offsets()->i_can_has_debugger_2, 1);
}

static void patch_vm_map_enter()
{
    set_kernel_page_writable_tfp((get_kernel_region_tfp() + get_offsets()->vm_map_enter_patch) & ~0xFFF);
    kernel_write_uint16_tfp(get_kernel_region_tfp() + get_offsets()->vm_map_enter_patch, 0xBF00);
}

static void patch_vm_map_protect()
{
    set_kernel_page_writable_tfp((get_kernel_region_tfp() + get_offsets()->vm_map_protect_patch) & ~0xFFF);
    kernel_write_uint16_tfp(get_kernel_region_tfp() + get_offsets()->vm_map_protect_patch, 0xE005);
}

static void patch_tfp0(io_connect_t connect)
{
    set_kernel_page_writable(connect, (get_kernel_region(connect) + get_offsets()->tfp0_patch) & ~0xFFF);
    kernel_write_uint16(connect, get_kernel_region(connect) + get_offsets()->tfp0_patch, 0xE006);
    call_direct(connect, get_kernel_region(connect) + get_offsets()->flush_dcache, 0, 0);
}

static void patch_bootargs()
{
    uint32_t bootargs = kernel_read_dword_tfp(get_kernel_region_tfp() + get_offsets()->p_bootargs) + 0x38;
    const char* new_bootargs = "cs_enforcement_disable=1";

    size_t new_bootargs_len = strlen(new_bootargs) + 1;
    size_t bootargs_buf_len = (new_bootargs_len + 3) / 4 * 4;
    char bootargs_buf[bootargs_buf_len];

    strlcpy(bootargs_buf, new_bootargs, bootargs_buf_len);
    memset(bootargs_buf + new_bootargs_len, 0, bootargs_buf_len - new_bootargs_len);

    kernel_write_tfp(bootargs, bootargs_buf, bootargs_buf_len);
}

extern void sb_evaluate_trampoline();
extern uint32_t sb_evaluate_trampoline_hook_address;
extern uint32_t sb_evaluate_trampoline_len;

extern void sb_evaluate_hook();
extern uint32_t sb_evaluate_hook_orig_addr;
extern uint32_t sb_evaluate_hook_vn_getpath;
extern uint32_t sb_evaluate_hook_memcmp;
extern uint32_t sb_evaluate_hook_len;

static void patch_sb()
{
    uint32_t kernel_region = get_kernel_region_tfp();
    uint32_t patch_location = kernel_region + get_offsets()->sb_patch;

    uint8_t* sb_evaluate_trampoline_addr = (uint8_t*)(((intptr_t)&sb_evaluate_trampoline) & ~1);
    uint8_t* sb_evaluate_hook_addr = (uint8_t*)(((intptr_t)&sb_evaluate_hook) & ~1);

    uint8_t* trampoline = (uint8_t*) malloc(sb_evaluate_trampoline_len);
    memcpy(trampoline, sb_evaluate_trampoline_addr, sb_evaluate_trampoline_len);
    *((uint32_t*)(trampoline + ((intptr_t)&sb_evaluate_trampoline_hook_address - (intptr_t)sb_evaluate_trampoline_addr))) = kernel_region + 0x701;

    uint32_t max_size_of_possible_overwritten_instructions = sb_evaluate_trampoline_len + 4;
    uint8_t* overwritten_instructions = (uint8_t*) malloc(max_size_of_possible_overwritten_instructions);
    kernel_read_tfp(patch_location, overwritten_instructions, max_size_of_possible_overwritten_instructions);

    if(memcmp(overwritten_instructions, trampoline, sb_evaluate_trampoline_len) == 0)
    {
        // Already patched
        free(overwritten_instructions);
        free(trampoline);
        return;
    }

    uint32_t size_of_possible_overwritten_instructions = 0;
    uint16_t* current_instruction = (uint16_t*) overwritten_instructions;
    while(((intptr_t)current_instruction - (intptr_t)overwritten_instructions) < sb_evaluate_trampoline_len)
    {
        if((*current_instruction & 0xe000) == 0xe000 && (*current_instruction & 0x1800) != 0x0)
        {
            size_of_possible_overwritten_instructions += 4;
            current_instruction += 2;
        } else
        {
            size_of_possible_overwritten_instructions += 2;
            current_instruction += 1;
        }
    }

    uint16_t bx_r9 = 0x4748;

    uint32_t hook_size = ((sb_evaluate_hook_len + size_of_possible_overwritten_instructions + sizeof(bx_r9) + 3) / 4) * 4;
    uint8_t* hook = (uint8_t*) malloc(hook_size);
    memcpy(hook, sb_evaluate_hook_addr, sb_evaluate_hook_len);
    memcpy(hook + sb_evaluate_hook_len, overwritten_instructions, size_of_possible_overwritten_instructions);
    memcpy(hook + sb_evaluate_hook_len + size_of_possible_overwritten_instructions, &bx_r9, sizeof(bx_r9));

    *((uint32_t*)(hook + ((intptr_t)&sb_evaluate_hook_orig_addr - (intptr_t)sb_evaluate_hook_addr))) = patch_location + size_of_possible_overwritten_instructions + 1;
    *((uint32_t*)(hook + ((intptr_t)&sb_evaluate_hook_vn_getpath - (intptr_t)sb_evaluate_hook_addr))) = kernel_region + get_offsets()->vn_getpath;
    *((uint32_t*)(hook + ((intptr_t)&sb_evaluate_hook_memcmp - (intptr_t)sb_evaluate_hook_addr))) = kernel_region + get_offsets()->memcmp;

    set_kernel_page_writable_tfp((kernel_region + 0x700) & ~0xFFF);
    kernel_write_tfp(kernel_region + 0x700, hook, hook_size);
    set_kernel_page_writable_tfp(patch_location & ~0xFFF);
    kernel_write_tfp(patch_location, trampoline, sb_evaluate_trampoline_len);
    kernel_call_tfp(kernel_region + get_offsets_from_cache()->flush_dcache, 0, 0, 0, 0);

    free(overwritten_instructions);
    free(trampoline);
    free(hook);
}

#define RB_AUTOBOOT 0
int reboot(int);

static void install_jailbreak()
{
    struct stat stat_buf;
    if(stat("/private/var/mobile/Media/evasi0n-install/Cydia.tar", &stat_buf) != 0)
        return;
    if(stat("/private/var/mobile/Media/evasi0n-install/packagelist.tar", &stat_buf) != 0)
        return;

    fb_setup();
    fb_update_status(0.0f, localize("Setting up fstab and AFC2..."));
    prepare_jailbreak_install();

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

    fb_update_status(0.05f, localize("Setting up Cydia..."));
    jb_log("Untarring Cydia...");
    untar("/private/var/mobile/Media/evasi0n-install/Cydia.tar", "/");

    fb_update_status(0.75f, localize("Setting up Cydia packages..."));
    jb_log("Untarring Cydia packages...");
    untar("/private/var/mobile/Media/evasi0n-install/packagelist.tar", "/");

    unlink("/private/var/mobile/Media/evasi0n-install/Cydia.tar");
    unlink("/private/var/mobile/Media/evasi0n-install/packagelist.tar");

    if(stat("/private/var/mobile/Media/evasi0n-install/extras.tar", &stat_buf) == 0)
    {
        fb_update_status(0.85f, localize("Setting up extra packages..."));
        jb_log("Untarring extras...");
        untar("/private/var/mobile/Media/evasi0n-install/extras.tar", "/");
        unlink("/private/var/mobile/Media/evasi0n-install/extras.tar");
    }

    FILE* info = fopen("/var/lib/dpkg/status", "a");
    if(info)
    {
        unsigned long control_size;
        void* control = getsectdata("__DATA", "control", &control_size);
        fprintf(info, "\n");
        fwrite(control, 1, control_size, info);
        fprintf(info, "Status: install ok installed\n\n");
        fclose(info);
    }

    rmdir("/private/var/mobile/Media/evasi0n-install");
    unlink("/private/var/mobile/Media/mount.stderr");
    unlink("/private/var/mobile/Media/mount.stdout");

    if(lstat("/private/etc/launchd.conf", &stat_buf) == 0)
    {
        if((stat_buf.st_mode & S_IFMT) == S_IFLNK)
        {
            if(stat("/private/var/evasi0n/launchd.conf", &stat_buf) == 0)
            {
                uint8_t buffer[stat_buf.st_size];
                FILE* f = fopen("/private/var/evasi0n/launchd.conf", "rb");
                fread(buffer, 1, stat_buf.st_size, f);
                fclose(f);

                f = fopen("/private/etc/launchd.conf.2", "wb");
                fwrite(buffer, 1, stat_buf.st_size, f);
                fclose(f);

                unlink("/private/etc/launchd.conf");
                rename("/private/etc/launchd.conf.2", "/private/etc/launchd.conf");
            }
        }
    }

    delete_dir("/private/var/mobile/DemoApp.app");

    disable_ota_updates();
    hide_weather_on_ipads();

    fb_update_status(1.0f, localize("Rebooting..."));
    sync();
    reboot(RB_AUTOBOOT);
}

static const char* kern_bootargs()
{
    static char buffer[256];
    size_t buffer_size = sizeof(buffer);
    sysctlbyname("kern.bootargs", buffer, &buffer_size, NULL, 0);
    return buffer;
}

static void kernel_read_for_offsets(void* context, uint32_t address, void* buffer, uint32_t size)
{
    io_connect_t connect = (io_connect_t) context;
    kernel_read(connect, get_offsets()->memmove, address, buffer, size);
}

static void credits(io_connect_t connect)
{
    uint32_t kernel_region = get_kernel_region_tfp();

    io_connect_t local_connect = 0;
    if(!connect)
    {
        io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOUSBDeviceInterface"));
        IOServiceOpen(service, mach_task_self(), 0, &local_connect);
        IOObjectRelease(service);
        connect = local_connect;

        initialize_offsets(connect);
        load_offsets(kernel_region, kernel_read_for_offsets, (void*)connect);
    }

    kernel_write_tfp(kernel_region + 0xe80, "evasi0n loaded. I fight for the Users.\n", sizeof("evasi0n loaded. I fight for the Users.\n"));
    kernel_call_tfp(kernel_region + get_offsets()->IOLog, kernel_region + 0xe80, 0, 0, 0);

    kernel_write_tfp(kernel_region + 0xf00, "by the evad3rs: planetbeing nikias pod2g musclenerd\n\0For S.C., RIP.", sizeof("by the evad3rs: planetbeing nikias pod2g musclenerd\n\0For S.C., RIP."));
    kernel_call_tfp(kernel_region + get_offsets()->IOLog, kernel_region + 0xf00, 0, 0, 0);

    if(local_connect)
    {
        IOServiceClose(local_connect);
    }
}

static void jailbreak()
{
    jb_log("Starting for %s %s", get_platform(), get_os_build_version());

    kern_return_t ret;
    io_connect_t connect = 0;
    io_service_t service = 0;

    for(int wait = 0; wait < 300; ++wait)
    {
        service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOUSBDeviceInterface"));
        if(service == 0)
        {
            usleep(100000);
            continue;
        }

        ret = IOServiceOpen(service, mach_task_self(), 0, &connect);

        jb_log("IOServiceOpen = 0x%x", ret);
        if(ret == kIOReturnSuccess)
            break;

        usleep(100000);
    }

    if(!connect)
    {
        jb_log("Waited too long! Letting boot continue.\n");
        return;
    }

    uint32_t kernel_region = get_kernel_region(connect);
    jb_log("Kernel Region: 0x%08x", kernel_region);

    int first_run = 0;
    struct stat stat_buf;
    if(stat("/private/var/evasi0n/memmove.cache", &stat_buf) != 0)
    {
        first_run = 1;
        fb_setup();
    }

    if(first_run)
        fb_update_status_blinking(0.0f, localize("Initializing offsets..."));

    initialize_offsets(connect);
    jb_log("Offsets initialized.");

    if(first_run)
        fb_update_status_blinking(30.0f, localize("Finding offsets..."));

    load_offsets(kernel_region, kernel_read_for_offsets, (void*)connect);
    jb_log("Offsets loaded.");

    if(first_run)
        fb_update_status_blinking(60.0f, localize("Patching kernel..."));

    patch_tfp0(connect);

    setup_tfp(connect);

    uint32_t value;
    size_t value_size = sizeof(value);
    sysctlbyname("security.mac.proc_enforce", &value, &value_size, NULL, 0);
    jb_log("old proc_enforce = %d\n", value);
    patch_proc_enforce();
    value_size = sizeof(value);
    sysctlbyname("security.mac.proc_enforce", &value, &value_size, NULL, 0);
    jb_log("new proc_enforce = %d\n", value);
    patch_cs_enforcement_disable_amfi();
    patch_cs_enforcement_disable_kernel();
    patch_i_can_has_debugger();
    jb_log("old bootargs = %s\n", kern_bootargs());
    patch_bootargs();
    jb_log("new bootargs = %s\n", kern_bootargs());
    jb_log("Done with data patches\n");

    patch_vm_map_enter();
    patch_vm_map_protect();
    patch_sb();

    jb_log("Cleaning up...");

    if(first_run)
        fb_update_status_blinking(80.0f, localize("Cleaning up..."));

    if(!is_test)
        clean_up_io_connect_method_leak();

    credits(connect);

    unsetup_tfp();

    jb_log("Done!");

    if(first_run)
        fb_update_status(80.0f, localize("Done! Continuing boot..."));

    kill(launchctl_pid, SIGCONT);

    while(1)
    {
        sleep(1);

        CFTypeRef finalized = IORegistryEntryCreateCFProperty(service, CFSTR("FinalizedTimestamp"), NULL, 0);
        if(!finalized)
        {
            jb_log("Not yet finalized...");
            continue;
        }

        CFRelease(finalized);
        IOObjectRelease(service);

#if USE_NVRAM
	unlink(JB_STATE_FILE);
        sync();
#endif

        jb_log("Exiting.");
        jb_end_log();
        fb_cleanup();
        exit(0);
    }

    return;
}

static void arrival_callback(void* context, IOUSBDeviceControllerRef device)
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    IOUSBDeviceDescriptionRef description = IOUSBDeviceDescriptionCreateFromDefaults(NULL);
    IOUSBDeviceDescriptionSetSerialString(description, (CFStringRef)[NSString stringWithContentsOfFile:@"/private/var/evasi0n/udid" encoding:NSUTF8StringEncoding error:NULL]);
    IOUSBDeviceControllerSetDescription(device, description);
    jailbreak();
    [pool release];
}

static void block_until_exception_port_up()
{
    // Temporary hack to make sure the EXC_BAD_INSTRUCTION handler is up
    for(int wait = 0; wait < 100; ++wait)
    {
        exception_mask_t masks[EXC_TYPES_COUNT];
        mach_port_t ports[EXC_TYPES_COUNT];
        exception_behavior_t behaviors[EXC_TYPES_COUNT];
        thread_state_flavor_t flavors[EXC_TYPES_COUNT];
        mach_msg_type_number_t count = EXC_TYPES_COUNT;

        if(host_get_exception_ports(mach_host_self(), EXC_MASK_ALL, masks, &count, ports, behaviors, flavors) != KERN_SUCCESS)
            continue;

        for(int i = 0; i < count; ++i)
        {
            mach_port_deallocate(mach_host_self(), ports[i]);

            if(masks[i] == EXC_MASK_BAD_INSTRUCTION && behaviors[i] == EXCEPTION_STATE_IDENTITY && flavors[i] == ARM_THREAD_STATE)
                return;
        }

        usleep(100000);
    }
}

static void crash_handler(int signal)
{
    jb_log("We crashed!");
    kill(launchctl_pid, SIGCONT);
    reboot(RB_AUTOBOOT);
}

int main(int argc, char* argv[])
{
    if(argc > 1 && strcmp(argv[1], "--block-until-exception-port-up") == 0)
    {
        block_until_exception_port_up();
        return 0;
    }

    if(argc > 1 && strcmp(argv[1], "--fb-test") == 0)
    {
        fb_setup();
        jb_log("Scale: %d", get_scale());
        fb_update_status_blinking(1.0f, localize("Initializing offsets..."));
        sleep(1);
        fb_update_status_blinking(2.0f, localize("Finding offsets..."));
        sleep(1);
        fb_update_status_blinking(3.0f, localize("Patching kernel..."));
        sleep(1);
        fb_update_status_blinking(3.0f, localize("Cleaning up..."));
        fb_cleanup();
        credits(0);
        return 0;
    }

    jb_log("Starting...");

    // Will reboot if jailbreak not already installed.
    install_jailbreak();

#if USE_NVRAM
    FILE* jb_state_file = fopen(JB_STATE_FILE, "rb");
    int jb_state = 0;
    if(jb_state_file)
    {
        fscanf(jb_state_file, "%d", &jb_state);
        fclose(jb_state_file);
    }

    if(jb_state >= 5)
    {
        jb_log("Exiting due to bad jb_state.");
        unlink(JB_STATE_FILE);
        sync();
        return 0;
    }

    ++jb_state;

    jb_state_file = fopen(JB_STATE_FILE, "wb");
    if(jb_state_file)
    {
        fprintf(jb_state_file, "%d", jb_state);
        fclose(jb_state_file);
        sync();
    }

    jb_log("Setting jb_state and forking...");
#endif

    stack_t sigstk;
    sigstk.ss_sp = malloc(0x2000);
    sigstk.ss_size = 0;
    sigstk.ss_flags = 0;
    sigaltstack(&sigstk, 0);

    signal(SIGSEGV, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGTRAP, crash_handler);

    launchctl_pid = getppid();

    if(!(argc > 1 && strcmp(argv[1], "--test") == 0))
    {
        kill(launchctl_pid, SIGSTOP);

        if(fork() != 0)
            exit(0);

        setsid();
        is_test = 0;
    } else
    {
        is_test = 1;
    }

    IOUSBDeviceControllerRegisterArrivalCallback(arrival_callback, NULL, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    jb_log("Waiting...");
    CFRunLoopRun();

    return 0;
}
