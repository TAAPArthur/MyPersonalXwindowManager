#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "communications.h"
#include "globals.h"
#include "settings.h"
#include "system.h"
#include "util/logger.h"
#include "wm-rules.h"
#include "wmfunctions.h"
#include "xevent.h"
#include "xutil/device-grab.h"
#include "xutil/xsession.h"

#ifdef DOXYGEN
#undef assert
#define assert(x)
#endif

/// if true, the event loop will be started else we wait for send confirmations and exit
static bool RUN_EVENT_LOOP = 1;
/**
 * Clear properties so we won't conflict with running WM
 */
static void clearWMSettings(void) {
    ROOT_EVENT_MASKS = ROOT_DEVICE_EVENT_MASKS = NON_ROOT_EVENT_MASKS = NON_ROOT_DEVICE_EVENT_MASKS = 0;
    startupMethod = NULL;
    RUN_AS_WM = 0;
}
static WindowID active = 0;

static void dumpOptions() {
    FOR_EACH(Option*, option, getOptions()) {
        printf("%s ", option->name);
    }
    exit(NORMAL_TERMINATION);
}
static void setWindow(WindowID win) { active = win;}
static void noEventLoop() {RUN_EVENT_LOOP = 0;}
static void replaceWM() {STEAL_WM_SELECTION = 1;}
static void dumpStartupOptions();
/// list of startup options
static Option options[] = {
    {"list-start-options", {dumpStartupOptions}},
    {"log-level", {setLogLevel}, .flags = VAR_SETTER | REQUEST_INT},
    {"list-options", {dumpOptions}},
    {"no-event-loop", {noEventLoop}},
    {"no-run-as-window-manager", {clearWMSettings}},
    {"replace", {replaceWM}},
    {"die-on-idle", {addShutdownOnIdleRule}},
    {"as", {setWindow}, .flags = REQUEST_INT},
};
static void dumpStartupOptions() {
    for(int i = 0; i < LEN(options); i++)
        printf("%s ", options[i].name);
    exit(NORMAL_TERMINATION);
}

/**
 * @param list list of options
 * @param argv
 * @param i index of argv; may be modified
 * @param varsOnly if true will only consider options with the VAR_SETTER flag
 *
 * @return true, if an option was found and called
 */
static inline bool callStartupOption(const char* const* argv, int* n) {
    const char* value;
    for(int i = 0; i < LEN(options); i++) {
        Option* option = &options[i];
        bool increment = 0;
        if(strcmp(argv[*n] + 2, option->name) == 0) {
            if(option->flags & REQUEST_INT) {
                value = argv[*n + 1];
                increment = 1;
            }
            else
                value = "";
            INFO("value '%s' %d %d", value, value[0], option->flags);
            if(matchesOption(option, option->name, !!value[0])) {
                callOption(option, value, NULL);
                if(increment)
                    *n += 1;
                return 1;
            }
        }
    }
    return 0;
}
/**
 * Parse command line arguments starting the 1st index
 * @param argc the number of args to parse
 * @param argv the list of args. An element by be like "-s", "--long", --long=V", etc
 */
static void parseArgs(int argc, const char* const* argv) {
    assert(argc > 1);
    for(int i = 1; i < argc; i++) {
        TRACE("Processing: %s", argv[i]);
        if(argv[i][0] == '-' && argv[i][1] == '-') {
            TRACE("processing %s", argv[i]);
            if(!callStartupOption(argv, &i)) {
                ERROR("Could not find matching options for %s.", argv[i]);
                exit(INVALID_OPTION);
            }
            continue;
        }
        DEBUG("Trying to send %s.", argv[i]);
        if(!findOption(argv[i], argv[i + 1], argv[i + 1] ? argv[i + 2] : NULL)) {
            ERROR("Could not find matching options for %s.", argv[i]);
            exit(INVALID_OPTION);
        }
        if(!hasXConnectionBeenOpened()) {
            openXDisplay();
            clearWMSettings();
            RUN_EVENT_LOOP = 0;
        }
        if(!isMPXManagerRunning())
            exit(WM_NOT_RESPONDING);
        noEventLoop();
        sendAs(argv[i], active, argv[i + 1], argv[i + 1] ? argv[i + 2] : NULL);
        break;
    }
}
void shutdownWhenNoOutsandingMessages() {
    if(!hasOutStandingMessages())
        requestShutdown();
}

void timeoutWaitingForRequests() {
    alarm(IDLE_TIMEOUT_CLI_SEC);
    if(hasOutStandingMessages()) {
        err(WM_NOT_RESPONDING, "WM did not confirm request(s)");
    }
}

int _main(int argc, const char* const* argv) {
    set_handlers();
    DEBUG("MPXManager started with %d args", argc);
    numPassedArguments = argc;
    passedArguments = argv;
    initOptions();
    if(!startupMethod)
        startupMethod = loadSettings;
    if(argc > 1)
        parseArgs(argc, argv);
    if(!hasXConnectionBeenOpened())
        onStartup();
    if(RUN_EVENT_LOOP)
        runEventLoop();
    else if(getNumberOfMessageSent()) {
        if(hasOutStandingMessages()) {
            TRACE("waiting for send receipts");
            registerForWindowEvents(getPrivateWindow(), XCB_EVENT_MASK_PROPERTY_CHANGE);
            addEvent(XCB_PROPERTY_NOTIFY, DEFAULT_EVENT(shutdownWhenNoOutsandingMessages));
            alarm(IDLE_TIMEOUT_CLI_SEC);
            createSigAction(SIGALRM, timeoutWaitingForRequests);
            runEventLoop();
            DEBUG("WM Running: %d; Outstanding messages: %d", isMPXManagerRunning(), hasOutStandingMessages());
        }
        //if(hasXConnectionBeenOpened()) return !getLastMessageExitCode();
    }
    return 0;
}
/**
 * @param argc
 * @param argv[]
 *
 * @return
 */
int __attribute__((weak)) main(int argc, const char* const argv[]) {
    setLogLevel(LOG_LEVEL_WARN);
    quit(_main(argc, argv));
}
#ifndef NDEBUG
int mainAlias(int argc, char* const argv[]) __attribute__((weak, alias("main")));
#endif
