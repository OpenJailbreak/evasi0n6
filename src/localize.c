#include <stdlib.h>
#include <stdint.h>
#include <plist/plist.h>

#include "localize.h"
#include "hashmap.h"
#include "jailbreak.h"

#ifdef __APPLE__
// OS X - Start
#include <CoreFoundation/CoreFoundation.h>

void get_language_and_variant(const char** language, const char** variant)
{
    static char s_language[100];
    static char s_variant[100];
    static int language_set = 0;

    if(language_set)
    {
        if(language) *language = s_language;
        if(variant) *variant = s_variant;
        return;
    }

    CFArrayRef preferred = CFLocaleCopyPreferredLanguages();
    if(CFArrayGetCount(preferred) > 0)
    {
        CFStringRef locale_name = CFArrayGetValueAtIndex(preferred, 0);
        CFLocaleRef locale = CFLocaleCreate(kCFAllocatorDefault, locale_name);
        if(CFStringGetCString(CFLocaleGetValue(locale, kCFLocaleLanguageCode), s_language, sizeof(s_language), kCFStringEncodingUTF8))
        {
            if(CFStringGetCString(locale_name, s_variant, sizeof(s_variant), kCFStringEncodingUTF8))
            {
                language_set = 1;
            }
        }

        CFRelease(locale);
    }

    if(!language_set)
    {
        strcpy(s_language, "en");
        strcpy(s_variant, "en");
    }

    if(language) *language = s_language;
    if(variant) *variant = s_variant;
}

// OS X - End
#else
#ifdef WIN32
// Windows - Start

#include <windows.h>

void get_language_and_variant(const char** language, const char** variant)
{
    static char s_language[10];
    static char s_variant[10];
    static int language_set = 0;

    if(language_set)
    {
        if(language) *language = s_language;
        if(variant) *variant = s_variant;
        return;
    }

    LANGID langid = GetUserDefaultLangID();
    char lang[19];
    GetLocaleInfoA(langid, LOCALE_SISO639LANGNAME, lang, 9);
    int ccBuf = strlen(lang);
    lang[ccBuf++] = '_';
    ccBuf += GetLocaleInfoA(langid, LOCALE_SISO3166CTRYNAME, lang + ccBuf, 9);

    int cursor = 0;
    int i;
    for(i = 0; i < strlen(lang); ++i)
    {
	if((lang[i] >= 'a' && lang[i] <= 'z') || (lang[i] >= 'A' && lang[i] <= 'Z'))
	{
	    s_language[cursor] = lang[i];
	    ++cursor;
	} else
	    break;
    }

    if(cursor > 0)
    {
	s_language[cursor] = '\0';
	strncpy(s_variant, lang, sizeof(s_variant));
	s_variant[sizeof(s_variant) - 1] = '\0';
	language_set = 1;
    }

    if(!language_set)
    {
        strcpy(s_language, "en");
        strcpy(s_variant, "en");
    }

    if(language) *language = s_language;
    if(variant) *variant = s_variant;
}

// Windows - End
#else
// Linux - Start
#include <stdlib.h>
#include <string.h>

void get_language_and_variant(const char** language, const char** variant)
{
    static char s_language[100];
    static char s_variant[100];
    static int language_set = 0;

    if(language_set)
    {
        if(language) *language = s_language;
        if(variant) *variant = s_variant;
        return;
    }

    char* lang = getenv("LANG");
    if(lang)
    {
        int cursor = 0;
        int i;
        for(i = 0; i < strlen(lang); ++i)
        {
            if((lang[i] >= 'a' && lang[i] <= 'z') || (lang[i] >= 'A' && lang[i] <= 'Z'))
            {
                s_language[cursor] = lang[i];
                ++cursor;
            } else
                break;
        }

        if(cursor > 0)
        {
            s_language[cursor] = '\0';
            strncpy(s_variant, lang, sizeof(s_variant));
            s_variant[sizeof(s_variant) - 1] = '\0';
            language_set = 1;
        }
    }

    if(!language_set)
    {
        strcpy(s_language, "en");
        strcpy(s_variant, "en");
    }

    if(language) *language = s_language;
    if(variant) *variant = s_variant;
}

// Linux - End
#endif
#endif

#ifdef WIN32

#include <windows.h>

static int tls_index = 0;
static int tls_wchar_index = 0;

__attribute__((constructor)) void initialize_tls_localize_buffer()
{
    tls_index = TlsAlloc();
    tls_wchar_index = TlsAlloc();
}

map_t get_tls_localize_buffer()
{
    void* ptr;
    if((ptr = TlsGetValue(tls_index)) == NULL)
    {
        ptr = hashmap_new();
        TlsSetValue(tls_index, ptr);
    }

    return (map_t)ptr;
}

map_t get_tls_wchar_buffer()
{
    void* ptr;
    if((ptr = TlsGetValue(tls_wchar_index)) == NULL)
    {
        ptr = hashmap_new();
        TlsSetValue(tls_wchar_index, ptr);
    }

    return (map_t)ptr;
}

wchar_t* utf8_to_wchar(const char* text)
{
    map_t tls_wchar_buffer = get_tls_wchar_buffer();

    wchar_t* mbstr = NULL;
    if(hashmap_get(tls_wchar_buffer, text, (any_t*)&mbstr) == MAP_OK)
        return mbstr;

    int mblen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if(mblen == 0)
        return L"???";

    mbstr = (wchar_t*)malloc(mblen*sizeof(wchar_t*));
    if(!mbstr)
        return L"???";

    if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, mbstr, mblen) == 0)
    {
        free(mbstr);
        return L"???";
    }

    hashmap_put(tls_wchar_buffer, text, mbstr);
    return mbstr;
}

#else

#include <pthread.h>

static pthread_key_t tls_localize_buffer_key;

void tls_localize_buffer_dtor(void* buffer)
{
    hashmap_free((map_t)buffer);
}

void initialize_tls_localize_buffer()
{
    pthread_key_create(&tls_localize_buffer_key, tls_localize_buffer_dtor);
}

map_t get_tls_localize_buffer()
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    pthread_once(&once_control, initialize_tls_localize_buffer);

    void* ptr;
    if((ptr = pthread_getspecific(tls_localize_buffer_key)) == NULL)
    {
        ptr = hashmap_new();
        pthread_setspecific(tls_localize_buffer_key, ptr);
    }

    return (map_t)ptr;
}

#endif

const char* get_language()
{
    static const char* language = NULL;
    if(language)
        return language;

    get_language_and_variant(&language, NULL);
    return language;
}

const char* get_variant()
{
    static const char* variant = NULL;
    if(variant)
        return variant;

    get_language_and_variant(NULL, &variant);
    return variant;
}

static plist_t check_alias(plist_t plist, const char* lang)
{
    plist_t alias_table = plist_dict_get_item(plist, "aliases");
    if(!alias_table || plist_get_node_type(alias_table) != PLIST_DICT)
        return NULL;

    plist_t alias_entry = plist_dict_get_item(alias_table, lang);
    if(!alias_entry || plist_get_node_type(alias_entry) != PLIST_STRING)
        return NULL;

    char* alias = NULL;
    plist_get_string_val(alias_entry, &alias);

    plist_t language_table = plist_dict_get_item(plist, alias);
    if(language_table && plist_get_node_type(language_table) == PLIST_DICT)
        return language_table;
    else
        return NULL;
}

plist_t get_language_table()
{
    const char* language = get_language();
    const char* variant = get_variant();
    size_t language_len = strlen(language);

    static plist_t language_table = NULL;
    if(language_table)
        return language_table;

    plist_t plist;
    plist_from_bin(get_languages_plist(), get_languages_plist_size(), &plist);
    if(!plist)
        return NULL;

    if(plist_get_node_type(plist) != PLIST_DICT)
    {
        plist_free(plist);
        return NULL;
    }

    language_table = plist_dict_get_item(plist, variant);
    if(language_table && plist_get_node_type(language_table) == PLIST_DICT)
        return language_table;
    else
        language_table = NULL;

    language_table = plist_dict_get_item(plist, language);
    if(language_table && plist_get_node_type(language_table) == PLIST_DICT)
        return language_table;
    else
        language_table = NULL;

    language_table = check_alias(plist, variant);
    if(language_table)
        return language_table;

    language_table = check_alias(plist, language);
    if(language_table)
        return language_table;

    plist_dict_iter iter = NULL;
    plist_dict_new_iter(plist, &iter);
    if(!iter)
    {
        plist_free(plist);
        return NULL;
    }

    char* key = NULL;
    plist_t val = NULL;
    plist_dict_next_item(plist, iter, &key, &val);
    while(key && val)
    {
        if(strncmp(key, language, language_len) == 0)
        {
            language_table = val;
            break;
        }

        free(key);
        key = NULL;
        val = NULL;
        plist_dict_next_item(plist, iter, &key, &val);
    }
    free(iter);

    if(language_table && plist_get_node_type(language_table) == PLIST_DICT)
        return language_table;
    else
        language_table = NULL;

    return NULL;
}

const char* localize(const char* str)
{
    map_t tls_localize_buffer = get_tls_localize_buffer();

    const char* localized = NULL;
    if(hashmap_get(tls_localize_buffer, str, (any_t*)&localized) == MAP_OK)
        return localized;

    plist_t language_table = get_language_table();
    if(!language_table)
        return str;

    plist_t str_node = plist_dict_get_item(language_table, str);
    if(!str_node)
        return str;

    char* str_value = NULL;
    if(plist_get_node_type(str_node) == PLIST_STRING)
        plist_get_string_val(str_node, &str_value);
    else if(plist_get_node_type(str_node) == PLIST_KEY) // WORK AROUND FOR LIBPLIST BUG WHEN KEY == VALUE
        plist_get_key_val(str_node, &str_value);

    if(!str_value)
        return str;

    hashmap_put(tls_localize_buffer, str, str_value);

    return str_value;
}
