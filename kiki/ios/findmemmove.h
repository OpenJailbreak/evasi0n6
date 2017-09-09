#ifndef FINDMEMMOVE_H
#define FINDMEMMOVE_H

#include <stdint.h>
#include <IOKit/IOKitLib.h>

uint32_t get_memmove();
uint32_t get_memmove2(io_connect_t connect);
void kernel_read_section(io_connect_t connect, uint32_t memmove, uint32_t addr, void* buffer);

#endif
