#include <string.h>
#include <stdlib.h>

#include "cpio.h"

unsigned int cpio_get_namesize(cpio_record_t *record)
{
	char buf[8];
	if (!record) {
		return 0;
	}
	memcpy(buf, record->namesize, 6);
	buf[6] = '\0';

	char* ep = NULL;
	unsigned int res = strtoul(buf, &ep, 8);
	if (!ep || (strlen(ep) == 0)) {
		return res;
	} else {
		return 0;
	}
}

unsigned long long cpio_get_filesize(cpio_record_t *record)
{
	char buf[12];
	if (!record) {
		return 0;
	}
	memcpy(buf, record->filesize, 11);
	buf[11] = '\0';

	char* ep = NULL;
	unsigned int res = strtoull(buf, &ep, 8);
	if (!ep || (strlen(ep) == 0)) {
		return res;
	} else {
		return 0;
	}
}
