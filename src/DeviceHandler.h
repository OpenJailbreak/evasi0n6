#ifndef __DEVICEHANDLER_H
#define __DEVICEHANDLER_H

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include "MainWnd.h"

class MainWnd;

class DeviceHandler
{
private:
	MainWnd* mainWnd;
	int device_count;
	char* current_udid;
public:
	DeviceHandler(MainWnd* main);
	~DeviceHandler(void);
	void setUDID(const char* udid);
	char* getUDID();
	void DeviceEventCB(const idevice_event_t *event, void *user_data);
	void checkDevice();
	void processStart(void);
	void processStatus(const char* msg, int progress, int attention);
	void processFinished(const char* error);
};

#endif /* __DEVICEHANDLER_H */
