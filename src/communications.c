#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bindings.h"
#include "communications.h"
#include "devices.h"
#include "functions.h"
#include "layouts.h"
#include "system.h"
#include "util/debug.h"
#include "util/logger.h"
#include "windows.h"
#include "wmfunctions.h"
#include "xevent.h"
#include "xutil/window-properties.h"
#include "xutil/xsession.h"


/**
 *
 * @param str the str to check
 * @param isNumeric if str can be converted to an int
 *
 * @return the interger if isNumeric is set to 1
 */
static int isInt(const char* str, bool* isNumeric) {
    char* end;
    int num = strtol(str, &end, 10);
    if(*end) {
        num = strtol(str, &end, 16);
    }
    if(isNumeric)
        *isNumeric = (*end == 0) && str[0];
    return num;
}

bool matchesOption(Option* o, const char* str, char numOptions) {
    if(strcasecmp(o->name, str) == 0) {
        if(o->flags & REQUEST_MULTI)
            return numOptions == 2;
        else if(o->flags & (REQUEST_STR | REQUEST_INT))
            return numOptions == 1;
        else
            return numOptions == 0;
    }
    return 0;
}
void callOption(const Option* o, const char* p, const char* p2) {
    TRACE("Calling %s %s", o->name, p);
    uint32_t i = isInt(p, NULL);
    const BindingFunc* bindingFunc = &o->bindingFunc;
    if(o->flags & REQUEST_INT) {
        o->bindingFunc.func(i, bindingFunc->arg);
    }
    else if(o->flags & REQUEST_STR) {
        o->bindingFunc.func(p, p2 ? p2 : bindingFunc->arg.str);
    }
    else {
        o->bindingFunc.func(bindingFunc->arg, bindingFunc->arg2);
    }
}

static Option baseOptions[] = {
    {"dump", {dumpWindowByClass}, .flags = FORK_ON_RECEIVE | REQUEST_STR},
    {"dump", {dumpWindowFilter, .arg.i = MAPPABLE_MASK}, .flags = FORK_ON_RECEIVE},
    {"dump-master", {dumpMaster, .arg.i = 0}, .flags = FORK_ON_RECEIVE},
    {"dump-rules", {dumpRules},  .flags = FORK_ON_RECEIVE},
    {"dump-win", {dumpWindow},  .flags = FORK_ON_RECEIVE | REQUEST_INT},
    {"focus-win", {(void(*)())focusWindow}, .flags = REQUEST_INT},
    {"log-level", {setLogLevel}, .flags = VAR_SETTER | REQUEST_INT},
    {"lower", {lowerWindow}, .flags = REQUEST_INT},
    {"next-layout", {cycleLayouts}, UP},
    {"next-win", {shiftFocus}, UP},
    {"next-win-of-class", {shiftFocusToNextClass}, UP},
    {"prev-layout", {cycleLayouts}, DOWN},
    {"prev-win", {shiftFocus}, DOWN},
    {"prev-win-of-class", {shiftFocusToNextClass}, DOWN},
    {"raise", {raiseWindow}, .flags = REQUEST_INT},
    {"raise-or-run", {raiseOrRun2},  .flags = REQUEST_STR | REQUEST_MULTI | UNSAFE},
    {"raise-or-run", {raiseOrRun},  .flags = REQUEST_STR | UNSAFE},
    {"raise-or-run-role", {raiseOrRunRole},  .flags = REQUEST_STR | REQUEST_MULTI | UNSAFE},
    {"raise-or-run-title", {raiseOrRunTitle},  .flags = REQUEST_STR | REQUEST_MULTI | UNSAFE},
    {"restart", {restart}, .flags = CONFIRM_EARLY},
    {"spawn", {spawn},  .flags = REQUEST_STR | UNSAFE},
    {"sum", {printSummary}, .flags = FORK_ON_RECEIVE},
    {"switch-workspace", {switchToWorkspace}},
};
static ArrayList options;

ArrayList* getOptions() {
    return &options;
};

void initOptions() {
    for(int i = 0; i < LEN(baseOptions); i++)
        addElement(getOptions(), baseOptions + i);
}
void addInterClientCommunicationRule() {
    addEvent(XCB_CLIENT_MESSAGE, DEFAULT_EVENT(receiveClientMessage));
}


static int outstandingSendCount;
uint32_t getNumberOfMessageSent() {
    return outstandingSendCount;
}
int getConfirmedSentMessage(WindowID win) {
    int result = getWindowPropertyValueInt(win, MPX_WM_INTERPROCESS_COM, XCB_ATOM_CARDINAL);
    TRACE("Found %d out of %d confirmations", result, getNumberOfMessageSent());
    return result;
}
bool hasOutStandingMessages(void) {
    return getNumberOfMessageSent() > getConfirmedSentMessage(getPrivateWindow());
}
int getLastMessageExitCode(void) {
    return getWindowPropertyValueInt(getPrivateWindow(), MPX_WM_INTERPROCESS_COM_STATUS, XCB_ATOM_CARDINAL);
}
static void sendConfirmation(WindowID target, int exitCode) {
    DEBUG("sending receive confirmation to %d", target);
    // not atomic, but there should be only one thread/process modifying this value
    int count = getConfirmedSentMessage(target) + 1;
    setWindowPropertyInt(target, MPX_WM_INTERPROCESS_COM_STATUS, XCB_ATOM_CARDINAL, exitCode);
    setWindowPropertyInt(target, MPX_WM_INTERPROCESS_COM, XCB_ATOM_CARDINAL, count);
    flush();
}
void send(const char* name, const char* value) {
    sendAs(name, 0, value, 0);
}

void sendAs(const char* name, WindowID active, const char* value, const char* value2) {
    DEBUG("sending %s %s %s active: %d", name, value, value2, active);
    setWindowPropertyString(getPrivateWindow(), OPTION_NAME, ewmh->UTF8_STRING, name);
    StringJoiner joiner = {0};
    if(value)
        addString(&joiner, value);
    if(value2)
        addString(&joiner, value2);
    setWindowPropertyStrings(getPrivateWindow(), OPTION_VALUES, ewmh->UTF8_STRING, &joiner);
    freeBuffer(&joiner);
    unsigned int data[5] = {active};
    catchError(xcb_ewmh_send_client_message(dis, getPrivateWindow(), root, MPX_WM_INTERPROCESS_COM, sizeof(data), data));
    outstandingSendCount++;
}
const Option* findOption(const char* name, const char* value1, const char* value2) {
    char numArgs = (value2 && value2[0]) + (value1 && value1[0]);
    FOR_EACH(Option*, option, getOptions()) {
        if(matchesOption(option, name, numArgs))
            return option;
    }
    WARN("could not find option matching '%s' '%s' '%s' numArgs %d", name, value1, value2, numArgs);
    return NULL;
}

void receiveClientMessage(xcb_client_message_event_t* event) {
    xcb_client_message_data_t data = event->data;
    WindowID win = event->window;
    xcb_atom_t message = event->type;
    WindowID active = data.data32[0];
    char name[MAX_NAME_LEN];
    char value[MAX_NAME_LEN] = "", value2[MAX_NAME_LEN] = "";
    char* values[2] = {value, value2};
    getWindowPropertyString(win, OPTION_NAME, ewmh->UTF8_STRING, name);
    getWindowPropertyStrings(win, OPTION_VALUES, ewmh->UTF8_STRING, values, LEN(values));
    if(message == MPX_WM_INTERPROCESS_COM && name[0]) {
        DEBUG("Received option %s values %s %s", name, values[0], values[1]);
        pid_t callerPID = getWindowPropertyValueInt(win, ewmh->_NET_WM_PID, XCB_ATOM_CARDINAL);
        const Option* option = findOption(name, values[0], values[1]);
        if(active) {
            WindowInfo* winInfo = getWindowInfo(active);
            if(winInfo) {
                INFO("Find client master for %d", winInfo->id);
                setActiveMasterByDeviceID(getClientPointerForWindow(winInfo->id));
            }
            else {
                INFO("Setting active master to %d", active);
                setActiveMasterByDeviceID(active);
            }
        }
        if(option && (ALLOW_UNSAFE_OPTIONS || !(option->flags & UNSAFE))) {
            INFO("Executing %s", option->name);
            int returnValue = 0;
            if(option->flags & CONFIRM_EARLY) {
                TRACE("sending early confirmation");
                destroyWindow(getPrivateWindow());
                sendConfirmation(win, NORMAL_TERMINATION);
                flush();
            }
            fflush(NULL);
            int stdout = -1;
            if((option->flags & FORK_ON_RECEIVE)) {
                fflush(NULL);
                stdout = dup(STDOUT_FILENO);
                char outputFile[64] = {"/dev/null"};
                if(callerPID) {
                    const char* formatter = "/proc/%d/fd/1";
                    sprintf(outputFile, formatter, callerPID);
                }
                int fd = open(outputFile, O_WRONLY | O_APPEND);
                if(fd == -1) {
                    WARN("could not open %s for writing;", outputFile);
                }
                else if(dup2(fd, STDOUT_FILENO) == -1) {
                    WARN("could not open %s for writing; could not call/set '%s' aborting", outputFile,
                        option->name);
                    exit(SYS_CALL_FAILED);
                }
                else
                    close(fd);
            }
            callOption(option, values[0], values[1]);
            if((option->flags & FORK_ON_RECEIVE)) {
                fflush(NULL);
                if(dup2(stdout, STDOUT_FILENO) == -1) {
                    perror("Could not revert back stdout");
                }
                close(stdout);
            }
            if(!(option->flags & CONFIRM_EARLY))
                sendConfirmation(win, returnValue);
        }
        else {
            if(option)
                INFO("option '%s' is unsafe and unsafe options are not allowed", name);
            else
                WARN("could not find option matching '%s' '%s' '%s'", name, values[0], values[1]);
            sendConfirmation(win, INVALID_OPTION);
        }
    }
}
