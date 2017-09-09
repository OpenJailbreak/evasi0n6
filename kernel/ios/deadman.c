#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "wdt.h"

#define RB_AUTOBOOT 0
int reboot(int);

static pthread_t DeadmanThread = NULL;
static int DeadmanActive = 0;
static int DeadmanTimeRemaining = 0;
static pthread_mutex_t DeadmanMutex = PTHREAD_MUTEX_INITIALIZER;

#include "kiki-log.h"

static void deadman_function(void* arg)
{
    while(1)
    {
        sleep(1);
        pthread_mutex_lock(&DeadmanMutex);
        if(!DeadmanActive)
        {
            pthread_mutex_unlock(&DeadmanMutex);
            continue;
        }

        --DeadmanTimeRemaining;
        if(DeadmanTimeRemaining <= 0)
        {
            pthread_mutex_unlock(&DeadmanMutex);
            wdt_off();
            reboot(RB_AUTOBOOT);
            return;
        }
        pthread_mutex_unlock(&DeadmanMutex);
    }
}

void deadman_activate(int timeout)
{
    pthread_mutex_lock(&DeadmanMutex);

    if(!DeadmanActive)
    {
        DeadmanTimeRemaining = timeout;
        wdt_reset(timeout + 1);
    }

    ++DeadmanActive;

    if(!DeadmanThread)
        pthread_create(&DeadmanThread, NULL, (void*)deadman_function, NULL);

    pthread_mutex_unlock(&DeadmanMutex);
}

void deadman_reset(int timeout)
{
    pthread_mutex_lock(&DeadmanMutex);
    DeadmanTimeRemaining = timeout;
    wdt_reset(timeout + 1);
    pthread_mutex_unlock(&DeadmanMutex);
}

void deadman_deactivate()
{
    pthread_mutex_lock(&DeadmanMutex);
    if(DeadmanActive > 0)
    {
        --DeadmanActive;
    }
    if(DeadmanActive <= 0)
    {
        DeadmanActive = 0;
        wdt_off();
    }
    pthread_mutex_unlock(&DeadmanMutex);
}
