#ifndef NVRAM_H
#define NVRAM_H

void nvram_set(const char* name, const char* value);
const char* nvram_get(const char* name);
void nvram_unset(const char* name);
void nvram_sync(const char* name);

#endif
