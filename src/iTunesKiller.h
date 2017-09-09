#ifndef __ITUNESKILLER_H 
#define __ITUNESKILLER_H
#if defined(__APPLE__) || defined(WIN32)

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

class iTunesKiller
{
private:
	int* watchit;
#ifdef WIN32
	HANDLE thread;
#else
	pthread_t thread;
#endif

public:
	iTunesKiller(int* watchdog);
	void Start(void);
	void* Entry(void* data);
};
#endif
#endif /* __ITUNESKILLER_H */
