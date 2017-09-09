#ifndef __bsdprocesslist_h
#define __bsdprocesslist_h

#include <stdlib.h>
#include <sys/sysctl.h>

typedef struct kinfo_proc kinfo_proc;

#ifdef __cplusplus
extern "C" {
#endif

int GetBSDProcessList(kinfo_proc **procList, size_t *procCount);

#ifdef __cplusplus
}
#endif

#endif
