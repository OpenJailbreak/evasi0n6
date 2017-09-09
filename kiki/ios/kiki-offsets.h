#ifndef KIKI_OFFSETS_H
#define KIKI_OFFSETS_H

#include <stdint.h>
#include <IOKit/IOKitLib.h>

struct offsets
{
    char platform[20];
    char build[20];
    uint32_t memmove;
    uint32_t str_r1_r2_bx_lr;
    uint32_t flush_dcache;
    uint32_t invalidate_tlb;
    uint32_t pmap_location;
    uint32_t proc_enforce;
    uint32_t cs_enforcement_disable_amfi;
    uint32_t cs_enforcement_disable_kernel;
    uint32_t i_can_has_debugger_1;
    uint32_t i_can_has_debugger_2;
    uint32_t vm_map_enter_patch;
    uint32_t vm_map_protect_patch;
    uint32_t tfp0_patch;
    uint32_t sb_patch;
    uint32_t vn_getpath;
    uint32_t memcmp;
    uint32_t p_bootargs;
    uint32_t zone_page_table;
    uint32_t ipc_kmsg_destroy;
    uint32_t syscall0;
    uint32_t io_free;
    uint32_t IOLog;
    uint32_t crc;
};

const char* get_platform();
const char* get_os_build_version();
void initialize_offsets(io_connect_t connect);
struct offsets* get_offsets();
void load_offsets(uint32_t region, void (*kernel_read)(void* context, uint32_t address, void* buffer, uint32_t size), void* context);

#endif
