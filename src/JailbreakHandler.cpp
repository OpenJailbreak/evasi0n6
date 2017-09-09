#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "JailbreakHandler.h"
#include "jailbreak.h"

static JailbreakHandler* self;

static void status_cb(const char* message, int progress, int attention)
{
	self->statusCallback(message, progress, attention);
}

JailbreakHandler::JailbreakHandler(DeviceHandler* devhandler)
	: devhandler(devhandler)
{
	self = this;
}

void JailbreakHandler::statusCallback(const char* message, int progress, int attention)
{
	devhandler->processStatus(message, progress, attention);
}

void* JailbreakHandler::Entry(void* data)
{
	char* udid = strdup(devhandler->getUDID());
	idevice_event_t event;
	event.udid = udid;
	event.event = IDEVICE_DEVICE_ADD;
	jb_device_event_cb(&event, udid);
	jailbreak(udid, status_cb);
	free(udid);

	const char* error = "Done!";

	devhandler->processFinished(error);
	return 0;
}

static void* thread_func(void* data)
{
	return self->Entry(data);
}

void JailbreakHandler::Start(void)
{
#ifdef WIN32
	this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_func, NULL, 0, NULL);
#else
	pthread_create(&this->thread, NULL, thread_func, NULL);
#endif
}
