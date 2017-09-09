#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <plist/plist.h>

#include "jailbreak.h"

extern uint16_t userpref_remove_device_public_key(const char* udid);

static const char* lastmsg = NULL;
static void status_cb(const char* msg, int progress, int attention)
{
        if (!msg) {
                msg = lastmsg;
        } else {
                lastmsg = msg;
        }
	if (attention) {
		printf("********\n");
	}
	if (progress == 0) {
		printf("%s\n", msg);
	} else if (progress == -1) {
		if (msg) {
			printf("%s\n", msg);
		}
	} else {
		printf("[%d%%] %s\n", progress, msg);
	}
}

static void idevice_event_cb(const idevice_event_t *event, void* userdata)
{
	jb_device_event_cb(event, userdata);
}

static void signal_handler(int sig)
{
        jb_signal_handler(sig);
}

int main(int argc, char** argv)
{
	char* udid = NULL;
	if (argc == 2) {
		if (strlen(argv[1]) != 40) {
			char* name = strrchr(argv[0], '/');
			printf("usage: %s [UDID]\n", (name) ? name+1 : argv[0]);
			return -1;
		}
		udid = argv[1];
	}

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
#ifndef WIN32   
        signal(SIGQUIT, signal_handler);
        signal(SIGPIPE, SIG_IGN);
#endif

	idevice_t device = NULL;
	if (!udid) {
		idevice_new(&device, NULL);
		if (!device) {
			fprintf(stderr, "No device found!\n");
			return -1;
		}
		idevice_get_udid(device, &udid);
	} else {
		idevice_new(&device, udid);
		if (!device) {
			fprintf(stderr, "Device %s not found!\n", udid);
			return -1;
		}
		udid = strdup(udid);
	}

	lockdownd_client_t lockdown = NULL;
	lockdownd_error_t lerr = lockdownd_client_new_with_handshake(device, &lockdown, NULL);
	if (lerr == LOCKDOWN_E_PASSWORD_PROTECTED) {
		lockdownd_client_free(lockdown);
		idevice_free(device);
		printf("ERROR: Device has a passcode set! If a passcode is set, the jailbreak procedure will most likely fail. Unplug device, go to Settings and DISABLE THE PASSCODE, then plug it back in.\n");
		return -1;
	} else if ((lerr == LOCKDOWN_E_INVALID_HOST_ID) || (lerr= LOCKDOWN_E_SSL_ERROR)) {
		lerr = lockdownd_unpair(lockdown, NULL);
		if (lerr == LOCKDOWN_E_SUCCESS) {
			userpref_remove_device_public_key(udid);
		}
		lockdownd_client_free(lockdown);
		lockdown = NULL;
		lerr = lockdownd_client_new_with_handshake(device, &lockdown, NULL);	
	}
	if (lerr != LOCKDOWN_E_SUCCESS) {
		printf("Could not connect to lockdown (%d)\n", lerr);
		idevice_free(device);
		return -1;
	}

	plist_t node = NULL;
	char* productType = NULL;
	char* productVersion = NULL;
	char* buildVersion = NULL;

	node = NULL;
	lerr = lockdownd_get_value(lockdown, NULL, "ProductType", &node);
	if (node) {
		plist_get_string_val(node, &productType);
		plist_free(node);
	}
	if ((lerr != LOCKDOWN_E_SUCCESS) || !productType) {
		lockdownd_client_free(lockdown);
		idevice_free(device);
		fprintf(stderr, "Error getting product type (lockdown error %d)\n", lerr);
		return -1;
	}

	node = NULL;
	lerr = lockdownd_get_value(lockdown, NULL, "ProductVersion", &node);
	if (node) {
		plist_get_string_val(node, &productVersion);
		plist_free(node);
	}
	if ((lerr != LOCKDOWN_E_SUCCESS) || !productVersion) {
		free(productType);
		lockdownd_client_free(lockdown);
		idevice_free(device);
		fprintf(stderr, "Error getting product version (lockdownd error %d)\n", lerr);
		return -1;
	}

	node = NULL;
	lerr = lockdownd_get_value(lockdown, NULL, "BuildVersion", &node);
	if (node) {
		plist_get_string_val(node, &buildVersion);
		plist_free(node);
	}
	if ((lerr != LOCKDOWN_E_SUCCESS) || !buildVersion) {
		free(productType);
		free(productVersion);
		lockdownd_client_free(lockdown);
		idevice_free(device);
		fprintf(stderr, "Error getting build version (lockdownd error %d)\n", lerr);
		return -1;
	}

	if (!jb_device_is_supported(productType, buildVersion)) {
		printf("Sorry, the attached device is not supported.\n");
		free(productType);
		free(productVersion);
		free(buildVersion);
		lockdownd_client_free(lockdown);
		idevice_free(device);
		return -1;
	}

	node = NULL;
	lockdownd_get_value(lockdown, NULL, "ActivationState", &node);
	if (node && plist_get_node_type(node) == PLIST_STRING) {
		char* as = NULL;
		plist_get_string_val(node, &as);
		plist_free(node);
		if (as) {
			if (strcmp(as, "Unactivated") == 0) {
				free(as);
				fprintf(stderr, "Error: The attached device is not activated. You need to activate it before it can be used with Absinthe.\n");
				lockdownd_client_free(lockdown);
				idevice_free(device);
				return -1;
			}
			free(as);
		}
	}

	node = NULL;
	lockdownd_get_value(lockdown, "com.apple.mobile.backup", "WillEncrypt", &node);
	if (node && plist_get_node_type(node) == PLIST_BOOLEAN) {
		unsigned char c = 0;
		plist_get_bool_val(node, &c);
		plist_free(node);
		if (c) {
			fprintf(stderr, "Error: You have a device backup password set. You need to disable the backup password in iTunes.\n");
			lockdownd_client_free(lockdown);
			idevice_free(device);
			return -1;
		}
	}
	lockdownd_client_free(lockdown);
	idevice_free(device);
	device = NULL;

	idevice_event_subscribe(idevice_event_cb, udid);
	jailbreak(udid, status_cb);
	idevice_event_unsubscribe();

	free(udid);

	return 0;
}
