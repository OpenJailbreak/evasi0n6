#ifndef __JAILBREAK_H
#define __JAILBREAK_H

#include <libimobiledevice/libimobiledevice.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

int jb_device_is_supported(const char* ptype, const char* bver);
void jb_device_event_cb(const idevice_event_t *event, void* userdata);
void jb_signal_handler(int sig);

int jailbreak(const char* udid, status_cb_t status_cb);

const char* get_languages_plist();
size_t get_languages_plist_size();

#ifdef __cplusplus
}
#endif

#endif
