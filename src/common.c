#include "common.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgen.h>
#include <errno.h>

#ifdef WIN32
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#endif

int __mkdir(const char* path, int mode)
{
#ifdef WIN32
	return _mkdir(path);
#else
	return mkdir(path, mode);
#endif
}

int mkdir_with_parents(const char *dir, int mode)
{
	if (!dir) return -1;
	if (__mkdir(dir, mode) == 0) {
		return 0;
	} else {
		if (errno == EEXIST) return 0;	
	}
	int res;
	char *parent = strdup(dir);
	char* parentdir = dirname(parent);
	if (parentdir) {
		res = mkdir_with_parents(parentdir, mode);
	} else {
		res = -1;	
	}
	free(parent);
	if (res == 0) {
		mkdir_with_parents(dir, mode);
	}
	return res;
}

char* build_path(const char* elem, ...)
{
	if (!elem) return NULL;
	va_list args;
	int len = strlen(elem)+1;
	va_start(args, elem);
	char *arg = va_arg(args, char*);
	while (arg) {
		len += strlen(arg)+1;
		arg = va_arg(args, char*);
	}
	va_end(args);

	char* out = (char*)malloc(len);
	strcpy(out, elem);

	va_start(args, elem);
	arg = va_arg(args, char*);
	while (arg) {
		strcat(out, "/");
		strcat(out, arg);
		arg = va_arg(args, char*);
	}
	va_end(args);
	return out;
}
