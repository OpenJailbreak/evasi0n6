#include <stdio.h>
#include <syslog.h>
#include <sys/time.h>

static char log_path[PATH_MAX];

void jb_log(const char* format, ...)
{
    static FILE* f = NULL;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    if(!f)
    {
        snprintf(log_path, sizeof(log_path), "/private/var/mobile/Media/jailbreak-%lu.log", tv.tv_sec);
        f = fopen(log_path, "wb");
    }

    fprintf(f, "[%lu.%llu] ", tv.tv_sec, (unsigned long long)tv.tv_usec);

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    vsyslog(LOG_EMERG, format, args);
    va_end(args);

    if(format[strlen(format) - 1] != '\n')
        fprintf(f, "\n");

    fflush(f);
    sync();
}

void jb_end_log()
{
    unlink("/private/var/mobile/Media/jailbreak.log");
    rename(log_path, "/private/var/mobile/Media/jailbreak.log");
}

