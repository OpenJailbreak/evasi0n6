#if defined(__APPLE__) || defined(WIN32)
#include <stdio.h>
#include "iTunesKiller.h"

#include "debug.h"

#if defined(__APPLE__)
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>

#include "bsdprocesslist.h"

#endif

#if defined(WIN32)
#include <windows.h>
#include <tlhelp32.h>
#define sleep(x) Sleep(x*1000)
#endif

static iTunesKiller* self;

iTunesKiller::iTunesKiller(int* watchdog)
	: watchit(watchdog)
{
	self = this;
	debug("ITK:init\n");
}

void* iTunesKiller::Entry(void* data)
{
#if defined(WIN32)
	PROCESSENTRY32 pe;
#endif
	while (*(this->watchit)) {
#if defined(__APPLE__)
		size_t proc_count = 0;
		kinfo_proc *proc_list = NULL;
		GetBSDProcessList(&proc_list, &proc_count);
		size_t i;
		for (i = 0; i < proc_count; i++) {
			if ((!strcmp((&proc_list[i])->kp_proc.p_comm, "iTunesHelper")) || (!strcmp((&proc_list[i])->kp_proc.p_comm, "iTunes"))) {
				kill((&proc_list[i])->kp_proc.p_pid, SIGKILL);
			}
		}
		free(proc_list);
#endif
#if defined(WIN32)
		memset(&pe, 0, sizeof(pe));
		pe.dwSize = sizeof(PROCESSENTRY32);

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			sleep(2);
			continue;
		}

		BOOL i = Process32First(hSnapshot, &pe);
		while (i) {
			if (!wcscmp(pe.szExeFile, L"iTunesHelper.exe") || !wcscmp(pe.szExeFile, L"iTunes.exe")) {
				HANDLE p = OpenProcess(PROCESS_ALL_ACCESS, 0, pe.th32ProcessID);
				if (p != INVALID_HANDLE_VALUE) {
					TerminateProcess(p, 0);
					CloseHandle(p);
				}
			}
			i = Process32Next(hSnapshot, &pe);
		}
		CloseHandle(hSnapshot);
#endif
		sleep(2);
	}
	debug("ITK:Exiting.\n");
	return 0;
}

static void* thread_func(void* data)
{
	return self->Entry(data);
}

void iTunesKiller::Start(void)
{
#ifdef WIN32
	this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_func, NULL, 0, NULL);
#else
	pthread_create(&this->thread, NULL, thread_func, NULL);
#endif
}
#endif
