/**
  * GreenPois0n Apparition - mbdb_record.c
  * Copyright (C) 2010 Chronic-Dev Team
  * Copyright (C) 2010 Joshua Hill
  * Copyright (C) 2012 Hanéne Samara
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "backup.h"
#include "debug.h"
#include "file.h"

backup_t* backup_open(const char* backupdir, const char* udid)
{
	if (!backupdir || !udid) {
		return NULL;
	}

	char *backup_path = (char*)malloc(strlen(backupdir)+1+strlen(udid)+1+4);
	strcpy(backup_path, backupdir);
	strcat(backup_path, "/");
	strcat(backup_path, udid);

	char *mbdb_path = (char*)malloc(strlen(backup_path)+1+strlen("Manifest.mbdb")+1+4);
	strcpy(mbdb_path, backup_path);
	strcat(mbdb_path, "/");
	strcat(mbdb_path, "Manifest.mbdb");

	mbdb_t* mbdb = mbdb_open(mbdb_path);
	if (mbdb) {
		debug("Manifest.mbdb opened, %d records\n", mbdb->num_records);
	} else {
		error("ERROR: could not open %s\n", mbdb_path);
		free(mbdb_path);
		return NULL;
	}
	free(mbdb_path);

	backup_t* backup = (backup_t*)malloc(sizeof(backup_t));
        if (backup == NULL) {
		free(mbdb);
                return NULL;
        }
	memset(backup, '\0', sizeof(backup_t));

	backup->mbdb = mbdb;
	backup->path = backup_path;

	return backup;
}

int backup_get_file_index(backup_t* backup, const char* domain, const char* path)
{
	if (!backup || !backup->mbdb) {
		return -1;
	}
	int i = 0;
	int found = 0;
	mbdb_record_t* rec = NULL;
	for (i = 0; i < backup->mbdb->num_records; i++) {
		rec = backup->mbdb->records[i];
		if (rec->domain && !strcmp(rec->domain, domain) && rec->path && !strcmp(rec->path, path)) {
			found = 1;
			break;
		}
	}
	return (found) ? i : -1;
}

backup_file_t* backup_get_file(backup_t* backup, const char* domain, const char* path)
{
	if (!backup || !backup->mbdb) {
		return NULL;
	}
	int idx = backup_get_file_index(backup, domain, path);
	if (idx < 0) {
		// not found
		return NULL;
	}
	mbdb_record_t* rec = backup->mbdb->records[idx];
	return backup_file_create_from_record(rec);
}

char* backup_get_file_path(backup_t* backup, backup_file_t* bfile)
{
	if (!backup || !bfile) {
		return NULL;
	}
	if (!backup->mbdb) {
		error("%s: ERROR: no mbdb in given backup_t\n", __func__);
		return NULL;
	}

	char* bfntmp = (char*)malloc(bfile->mbdb_record->domain_size + 1 + bfile->mbdb_record->path_size+1+4);
	strcpy(bfntmp, bfile->mbdb_record->domain);
	strcat(bfntmp, "-");
	strcat(bfntmp, bfile->mbdb_record->path);

	char* backupfname = (char*)malloc(strlen(backup->path)+1+40+1);
	unsigned char sha1[20] = {0, };
	SHA1((unsigned char*)bfntmp, strlen(bfntmp), sha1);
	free(bfntmp);

	strcpy(backupfname, backup->path);
	strcat(backupfname, "/");

	int i;
	char* p = backupfname + strlen(backup->path) + 1;
	for (i = 0; i < 20; i++) {
		sprintf(p + i*2, "%02x", sha1[i]);
	}

	debug("backup filename is %s\n", backupfname);

	return backupfname;
}

int backup_update_file(backup_t* backup, backup_file_t* bfile)
{
	int res = 0;

	if (!backup || !bfile) {
		return -1;
	}
	if (!backup->mbdb) {
		error("%s: ERROR: no mbdb in given backup_t\n", __func__);
		return -1;
	}

	unsigned char* rec = NULL;
	unsigned int rec_size = 0;

	if (backup_file_get_record_data(bfile, &rec, &rec_size) < 0) {
		error("%s: ERROR: could not build mbdb_record data\n", __func__);
		return -1;
	}

	unsigned int newsize = 0;
	unsigned char* newdata = NULL;

	// find record
	int idx = backup_get_file_index(backup, bfile->mbdb_record->domain, bfile->mbdb_record->path);
	if (idx < 0) {
		// append record to mbdb
		newsize = backup->mbdb->size + rec_size;
		newdata = (unsigned char*)malloc(newsize);

		memcpy(newdata, backup->mbdb->data, backup->mbdb->size);
		memcpy(newdata+backup->mbdb->size, rec, rec_size);
	} else {
		// update record in mbdb
		backup_file_t* oldfile = backup_file_create_from_record(backup->mbdb->records[idx]);
		unsigned int oldsize = oldfile->mbdb_record->this_size;
		backup_file_free(oldfile);

		newsize = backup->mbdb->size - oldsize + rec_size;
		newdata = (unsigned char*)malloc(newsize);

		char* p = (char*)newdata;
		memcpy(p, backup->mbdb->data, sizeof(mbdb_header_t));
		p+=sizeof(mbdb_header_t);

		mbdb_record_t* r;
		unsigned char* rd;
		unsigned int rs;
		int i;

		for (i = 0; i < idx; i++) {
			r = backup->mbdb->records[i];
			rd = NULL;
			rs = 0;
			mbdb_record_build(r, &rd, &rs);
			memcpy(p, rd, rs);
			free(rd);
			p+=rs;
		}
		memcpy(p, rec, rec_size);
		p+=rec_size;
		for (i = idx+1; i < backup->mbdb->num_records; i++) {
			r = backup->mbdb->records[i];
			rd = NULL;
			rs = 0;
			mbdb_record_build(r, &rd, &rs);
			memcpy(p, rd, rs);
			free(rd);
			p+=rs;
		}
	}

	if (!newdata) {
		error("Uh, could not re-create mbdb data?!\n");
		return -1;
	}

	mbdb_free(backup->mbdb);
	free(rec);

	// parse the new data
	backup->mbdb = mbdb_parse(newdata, newsize);
	free(newdata);

	// write out the file data
	char* bfntmp = (char*)malloc(bfile->mbdb_record->domain_size + 1 + bfile->mbdb_record->path_size+1+4);
	strcpy(bfntmp, bfile->mbdb_record->domain);
	strcat(bfntmp, "-");
	strcat(bfntmp, bfile->mbdb_record->path);

	char* backupfname = (char*)malloc(strlen(backup->path)+1+40+1);
	unsigned char sha1[20] = {0, };
	SHA1((unsigned char*)bfntmp, strlen(bfntmp), sha1);
	free(bfntmp);

	strcpy(backupfname, backup->path);
	strcat(backupfname, "/");

	int i;
	char* p = backupfname + strlen(backup->path) + 1;
	for (i = 0; i < 20; i++) {
		sprintf(p + i*2, "%02x", sha1[i]);
	}

	debug("backup filename is %s\n", backupfname);

	if (bfile->filepath) {
		// copy file to backup dir
		if (file_copy(bfile->filepath, backupfname) < 0) {
			error("%s: ERROR: could not copy file '%s' to '%s'\n", __func__, bfile->filepath, backupfname);
			res = -1;
		}
	} else if (bfile->data) {
		// write data buffer to file
		if (file_write(backupfname, bfile->data, bfile->size) < 0) {
			error("%s: ERROR: could not write to '%s'\n", __func__, backupfname);
			res = -1;
		}
	} else if ((bfile->mbdb_record->mode) & 040000) {
		// directory!
	} else if ((bfile->mbdb_record->mode) & 020000) {
		// symlink!
	} else {
		debug("%s: WARNING: file data not updated, no filename or data given\n", __func__);
	}

	free(backupfname);

	return res;
}

int backup_remove_file(backup_t* backup, backup_file_t* bfile)
{
	int res = 0;

	if (!backup || !bfile) {
		return -1;
	}
	if (!backup->mbdb) {
		error("%s: ERROR: no mbdb in given backup_t\n", __func__);
		return -1;
	}

	unsigned int newsize = 0;
	unsigned char* newdata = NULL;

	// find record
	int idx = backup_get_file_index(backup, bfile->mbdb_record->domain, bfile->mbdb_record->path);
	if (idx < 0) {
		debug("file %s-%s not found in backup so not removed.\n", bfile->mbdb_record->domain, bfile->mbdb_record->path);
		return -1;
	} else {
		// remove record from mbdb
		backup_file_t* oldfile = backup_file_create_from_record(backup->mbdb->records[idx]);
		unsigned int oldsize = oldfile->mbdb_record->this_size;
		backup_file_free(oldfile);

		newsize = backup->mbdb->size - oldsize;
		newdata = (unsigned char*)malloc(newsize);

		char* p = (char*)newdata;
		memcpy(p, backup->mbdb->data, sizeof(mbdb_header_t));
		p+=sizeof(mbdb_header_t);

		mbdb_record_t* r;
		unsigned char* rd;
		unsigned int rs;
		int i;

		for (i = 0; i < idx; i++) {
			r = backup->mbdb->records[i];
			rd = NULL;
			rs = 0;
			mbdb_record_build(r, &rd, &rs);
			memcpy(p, rd, rs);
			free(rd);
			p+=rs;
		}
		for (i = idx+1; i < backup->mbdb->num_records; i++) {
			r = backup->mbdb->records[i];
			rd = NULL;
			rs = 0;
			mbdb_record_build(r, &rd, &rs);
			memcpy(p, rd, rs);
			free(rd);
			p+=rs;
		}
	}

	if (!newdata) {
		error("Uh, could not re-create mbdb data?!\n");
		return -1;
	}

	mbdb_free(backup->mbdb);

	// parse the new data
	backup->mbdb = mbdb_parse(newdata, newsize);
	free(newdata);

	// write out the file data
	char* bfntmp = (char*)malloc(bfile->mbdb_record->domain_size + 1 + bfile->mbdb_record->path_size+1+4);
	strcpy(bfntmp, bfile->mbdb_record->domain);
	strcat(bfntmp, "-");
	strcat(bfntmp, bfile->mbdb_record->path);

	char* backupfname = (char*)malloc(strlen(backup->path)+1+40+1);
	unsigned char sha1[20] = {0, };
	SHA1((unsigned char*)bfntmp, strlen(bfntmp), sha1);
	free(bfntmp);

	strcpy(backupfname, backup->path);
	strcat(backupfname, "/");

	int i;
	char* p = backupfname + strlen(backup->path) + 1;
	for (i = 0; i < 20; i++) {
		sprintf(p + i*2, "%02x", sha1[i]);
	}

	if (!(bfile->mbdb_record->mode & 040000)) {
		debug("deleting file %s\n", backupfname);
		remove(backupfname);
	}

	free(backupfname);

	return res;
}

int backup_write_mbdb(backup_t* backup)
{
	if (!backup || !backup->path || !backup->mbdb) {
		return -1;
	}

	char *mbdb_path = (char*)malloc(strlen(backup->path)+1+strlen("Manifest.mbdb")+1);
	strcpy(mbdb_path, backup->path);
	strcat(mbdb_path, "/");
	strcat(mbdb_path, "Manifest.mbdb");

	int res = file_write(mbdb_path, backup->mbdb->data, backup->mbdb->size);
	free(mbdb_path);
	return res;
}

void backup_free(backup_t* backup)
{
	if (backup) {
		if (backup->mbdb) {
			mbdb_free(backup->mbdb);
		}
		if (backup->path) {
			free(backup->path);
		}
		free(backup);
	}
}
