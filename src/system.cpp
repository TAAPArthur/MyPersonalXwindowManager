
/**
 * @file system.cpp
 * @copybrief system.h
 */



#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "arraylist.h"
#include "compatibility.h"
#include "debug.h"
#include "globals.h"
#include "logger.h"
#include "masters.h"
#include "monitors.h"
#include "mywm-structs.h"
#include "system.h"
#include "windows.h"
#include "workspaces.h"
#include "workspaces.h"

int statusPipeFD[4] = {0};
int numPassedArguments;
char* const* passedArguments;

static volatile int shuttingDown = 0;
void requestShutdown(void) {
    shuttingDown = 1;
}
int isShuttingDown(void) {
    return shuttingDown;
}


void (*onChildSpawn)(void) = setClientMasterEnvVar;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int trylock(void) {
    return pthread_mutex_trylock(&mutex);
}
void lock(void) {
    pthread_mutex_lock(&mutex);
}
void unlock(void) {
    pthread_mutex_unlock(&mutex);
}

/// holds thread metadata
typedef struct {
    /// pthread id
    pthread_t thread;
    /// user specified name of thread
    const char* name;
} Thread;
static ArrayList<Thread*> threads __attribute__((unused)) ;
void runInNewThread(void* (*method)(void*)__attribute__((unused)), void* arg __attribute__((unused)),
    const char* name) {
    if(isShuttingDown())return;
    pthread_t thread;
    int result __attribute__((unused)) = pthread_create(&thread, NULL, method, arg) == 0;
    assert(result);
    assert(sizeof(thread) <= sizeof(void*));
    Thread* t = new Thread{thread, name};
    threads.add(t);
}
int getNumberOfThreads(void) {return threads.size();}
void waitForAllThreadsToExit(void) {
    pthread_t self = pthread_self();
    bool relock = 0;
    // if already locked, set relock, else lock
    if(trylock()) {
        relock = 1;
    }
    unlock();
    LOG(LOG_LEVEL_INFO, "Thread %ld is waiting on %d threads\n", self, threads.size());
    while(threads.size()) {
        Thread* thread = threads.pop();
        LOG(LOG_LEVEL_INFO, "Waiting for thread '%s' and %d more threads\n", thread->name, threads.size());
        if(thread->thread != self) {
            int result __attribute__((unused)) = pthread_join(thread->thread, NULL);
            assert(result == 0);
        }
        delete thread;
    }
    LOG(LOG_LEVEL_INFO, "Finished waiting on threads\n");
    if(relock)
        lock();
}
void resetPipe() {
    if(statusPipeFD[0]) {
        // other fields were closed right after a call to spawnPipe;
        LOG(LOG_LEVEL_TRACE, "reseting pipe\n");
        close(STATUS_FD);
        close(STATUS_FD_READ);
        for(int i = LEN(statusPipeFD) - 1; i >= 0; i--)
            statusPipeFD[i] = 0;
    }
}
static inline void setEnvRect(const char* name, const Rect& rect) {
    const char var[4][32] = {"_%s_X", "_%s_Y", "_%s_WIDTH", "_%s_HEIGHT"};
    char strName[32];
    char strValue[8];
    for(int n = 0; n < 4; n++) {
        sprintf(strName, var[n], name);
        sprintf(strValue, "%d", rect[n]);
        setenv(strName, strValue, 1);
    }
}
void setClientMasterEnvVar(void) {
    char strValue[8];
    if(getActiveMaster()) {
        sprintf(strValue, "%d", getActiveMasterKeyboardID());
        setenv(DEFAULT_KEYBOARD_ENV_VAR_NAME, strValue, 1);
        sprintf(strValue, "%d", getActiveMasterPointerID());
        setenv(DEFAULT_POINTER_ENV_VAR_NAME, strValue, 1);
        if(getFocusedWindow()) {
            sprintf(strValue, "%d", getFocusedWindow()->getID());
            setenv("_WIN_ID", strValue, 1);
        }
        Monitor* m = getActiveWorkspace()->getMonitor();
        if(getFocusedWindow())
            setEnvRect("WIN", getFocusedWindow()->getGeometry());
        if(m) {
            setEnvRect("VIEW", m->getViewport());
            setEnvRect("MON", m->getBase());
        }
        const Rect rootBounds = {0, 0, getRootWidth(), getRootHeight()};
        setEnvRect("ROOT", rootBounds);
        if(LD_PRELOAD_INJECTION)
            setenv("LD_PRELOAD", LD_PRELOAD_PATH.c_str(), 1);
    }
}

static int _spawn(const char* command, bool spawnPipe, bool dup = 1) {
    LOG(LOG_LEVEL_INFO, "running command %s\n", command);
    if(spawnPipe) {
        resetPipe();
        pipe(statusPipeFD);
        pipe(statusPipeFD + 2);
    }
    int pid = fork();
    if(pid == 0) {
        if(spawnPipe) {
            close(STATUS_FD);
            close(STATUS_FD_READ);
            if(dup) {
                dup2(STATUS_FD_EXTERNAL_READ, STDIN_FILENO);
                dup2(STATUS_FD_EXTERNAL_WRITE, STDOUT_FILENO);
            }
        }
        else
            resetPipe();
        if(onChildSpawn)
            onChildSpawn();
        if(command == NULL)
            return 0;
        execl(SHELL.c_str(), SHELL.c_str(), "-c", command, (char*)0);
        err(1, "exec failed; Aborting");
    }
    else if(pid < 0)
        err(1, "error forking\n");
    if(spawnPipe) {
        close(STATUS_FD_EXTERNAL_READ);
        close(STATUS_FD_EXTERNAL_WRITE);
    }
    return pid;
}
int spawn(const char* command) {
    return _spawn(command, 0);
}
int spawnPipe(const char* command, bool dup) {
    return _spawn(command, 1, dup);
}

int waitForChild(int pid) {
    LOG(LOG_LEVEL_DEBUG, "Waiting for process: %d\n", pid);
    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status) : -1;
    LOG(LOG_LEVEL_DEBUG, "Process exited with %d status %d\n", pid, status);
    return exitCode;
}
void clearAllLists() {
    setActiveMaster(NULL);
    getAllSlaves().deleteElements();
    getAllMasters().deleteElements();
    removeAllWorkspaces();
    getAllWindows().deleteElements();
    getAllMonitors().deleteElements();
}
static void stop(void) {
    requestShutdown();
    waitForAllThreadsToExit();
    LOG(LOG_LEVEL_INFO, "Shutting down\n");
    resetPipe();
    clearAllLists();
}

void restart(void) {
    if(passedArguments) {
        LOG(LOG_LEVEL_INFO, "restarting\n");
        stop();
        LOG(LOG_LEVEL_INFO, "calling execv\n");
        execv(passedArguments[0], passedArguments);
        err(1, "exec failed; Aborting");
    }
    else
        quit(2);
}
void quit(int exitCode) {
    stop();
    LOG(LOG_LEVEL_INFO, "Exiting\n");
    exit(exitCode);
}

static void handler(int sig) {
    LOG(LOG_LEVEL_ERROR, "Error: signal %d:\n", sig);
    printStackTrace();
    printSummary();
    validate();
    if(sig == SIGSEGV || sig == SIGABRT)
        exit(sig);
    quit(sig);
}

__attribute__((constructor)) static void set_handlers() {
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);
    signal(SIGTERM, handler);
    signal(SIGPIPE, [](int) {resetPipe();});
    signal(SIGUSR1, [](int) {restart();});
    signal(SIGUSR2, [](int) {printStackTrace();});
}
