#include "bsdprocesslist.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>

int GetBSDProcessList(kinfo_proc **procList, size_t *procCount) /*{{{*/
{
	int err;
	kinfo_proc* result;
	bool done;
	static const int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t length;

	assert(procList != NULL);
	assert(*procList == NULL);
	assert(procCount != NULL);

	*procCount = 0;

	result = NULL;
	done = false;
	do {
		assert(result == NULL);

		length = 0;
		err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1, NULL, &length, NULL, 0);
		if (err == -1) {
			err = errno;
		}

		if (err == 0) {
			result = (kinfo_proc*)malloc(length);
			if (result == NULL) {
				err = ENOMEM;
			}
		}

		if (err == 0) {
			err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1, result, &length, NULL, 0);
			if (err == -1) {
				err = errno;
			}
			if (err == 0) {
				done = true;
			} else if (err == ENOMEM) {
				assert(result != NULL);
				free(result);
				result = NULL;
				err = 0;
			}
		}
	} while (err == 0 && ! done);

	if (err != 0 && result != NULL) {
		free(result);
		result = NULL;
	}
	*procList = result;
	if (err == 0) {
		*procCount = length / sizeof(kinfo_proc);
	}

	assert( (err == 0) == (*procList != NULL) );

	return err;
} /*}}}*/
