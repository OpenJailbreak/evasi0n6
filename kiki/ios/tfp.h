#ifndef TFP_H
#define TFP_H

#include <stdint.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

#include "kiki-offsets.h"

struct offsets* get_offsets_from_cache();

mach_port_t get_kernel_task();
uint32_t get_kernel_region_tfp();
int kernel_read_tfp(uint32_t addr, void* buffer, uint32_t size);
int kernel_write_tfp(uint32_t addr, void* buffer, uint32_t size);
void kernel_write_dword_tfp(uint32_t address, uint32_t value);
uint32_t kernel_read_dword_tfp(uint32_t address);
void kernel_write_uint16_tfp(uint32_t address, uint16_t value);
void set_kernel_page_writable_tfp(uint32_t page);
void set_kernel_page_writable_tfp2(io_connect_t connect, uint32_t page);
uint32_t kernel_call_tfp(uint32_t fn, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
void setup_tfp(io_connect_t connect);
void unsetup_tfp();

#endif
