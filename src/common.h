#ifndef __COMMON_H
#define __COMMON_H

typedef void (*status_cb_t)(const char* message, int progress, int attention);

int __mkdir(const char* path, int mode);
int mkdir_with_parents(const char *dir, int mode);
char* build_path(const char* elem, ...);

#endif
