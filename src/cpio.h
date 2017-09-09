#ifndef __CPIO_H
#define __CPIO_H

typedef struct {
	unsigned char magic[6];
	unsigned char dev[6];
	unsigned char ino[6];
	unsigned char mode[6];
	unsigned char uid[6];
	unsigned char gid[6];
	unsigned char nlink[6];
	unsigned char rdev[6];
	unsigned char mtime[11];
	unsigned char namesize[6];
	unsigned char filesize[11];
} cpio_record_t;

unsigned int cpio_get_namesize(cpio_record_t *record);
unsigned long long cpio_get_filesize(cpio_record_t *record);

#endif
