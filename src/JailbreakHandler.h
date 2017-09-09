#ifndef __JAILBREAKHANDLER_H 
#define __JAILBREAKHANDLER_H

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "DeviceHandler.h"

class JailbreakHandler
{
private:
	DeviceHandler* devhandler;
#ifdef WIN32
	HANDLE thread;
#else
	pthread_t thread;
#endif

public:
	JailbreakHandler(DeviceHandler* devhandler);
	void Start(void);
	void statusCallback(const char* message, int progress, int attention);
	void* Entry(void* data);
};

#endif /* __JAILBREAKHANDLER_H */
