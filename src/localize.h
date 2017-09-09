#ifndef __LOCALIZE_H
#define __LOCALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

const char* localize(const char* str);

#ifdef WIN32
wchar_t* utf8_to_wchar(const char* text);
#endif

#ifdef __cplusplus
}
#endif

#endif
