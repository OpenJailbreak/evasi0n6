#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/select.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *memcpy(void *dest, const void *src, size_t n)
{
    return memmove(dest, src, n);
}

unsigned long int __fdelt_chk (unsigned long int d)
{
    if (d >= FD_SETSIZE)
        abort();

    return d / __NFDBITS;
}

int __isoc99_sscanf(const char *str, const char *format, ...)
{
    va_list arg;
    int done;

    va_start (arg, format);
    done = vsscanf(str, format, arg);
    va_end (arg);

    return done;
}

void __stack_chk_fail()
{
    abort();
}
