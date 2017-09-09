#include <stdint.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

#include "tfp.h"
#include "zone_fix.h"

#define zone_page_table_first_level_slot(x)  ((x) >> ZonePageTableSecondLevelShift)
#define zone_page_table_second_level_slot(x) (((x) << (32 - ZonePageTableSecondLevelShift)) >> (32 - ZonePageTableSecondLevelShift))

struct zone_page_table_entry
{
    uint16_t alloc_count;
#define ZONE_PAGE_USED  0
#define ZONE_PAGE_UNUSED 0xffff
    uint16_t collect_count;
};

static uint32_t ZoneMapStart = 0;
static uint32_t ZoneMapEnd = 0;

static uint32_t ZonePages = 0;
static uint32_t ZonePageTableSecondLevelShift = 0;

static struct zone_page_table_entry* ZonePageTable[32];

static void find_zone_map()
{
    ZoneMapStart = 0;
    ZoneMapEnd = 0;

    mach_port_t kernel_task = get_kernel_task();

    uint32_t memsize;
    size_t value_size = sizeof(memsize);
    sysctlbyname("hw.memsize", &memsize, &value_size, NULL, 0);

    uint32_t kernel_region = get_kernel_region_tfp();

    uint32_t pmap_location;
    uint32_t first_level_page_table_location;
    kernel_read_tfp(kernel_region + get_offsets_from_cache()->pmap_location, &pmap_location, sizeof(pmap_location));
    kernel_read_tfp(pmap_location, &first_level_page_table_location, sizeof(first_level_page_table_location));
    uint32_t first_level_page_table_offset = first_level_page_table_location - kernel_region;

    size_t min_region_size = (memsize - (first_level_page_table_offset + 0x100000)) >> 2;

    vm_address_t addr = 0;
    while(1)
    {
        vm_size_t size;
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
        unsigned int depth = 0;

        if(vm_region_recurse_64(kernel_task, &addr, &size, &depth, (vm_region_info_t)&info, &info_count) != KERN_SUCCESS)
            return;

        if(size > min_region_size && size < memsize)
        {
            ZoneMapStart = addr;
            ZoneMapEnd = addr + size;

            return;
        }

        if((addr + size) <= addr)
            return;

        addr += size;
    }
}

static uint32_t zone_map_start()
{
    if(!ZoneMapStart)
        find_zone_map();
    return ZoneMapStart;
}

static uint32_t zone_map_end()
{
    if(!ZoneMapEnd)
        find_zone_map();
    return ZoneMapEnd;
}

static void load_zone_pages()
{
    ZonePages = (zone_map_end() - zone_map_start()) / 0x1000;
    ZonePageTableSecondLevelShift = 0;
    while((zone_page_table_first_level_slot(ZonePages - 1)) >= 32)
    {
        ++ZonePageTableSecondLevelShift;
    }

    uint32_t kernel_region = get_kernel_region_tfp();

    uint32_t zone_page_table[32];
    kernel_read_tfp(kernel_region + get_offsets_from_cache()->zone_page_table, zone_page_table, sizeof(zone_page_table));

    for(int i = 0; i < ZonePages; ++i)
    {
        uint32_t idx = zone_page_table_first_level_slot(i);
        uint32_t second_level = zone_page_table[idx];
        if(!second_level)
        {
            ZonePageTable[idx] = NULL;
            continue;
        }

        uint32_t size = sizeof(struct zone_page_table_entry) * (1 << ZonePageTableSecondLevelShift);
        ZonePageTable[idx] = (struct zone_page_table_entry*) malloc(size);
        kernel_read_tfp(second_level, ZonePageTable[idx], size);
    }
}

uint32_t zone_pages()
{
    if(!ZonePages)
        load_zone_pages();
    return ZonePages;
}

static struct zone_page_table_entry* zone_page_table_lookup(uint32_t address)
{
    if(!ZonePages)
        load_zone_pages();

    if(address < zone_map_start())
        return NULL;

    if(address >= zone_map_end())
        return NULL;

    uint32_t page = (address - zone_map_start()) / 0x1000;
    struct zone_page_table_entry* entry = ZonePageTable[zone_page_table_first_level_slot(page)];
    if(!entry)
        return NULL;

    return &entry[zone_page_table_second_level_slot(page)];
}

static int is_in_zone(uint32_t address)
{
    struct zone_page_table_entry* entry = zone_page_table_lookup(address);
    if(!entry)
        return 0;

    return entry->alloc_count != ZONE_PAGE_UNUSED;
}

struct ipc_labelh;
struct ipc_kmsg {
    struct ipc_kmsg *ikm_next;
    struct ipc_kmsg *ikm_prev;
    void* ikm_prealloc;
    mach_msg_size_t ikm_size;
    struct ipc_labelh *ikm_sender;
    mach_msg_header_t *ikm_header;
};

#define IO_BITS_KOTYPE 0x00000fff
#define IO_BITS_OTYPE 0x7fff0000
#define IO_BITS_ACTIVE 0x80000000
#define IOT_PORT 0
#define IKOT_NONE 0
#define io_active(bits) (bits & IO_BITS_ACTIVE)
#define io_otype(bits) ((bits & IO_BITS_OTYPE) >> 16)
#define io_kotype(bits) (bits & IO_BITS_KOTYPE)

void clean_up_io_connect_method_leak()
{
    uint32_t kernel_region = get_kernel_region_tfp();

    for(uint32_t i = zone_map_start(); i < zone_map_end(); i += 0x1000)
    {
        struct zone_page_table_entry* entry = zone_page_table_lookup(i);
        if(!entry)
            continue;

        // We're looking for stuff in kalloc.6144, which has an alloc size of 12K, which means 2 elements per alloc, so there is a maximum of 2 elements in a single page
        if(entry->alloc_count != 1 && entry->alloc_count != 2)
            continue;

        uint32_t address;
        for(address = i; address < (i + 0x1000); address += 0x800)
        {
            struct ipc_kmsg kmsg;
            kernel_read_tfp(address, &kmsg, sizeof(kmsg));

            // We're looking for stuff set by ikm_init in ipc_kmsg_alloc for the io_connect_method reply message.
            if(kmsg.ikm_prealloc != NULL || kmsg.ikm_size != 0x167C || kmsg.ikm_sender != NULL)
                continue;

            // The header offset set by ikm_set_header is always constant for io_connect_method.
            uint32_t header_address = address + 0x5a0;
            if((uint32_t)kmsg.ikm_header != header_address)
                continue;

            // Header must also be allocated.
            if(!is_in_zone(header_address))
                continue;

            // Now check the mach message header
            mach_msg_header_t header;
            kernel_read_tfp(header_address, &header, sizeof(header));

            // Check for the exact mig_reply_error_t reply message for io_connect_method
            if(header.msgh_bits != 0x12 || header.msgh_local_port != MACH_PORT_NULL || header.msgh_reserved != 2865 || header.msgh_id != (2865 + 100) || header.msgh_size != 0x24)
                continue;

            uint32_t port_address = header.msgh_remote_port;

            uint32_t ipc_object_bits;
            kernel_read_tfp(port_address, &ipc_object_bits, sizeof(ipc_object_bits));

            // Make sure the message is going to a dead port, as we expect.
            if(io_active(ipc_object_bits) || io_otype(ipc_object_bits) != IOT_PORT || io_kotype(ipc_object_bits) != IKOT_NONE)
                continue;

            kernel_call_tfp(kernel_region + get_offsets_from_cache()->ipc_kmsg_destroy, address, 0, 0, 0);
        }
    }
}

