#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include "DeviceHandler.h"
#include "JailbreakHandler.h"
#include "iTunesKiller.h"
#include "device_types.h"
#include "jailbreak.h"
#include "localize.h"

extern "C" {
typedef int16_t userpref_error_t;
extern userpref_error_t userpref_remove_device_public_key(const char *udid);
}

static int detection_blocked = 0;
static DeviceHandler* self;

static const char* getDeviceName(const char* productType)
{
	int i = 0;
	while (device_types[i].productType) {
		if (strcmp(device_types[i].productType, productType) == 0) {
			return device_types[i].displayName;
		}
		i++;
	}
	return productType;
}

static void device_event_cb(const idevice_event_t* event, void* userdata)
{
	if (!detection_blocked) {
		self->DeviceEventCB(event, userdata);
	}
	jb_device_event_cb(event, (void*)event->udid);
}

DeviceHandler::DeviceHandler(MainWnd* main)
	: mainWnd(main), device_count(0)
{
	self = this;

	this->current_udid = NULL;

	this->checkDevice();

	idevice_event_subscribe(&device_event_cb, NULL);
}

DeviceHandler::~DeviceHandler(void)
{
	idevice_event_unsubscribe();
	if (this->current_udid) {
		free(this->current_udid);
	}
}

void DeviceHandler::setUDID(const char* udid)
{
	if (this->current_udid) {
		free(this->current_udid);
		this->current_udid = NULL;
	}
	if (udid) {
		this->current_udid = strdup(udid);
	}
}

char* DeviceHandler::getUDID(void)
{
	return current_udid;
}

void DeviceHandler::DeviceEventCB(const idevice_event_t *event, void *user_data)
{
	if (event->event == IDEVICE_DEVICE_ADD) {
		this->device_count++;
		this->checkDevice();
	} else if (event->event == IDEVICE_DEVICE_REMOVE) {
		this->device_count--;
		this->checkDevice();
	}
}

void DeviceHandler::checkDevice()
{
	MainWnd* mainwnd = (MainWnd*)this->mainWnd;
	char str[256];

	this->setUDID(NULL);

	if (this->device_count == 0) {
		mainwnd->setButtonEnabled(0);
		mainwnd->setProgress(0);
		mainwnd->setStatusText(localize("Connect your iPhone, iPod touch, or iPad to begin."));
	} else if (this->device_count == 1) {
		idevice_t dev = NULL;
		idevice_error_t ierr = idevice_new(&dev, NULL);
		if (ierr != IDEVICE_E_SUCCESS) {
			sprintf(str, localize("Error detecting device type (idevice error %d)"), ierr);
			mainwnd->setStatusText(str);
			return;
		}

		lockdownd_client_t client = NULL;
		lockdownd_error_t lerr = lockdownd_client_new_with_handshake(dev, &client, NULL);
		if (lerr == LOCKDOWN_E_PASSWORD_PROTECTED) {
			lockdownd_client_free(client);
			idevice_free(dev);
			mainwnd->setStatusText(localize("ERROR: Device has a passcode set! If a passcode is set, the jailbreak procedure will most likely fail. Unplug device, go to Settings and DISABLE THE PASSCODE, then plug it back in."));
			return;
		} else if (lerr == LOCKDOWN_E_INVALID_HOST_ID) {
			lerr = lockdownd_unpair(client, NULL);
			if (lerr == LOCKDOWN_E_SUCCESS) {
				char *devudid = NULL;
				idevice_get_udid(dev, &devudid);
				if (devudid) {
					userpref_remove_device_public_key(devudid);
					free(devudid);
				}
			}
			lockdownd_client_free(client);
			idevice_free(dev);
			mainwnd->setStatusText(localize("Error detecting device. Try reconnecting it."));
			return;
		} else if (lerr == LOCKDOWN_E_SSL_ERROR) {
			lockdownd_client_t newl = NULL;
			lockdownd_client_new(dev, &newl, NULL);
			if (newl) {
				plist_t device_public_key = NULL;
				lockdownd_get_value(newl, NULL, "DevicePublicKey", &device_public_key);
				if (device_public_key && (plist_get_node_type(device_public_key) == PLIST_DATA)) {
					char* testdata = NULL;
					uint64_t testsize = 0;
					plist_get_data_val(device_public_key, &testdata, &testsize);
					const char chk[] = "-----BEGIN RSA PUBLIC KEY-----";
					if (memcmp(testdata, chk, strlen(chk)) == 0) {
						lerr = lockdownd_unpair(newl, NULL);
						if (lerr == LOCKDOWN_E_SUCCESS) {
							char *devudid = NULL;
							idevice_get_udid(dev, &devudid);
							if (devudid) {
								userpref_remove_device_public_key(devudid);
								free(devudid);
							}
						}
						lockdownd_client_free(newl);
						idevice_free(dev);
						mainwnd->setStatusText(localize("Error detecting device. Try reconnecting it."));
						return;
					}
				}
				lockdownd_client_free(newl);
			}
			idevice_free(dev);
			sprintf(str, localize("Error detecting device (lockdown error %d)"), lerr);
			mainwnd->setStatusText(str);
			return;
		} else if (lerr != LOCKDOWN_E_SUCCESS) {
			idevice_free(dev);
			sprintf(str, localize("Error detecting device (lockdown error %d)"), lerr);
			mainwnd->setStatusText(str);
			return;
		}

		plist_t node = NULL;
		char* productType = NULL;
		char* productVersion = NULL;
		char* buildVersion = NULL;

		node = NULL;
		lerr = lockdownd_get_value(client, NULL, "ProductType", &node);
		if (node) {
			plist_get_string_val(node, &productType);
			plist_free(node);
		}
		if ((lerr != LOCKDOWN_E_SUCCESS) || !productType) {
			lockdownd_client_free(client);
			idevice_free(dev);
			sprintf(str, localize("Error getting product type (lockdown error %d)"), lerr);
			mainwnd->setStatusText(str);
			return;
		}

		node = NULL;
		lerr = lockdownd_get_value(client, NULL, "ProductVersion", &node);
		if (node) {
			plist_get_string_val(node, &productVersion);
			plist_free(node);
		}
		if ((lerr != LOCKDOWN_E_SUCCESS) || !productVersion) {
			free(productType);
			lockdownd_client_free(client);
			idevice_free(dev);
			sprintf(str, localize("Error getting product version (lockdownd error %d)"), lerr);
			mainwnd->setStatusText(str);
			return;
		}

		node = NULL;
		lerr = lockdownd_get_value(client, NULL, "BuildVersion", &node);
		if (node) {
			plist_get_string_val(node, &buildVersion);
			plist_free(node);
		}
		if ((lerr != LOCKDOWN_E_SUCCESS) || !buildVersion) {
			free(productType);
			free(productVersion);
			lockdownd_client_free(client);
			idevice_free(dev);
			sprintf(str, localize("Error getting build version (lockdownd error %d)"), lerr);
			mainwnd->setStatusText(str);
			return;
		}

		if (!jb_device_is_supported(productType, buildVersion)) {
			mainwnd->setStatusText(localize("Sorry, the version of iOS of the attached device is not supported."));
			free(productType);
			free(productVersion);
			free(buildVersion);
			lockdownd_client_free(client);
			idevice_free(dev);
			return;
		}

		node = NULL;
		lockdownd_get_value(client, NULL, "PasswordProtected", &node);
		if (node) {
			uint8_t pcenabled = 0;
			plist_get_bool_val(node, &pcenabled);
			plist_free(node);
			if (pcenabled) {
				mainwnd->setStatusText(localize("ERROR: Device has a passcode set! If a passcode is set, the jailbreak procedure will most likely fail. Unplug device, go to Settings and DISABLE THE PASSCODE, then plug it back in."));
				return;
			}
		}

		// check for afc2
		uint16_t port = 0;
		lockdownd_start_service(client, "com.apple.afc2", &port);
		if (port != 0) {
			// device appears to be already jailbroken
			sprintf(str, localize("%s (iOS %s) is already jailbroken. Jailbreaking it again is NOT recommended."), getDeviceName(productType), productVersion);
		} else {
			sprintf(str, localize("%s (iOS %s) is supported. Click Jailbreak to begin."), getDeviceName(productType), productVersion);
		}
		mainwnd->setStatusText(str);

		int ready_to_go = 1;

		plist_t pl = NULL;
		lockdownd_get_value(client, NULL, "ActivationState", &pl);
		if (pl && plist_get_node_type(pl) == PLIST_STRING) {
			char* as = NULL;
			plist_get_string_val(pl, &as);
			plist_free(pl);
			if (as) {
				if (strcmp(as, "Unactivated") == 0) {
					ready_to_go = 0;
					mainwnd->msgBox(localize("The attached device is not activated. You need to activate it before you can apply the jailbreak."), localize("Error"), mb_OK | mb_ICON_ERROR);
				}
				free(as);
			}
		}

		pl = NULL;
		lockdownd_get_value(client, "com.apple.mobile.backup", "WillEncrypt", &pl);
		lockdownd_client_free(client);

		if (pl && plist_get_node_type(pl) == PLIST_BOOLEAN) {
			uint8_t c = 0;
			plist_get_bool_val(pl, &c);
			plist_free(pl);
			if (c) {
				ready_to_go = 0;
				mainwnd->msgBox(localize("The attached device has a backup password set. You need to disable the backup password in iTunes before you can continue.\nStart iTunes, remove the backup password and start this Program again."), localize("Error"), mb_OK | mb_ICON_ERROR);
			}
		}

		if (ready_to_go) {
			char* udid = NULL;
			idevice_get_udid(dev, &udid);
			if (udid) {
				this->setUDID(udid);
				free(udid);
			}
			mainwnd->setButtonEnabled(1);
		}
		idevice_free(dev);

		free(productType);
		free(productVersion);
		free(buildVersion);
	} else {
		mainwnd->setButtonEnabled(0);
		mainwnd->setStatusText(localize("Please attach only one device."));
	}
}

void DeviceHandler::processStart(void)
{
	MainWnd* mainwnd = (MainWnd*)this->mainWnd;

	detection_blocked = 1;
	mainwnd->closeBlocked = 1;

#if defined(__APPLE__) || defined(WIN32)
	iTunesKiller* ik = new iTunesKiller(&detection_blocked);
	ik->Start();
#endif

	JailbreakHandler* jb = new JailbreakHandler(this);
	jb->Start();
}

void DeviceHandler::processStatus(const char* msg, int progress, int attention)
{
	MainWnd* mainwnd = (MainWnd*)this->mainWnd;
	if (msg) {
		mainwnd->setStatusText(msg);
	}
	if (progress >= 0) {
		mainwnd->setProgress(progress);
	}
	if (attention > 0) {
		mainwnd->requestUserAttention(attention);
	} else {
		mainwnd->cancelUserAttention();
	}
}

void DeviceHandler::processFinished(const char* error)
{
	MainWnd* mainwnd = (MainWnd*)this->mainWnd;

	detection_blocked = 1;
	mainwnd->closeBlocked = 0;
	mainwnd->configureButtonForExit();
}
