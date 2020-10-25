#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../boundfunction.h"
#include "../monitors.h"
#include "../mywm-structs.h"
#include "../user-events.h"
#include "../window-masks.h"
#include "../windows.h"
#include "../workspaces.h"
#include "debug.h"
#include "logger.h"

static char buffer[MAX_NAME_LEN];

#define _ADD_EVENT_TYPE_CASE(TYPE) case TYPE: return #TYPE
#define _ADD_GE_EVENT_TYPE_CASE(TYPE) case GENERIC_EVENT_OFFSET+TYPE: return #TYPE
const char* eventTypeToString(int type) {
    switch(type) {
            _ADD_EVENT_TYPE_CASE(XCB_EXPOSE);
            _ADD_EVENT_TYPE_CASE(XCB_VISIBILITY_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_CREATE_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_DESTROY_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_UNMAP_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_MAP_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_MAP_REQUEST);
            _ADD_EVENT_TYPE_CASE(XCB_REPARENT_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_CONFIGURE_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_CONFIGURE_REQUEST);
            _ADD_EVENT_TYPE_CASE(XCB_PROPERTY_NOTIFY);
            _ADD_EVENT_TYPE_CASE(XCB_SELECTION_CLEAR);
            _ADD_EVENT_TYPE_CASE(XCB_CLIENT_MESSAGE);
            _ADD_EVENT_TYPE_CASE(XCB_GE_GENERIC);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_DEVICE_CHANGED);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_KEY_PRESS);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_KEY_RELEASE);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_BUTTON_PRESS);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_BUTTON_RELEASE);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_MOTION);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_ENTER);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_LEAVE);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_FOCUS_IN);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_FOCUS_OUT);
            _ADD_GE_EVENT_TYPE_CASE(XCB_INPUT_HIERARCHY);
            _ADD_EVENT_TYPE_CASE(EXTRA_EVENT);
            _ADD_EVENT_TYPE_CASE(X_CONNECTION);
            _ADD_EVENT_TYPE_CASE(POST_REGISTER_WINDOW);
            _ADD_EVENT_TYPE_CASE(UNREGISTER_WINDOW);
            _ADD_EVENT_TYPE_CASE(DEVICE_EVENT);
            _ADD_EVENT_TYPE_CASE(CLIENT_MAP_ALLOW);
            _ADD_EVENT_TYPE_CASE(WORKSPACE_WINDOW_ADD);
            _ADD_EVENT_TYPE_CASE(WORKSPACE_WINDOW_REMOVE);
            _ADD_EVENT_TYPE_CASE(MONITOR_WORKSPACE_CHANGE);
            _ADD_EVENT_TYPE_CASE(SCREEN_CHANGE);
            _ADD_EVENT_TYPE_CASE(WINDOW_MOVE);
            _ADD_EVENT_TYPE_CASE(TILE_WORKSPACE);
            _ADD_EVENT_TYPE_CASE(IDLE);
            _ADD_EVENT_TYPE_CASE(TRUE_IDLE);
        case 0:
            return "Error";
        default:
            return "unknown event";
    }
}


/**
 * @return a string representation of mask
 */
const char* getMaskAsString(WindowMask mask, char* buff) {
#define _PRINT_MASK(windowMask) if( windowMask != 0 && (windowMask & M) == (uint32_t)windowMask || windowMask==0 && M==0){strcat(buff, #windowMask " ");M&=~windowMask;}
    if(!buff)
        buff = buffer;
    *buff = 0;
    uint32_t M = mask;
    _PRINT_MASK(NO_MASK);
    _PRINT_MASK(MAXIMIZED_MASK);
    _PRINT_MASK(X_MAXIMIZED_MASK);
    _PRINT_MASK(Y_MAXIMIZED_MASK);
    _PRINT_MASK(CENTERED_MASK);
    _PRINT_MASK(X_CENTERED_MASK);
    _PRINT_MASK(Y_CENTERED_MASK);
    _PRINT_MASK(FULLSCREEN_MASK);
    _PRINT_MASK(ROOT_FULLSCREEN_MASK);
    _PRINT_MASK(FLOATING_MASK);
    _PRINT_MASK(MODAL_MASK);
    _PRINT_MASK(BELOW_MASK);
    _PRINT_MASK(ABOVE_MASK);
    _PRINT_MASK(NO_TILE_MASK);
    _PRINT_MASK(STICKY_MASK);
    _PRINT_MASK(PRIMARY_MONITOR_MASK);
    _PRINT_MASK(HIDDEN_MASK);
    _PRINT_MASK(EXTERNAL_CONFIGURABLE_MASK);
    _PRINT_MASK(EXTERNAL_RESIZE_MASK);
    _PRINT_MASK(EXTERNAL_MOVE_MASK);
    _PRINT_MASK(EXTERNAL_BORDER_MASK);
    _PRINT_MASK(EXTERNAL_RAISE_MASK);
    _PRINT_MASK(IGNORE_WORKSPACE_MASKS_MASK);
    _PRINT_MASK(INPUT_MASK);
    _PRINT_MASK(NO_RECORD_FOCUS_MASK);
    _PRINT_MASK(NO_ACTIVATE_MASK);
    _PRINT_MASK(MAPPED_MASK);
    _PRINT_MASK(MAPPABLE_MASK);
    _PRINT_MASK(URGENT_MASK);
    assert(!M);
    return buff;
}

void dumpRect(Rect rect) {
    printf(" { %d, %d, %u, %u}", rect.x, rect.y, rect.width, rect.height);
}
void dumpMonitor(Monitor* monitor) {
    printf("Monitor %d%s %s ", monitor->id, (isPrimary(monitor) ? "!" : monitor->fake ? "?" : ""), monitor->name);
    if(getWorkspaceOfMonitor(monitor))
        printf("Workspace: %d ", getWorkspaceOfMonitor(monitor)->id);
    if(!isMonitorActive(monitor))
        printf("inactive ");
    dumpRect(monitor->base);
    if(memcmp(&monitor->base, &monitor->view, sizeof(Rect)))
        dumpRect(monitor->view);
    printf("\n");
}

void dumpWorkspace(Workspace* workspace) {
    printf("ID: %d%s; name: '%s' ", workspace->id, isWorkspaceVisible(workspace) ? "*" : "", workspace->name);
    if(workspace->mask)
        printf(" %s", getMaskAsString(workspace->mask, buffer));
    printf("Windows: {");
    if(getWorkspaceWindowStack(workspace)->size) {
        FOR_EACH(WindowInfo*, winInfo, getWorkspaceWindowStack(workspace))
        printf(" %d", winInfo->id);
    }
    printf("}\n");
}
void dumpMaster(Master* master) {
    if(!master)
        master = getActiveMaster();
    printf("Master %d (%d) %s %06x", master->id, master->pointerID, master->name, master->focusColor);
    printf("Workspace %d ", getMasterWorkspaceIndex(master));
    printf("Slaves: {");
    FOR_EACH(Slave*, slave, getSlaves(master)) {
        printf(" %d", slave->id);
    }
    printf("} Windows: {");
    FOR_EACH(WindowInfo*, winInfo, getMasterWindowStack(master)) {
        printf(" %d", winInfo->id);
    }
    printf("}\n");
}
void dumpWindowInfo(WindowInfo* winInfo) {
    printf("{ID %d%s ", winInfo->id, (isTileable(winInfo) ? "*" : !isMappable(winInfo) ? "?" :  ""));
    if(winInfo->type)
        printf("Title '%s' Class '%s' '%s' ", winInfo->title, winInfo->className, winInfo->instanceName);
    if(winInfo->role[0])
        printf("Role '%s' ", winInfo->role);
    if(winInfo->dock) {
        printf("Dock ");
    }
    if(winInfo->type) {
        printf("Type %d %d", winInfo->type, winInfo->implicitType);
    }
    printf("%s ", getMaskAsString(winInfo->mask, buffer));
    printf("Geometry: ");
    dumpRect(winInfo->geometry);
    printf("}\n");
}
void dumpWindowFilter(WindowMask filterMask) {
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        if(!filterMask || hasMask(winInfo, filterMask))
            dumpWindowInfo(winInfo);
    }
}
void dumpWindowByClass(const char* match) {
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        if(!strcmp(winInfo->className, match) || !strcmp(winInfo->instanceName, match))
            dumpWindowInfo(winInfo);
    }
}
void dumpWindow(WindowID win) {
    WindowInfo* winInfo = getWindowInfo(win);
    if(!winInfo) {
        printf("Could not find window\n");
        return;
    }
    dumpWindowInfo(winInfo);
}

void printSummary(void) {
    printf("Summary");
    printf("\nSlaves:\n");
    FOR_EACH(Slave*, slave, getAllSlaves()) {
        printf("ID: %d; Master %d; Keyboard %d; %s\n", slave->id, slave->attachment, slave->keyboard, slave->name);
    }
    printf("\nMasters:\n");
    FOR_EACH(Master*, master, getAllMasters()) {
        dumpMaster(master);
    }
    printf("\nWorkspaces:\n");
    FOR_EACH(Workspace*, workspace, getAllWorkspaces()) {
        dumpWorkspace(workspace);
    }
    printf("\nMonitors:\n");
    FOR_EACH(Monitor*, monitor, getAllMonitors()) {
        dumpMonitor(monitor);
    }
    printf("\nWindows:\n");
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        dumpWindowInfo(winInfo);
    }
}

ArrayList* getEventList(int type, bool batch);


void dumpRules(void) {
    for(int batch = 0; batch < 2; batch++) {
        for(int i = 0; i < NUMBER_OF_MPX_EVENTS; i++)
            if(!getEventList(i, batch)->size) {
                printf("%s%s: {", batch ? "BATCH_" : "", eventTypeToString(i));
                FOR_EACH(BoundFunction*, b, getEventList(i, batch)) {
                    printf("%s, ", b->name);
                }
                printf("}\n");
            }
    }
}
