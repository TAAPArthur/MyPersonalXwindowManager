#include <limits.h>
#include <assert.h>
#include <string.h>

#include "../boundfunction.h"
#include "../devices.h"
#include "../layouts.h"
#include "../masters.h"
#include "../monitors.h"
#include "../user-events.h"
#include "../util/debug.h"
#include "../util/logger.h"
#include "../windows.h"
#include "../wmfunctions.h"
#include "../workspaces.h"
#include "../util/string-array.h"
#include "../xutil/window-properties.h"
#include "../xutil/xsession.h"
#include "session.h"

/// Stores the active master so the state can be restored
xcb_atom_t MPX_WM_ACTIVE_MASTER;
/// Atom to store fake monitors
xcb_atom_t MPX_WM_FAKE_MONITORS;
/// Atom to store fake monitors
xcb_atom_t MPX_WM_FAKE_MONITORS_NAMES;
/// Used to save raw window masks
xcb_atom_t MPX_WM_MASKS;
/// Str representation of MPX_WM_MASKS used solely for debugging
xcb_atom_t MPX_WM_MASKS_STR;
/// Atom to store an array of each window for every master so the state can be restored
/// There is a '0' to separate each master's window stack and each stack is preceded with the master id
xcb_atom_t MPX_WM_MASTER_WINDOWS;
/// pair of master index and master's workspace index
xcb_atom_t MPX_WM_MASTER_WORKSPACES;
/// Atom to store an array of the layout offset for each workspace so the state can be restored
xcb_atom_t MPX_WM_WORKSPACE_LAYOUT_INDEXES;
/// Atom to store an array of the active layout's for each workspace so the state can be restored
xcb_atom_t MPX_WM_WORKSPACE_LAYOUT_NAMES;
/// Atom to store a mapping or monitor name to workspace name so a monitor can resume its workspace when it is disconnected and reconnected
xcb_atom_t MPX_WM_WORKSPACE_MONITORS;

/// Used to preserves the workspace stack order
xcb_atom_t MPX_WM_WORKSPACE_ORDER;


typedef struct {
    MonitorID id;
    Rect bounds;
} MonitorIDBounds;


static void initSessionAtoms() {
    CREATE_ATOM(MPX_WM_ACTIVE_MASTER);
    CREATE_ATOM(MPX_WM_FAKE_MONITORS);
    CREATE_ATOM(MPX_WM_FAKE_MONITORS_NAMES);
    CREATE_ATOM(MPX_WM_MASKS);
    CREATE_ATOM(MPX_WM_MASKS_STR);
    CREATE_ATOM(MPX_WM_MASTER_WINDOWS);
    CREATE_ATOM(MPX_WM_MASTER_WORKSPACES);
    CREATE_ATOM(MPX_WM_WORKSPACE_LAYOUT_INDEXES);
    CREATE_ATOM(MPX_WM_WORKSPACE_LAYOUT_NAMES);
    CREATE_ATOM(MPX_WM_WORKSPACE_MONITORS);
    CREATE_ATOM(MPX_WM_WORKSPACE_ORDER);
}

static void loadSavedLayouts() {
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_WORKSPACE_LAYOUT_NAMES, ewmh->UTF8_STRING);
    TRACE("Loading active layouts");
    if(reply) {
        uint32_t len = xcb_get_property_value_length(reply);
        char* strings = (char*) xcb_get_property_value(reply);
        uint32_t index = 0, count = 0;
        while(index < len) {
            Layout* layout = findLayoutByName(&strings[index]);
            setLayout(getWorkspace(count++), layout);
            index += strlen(&strings[index]) + 1;
            if(count == getNumberOfWorkspaces())
                break;
        }
        free(reply);
    }
}
static void loadSavedLayoutOffsets() {
    TRACE("Loading Workspace layout offsets");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_WORKSPACE_LAYOUT_INDEXES, XCB_ATOM_CARDINAL);
    if(reply)
        for(uint32_t i = 0; i < xcb_get_property_value_length(reply) / sizeof(int) && i < getNumberOfWorkspaces(); i++)
            setLayoutOffset(getWorkspace(i), (((int*)xcb_get_property_value(reply))[i]));
    free(reply);
}

static int getNumberOfSavedIDBoundInfo(xcb_get_property_reply_t* reply) {
    return reply ? xcb_get_property_value_length(reply) / (sizeof(int) * 3) : 0;
}
static inline MonitorIDBounds getNthSavedIDBoundInfo(xcb_get_property_reply_t* reply, int i) {
    int* values = (int*) & (((char*)xcb_get_property_value(reply))[i * sizeof(int) * 3]);
    return (MonitorIDBounds) {.id = values[0], .bounds = *(Rect*)&values[1]};
}
static void loadSavedFakeMonitor() {
    TRACE("Loading fake monitor mappings");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_FAKE_MONITORS, XCB_ATOM_CARDINAL);
    int size  = getNumberOfSavedIDBoundInfo(reply);
    if(size) {
        xcb_get_property_reply_t* replyNames = getWindowProperty(root, MPX_WM_FAKE_MONITORS_NAMES, ewmh->UTF8_STRING);
        char* strings = replyNames ? (char*) xcb_get_property_value(replyNames) : NULL;
        uint32_t index = 0;
        for(int i = 0; i < size; i++) {
            MonitorIDBounds buffer = getNthSavedIDBoundInfo(reply, i);
            Monitor* m = findElement(getAllMonitors(), &buffer.id, sizeof(MonitorID));
            const char* name = strings ? &strings[index] : "";
            index += strlen(&strings[index]) + 1;
            if(m)
                setBase(m, buffer.bounds);
            else
                newMonitor(buffer.id, buffer.bounds, 0, name, 1);
        }
        free(replyNames);
        free(reply);
    }
}
static void loadSavedMasterWindows() {
    TRACE("Loading Master window stacks");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_MASTER_WINDOWS, XCB_ATOM_CARDINAL);
    if(reply) {
        Master* master = NULL;
        WindowID* wid = (WindowID*)xcb_get_property_value(reply);
        TRACE("Found %ld properties", xcb_get_property_value_length(reply) / sizeof(int));
        for(uint32_t i = 0; i < xcb_get_property_value_length(reply) / sizeof(int); i++) {
            if(wid[i] == 0) {
                master = getMasterByID(wid[++i]);
            }
            else if(master)
                onWindowFocusForMaster(wid[i], master);
        }
    }
    free(reply);
}
static void restoreFocusColor() {
    FOR_EACH(Master*, master, getAllMasters()) {
        if(getFocusedWindowOfMaster(master))
            setBorderColor(getFocusedWindowOfMaster(master)->id, master->focusColor);
    }
}
static void loadSavedMasterWorkspaces() {
    TRACE("Loading Master Workspace indexes");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_MASTER_WORKSPACES, XCB_ATOM_CARDINAL);
    if(reply) {
        WorkspaceID* id = (WorkspaceID*)xcb_get_property_value(reply);
        for(uint32_t i = 0; i + 1 < xcb_get_property_value_length(reply) / sizeof(int); i += 2) {
            if(getMasterByID(id[i]))
                getMasterByID(id[i])->activeWorkspaceIndex = id[i + 1];
        }
    }
    free(reply);
}
static void loadSavedWorkspaceWindows() {
    TRACE("Loading Master window stacks");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_WORKSPACE_ORDER, XCB_ATOM_CARDINAL);
    if(reply) {
        WindowID* wid = (WindowID*)xcb_get_property_value(reply);
        WorkspaceID workspaceID = 0;
        for(uint32_t i = 0; i < xcb_get_property_value_length(reply) / sizeof(int); i++)
            if(wid[i] == 0) {
                if(++workspaceID == getNumberOfWorkspaces())
                    break;
            }
            else {
                WindowInfo* winInfo = getWindowInfo(wid[i]);
                if(winInfo && getWorkspaceIndexOfWindow(winInfo) == workspaceID) {
                    Workspace* w = getWorkspace(workspaceID);
                    shiftToEnd(getWorkspaceWindowStack(w), getIndex(getWorkspaceWindowStack(w), winInfo, sizeof(WindowID)));
                }
            }
    }
    free(reply);
}
static void loadSavedActiveMaster() {
    TRACE("Loading active Master");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_ACTIVE_MASTER, XCB_ATOM_CARDINAL);
    if(reply) {
        setActiveMasterByDeviceID(*(MasterID*)xcb_get_property_value(reply));
    }
    free(reply);
}
void loadSavedMonitorWorkspaceMapping() {
    TRACE("Montiro workspace mappings ");
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_WORKSPACE_MONITORS, XCB_ATOM_CARDINAL);
    if(reply) {
        int size  = getNumberOfSavedIDBoundInfo(reply);
        for(int i = 0; i < size ; i++) {
            MonitorIDBounds buffer = getNthSavedIDBoundInfo(reply, i);
            WorkspaceID id = buffer.id;
            if(id >= getNumberOfWorkspaces()) {
                INFO("Skipping Workspace %d because it is out of range", id);
                continue;
            }
            Workspace* workspace = getWorkspace(id);
            if(workspace && !isWorkspaceVisible(workspace)) {
                FOR_EACH(Monitor*, m, getAllMonitors()) {
                    if(!getWorkspaceOfMonitor(m) && memcmp(&m->base, &buffer.bounds, sizeof(Rect)) == 0) {
                        INFO("Restoring Monitor %d to Workspace %d", m->id, workspace->id);
                        setMonitor(workspace, m);
                        break;
                    }
                }
            }
        }
        free(reply);
    }
}

void loadWindowMasks() {
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        WindowMask mask = ~EXTERNAL_MASKS & getWindowPropertyValueInt(winInfo->id, MPX_WM_MASKS, XCB_ATOM_CARDINAL);
        if(mask) {
            INFO("Restoring mask of window %d", winInfo->id);
            if(!hasMask(winInfo, mask)) {
                addMask(winInfo, mask);
            }
        }
    }
}

void loadSavedNonWindowState(void) {
    loadSavedLayouts();
    loadSavedLayoutOffsets();
    loadSavedFakeMonitor();
    loadSavedMonitorWorkspaceMapping();
    loadSavedMasterWorkspaces();
    loadSavedActiveMaster();
}

void loadSavedWindowState(void) {
    loadSavedMasterWindows();
    loadSavedWorkspaceWindows();
    loadWindowMasks();
    restoreFocusColor();
}


static int serializeMonitorBounds(int* idBounds, int bufferSize, bool onlyFakes, bool storeWorkspaceID) {
    int i = 0;
    FOR_EACH(Monitor*, monitor, getAllMonitors()) {
        if((!onlyFakes || monitor->fake)) {
            Workspace* w = getWorkspaceOfMonitor(monitor);
            idBounds[i++] = storeWorkspaceID ? w ? w->id : -1 : monitor->id;
            memcpy(idBounds + i, &monitor->base.x, sizeof(Rect));
            i += 2;
            if(i == bufferSize)
                break;
        }
    }
    return i;
}

void saveMonitorWorkspaceMapping() {
    TRACE("Saving monitor workspace mapping");
    const int MAX_SIZE = 16;
    int idBounds[MAX_SIZE  * 3];
    int i = serializeMonitorBounds(idBounds, LEN(idBounds), 0, 1);
    xcb_get_property_reply_t* reply = getWindowProperty(root, MPX_WM_WORKSPACE_MONITORS, XCB_ATOM_CARDINAL);
    int size  = getNumberOfSavedIDBoundInfo(reply);
    for(int n = 0; i < LEN(idBounds) && n < size; n++) {
        MonitorIDBounds buffer = getNthSavedIDBoundInfo(reply, n);
        idBounds[i++] = buffer.id;
        memcpy(idBounds + i, &buffer.bounds.x, sizeof(Rect));
        i += 2;
    }
    setWindowProperty(root, MPX_WM_WORKSPACE_MONITORS, XCB_ATOM_CARDINAL, idBounds, i);
    free(reply);
}
static void saveFakeMonitorInfo(void) {
    DEBUG("Saving fake monitor state State");
    int idBounds[getAllMonitors()->size * 3];
    int i = serializeMonitorBounds(idBounds, LEN(idBounds), 1, 0);
    StringJoiner joiner = {0};
    FOR_EACH(Monitor*, monitor, getAllMonitors()) {
        if(monitor->fake) {
            assert(i);
            addString(&joiner, monitor->name);
        }
    }
    setWindowProperty(root, MPX_WM_FAKE_MONITORS, XCB_ATOM_CARDINAL, idBounds, i);
    setWindowProperty(root, MPX_WM_FAKE_MONITORS_NAMES, ewmh->UTF8_STRING, getBuffer(&joiner),
        joiner.usedBufferSize);
    freeBuffer(&joiner);
}
void saveCustomState(void) {
    DEBUG("Saving custom State");
    saveFakeMonitorInfo();
    int layoutOffsets[getNumberOfWorkspaces()];
    WindowID masterWindows[(getAllWindows()->size + 2) * getAllMasters()->size];
    WorkspaceID masterWorkspaces[getAllMasters()->size * 2];
    WindowID workspaceWindows[getAllWindows()->size + getNumberOfWorkspaces() ];
    int numMasterWindows = 0;
    int numWorkspaceWindows = 0;
    for(int i = getAllMasters()->size - 1; i >= 0; i--) {
        Master* master = getElement(getAllMasters(), i);
        masterWindows[numMasterWindows++] = 0;
        masterWindows[numMasterWindows++] = master->id;
        masterWorkspaces[2 * i] = master->id;
        masterWorkspaces[2 * i + 1] = getMasterWorkspaceIndex(master);
        FOR_EACH_R(WindowInfo*, winInfo, getMasterWindowStack(master)) {
            masterWindows[numMasterWindows++] = winInfo->id;
        }
    }
    StringJoiner joiner = {0};
    for(WorkspaceID i = 0; i < getNumberOfWorkspaces(); i++) {
        layoutOffsets[i] = getLayoutOffset(getWorkspace(i));
        Layout* layout = getLayout(getWorkspace(i));
        addString(&joiner, (layout ? layout->name : ""));
        FOR_EACH(WindowInfo*, winInfo, getWorkspaceWindowStack(getWorkspace(i))) {
            workspaceWindows[numWorkspaceWindows++] = winInfo->id;
        }
        workspaceWindows[numWorkspaceWindows++] = 0;
    }
    DEBUG("Saving other customer state");
    setWindowPropertyInt(root, MPX_WM_ACTIVE_MASTER, XCB_ATOM_CARDINAL, getActiveMaster()->id);
    setWindowProperty(root, MPX_WM_MASTER_WINDOWS, XCB_ATOM_CARDINAL, masterWindows, numMasterWindows);
    setWindowProperty(root, MPX_WM_MASTER_WORKSPACES, XCB_ATOM_CARDINAL, masterWorkspaces, LEN(masterWorkspaces));
    setWindowProperty(root, MPX_WM_WORKSPACE_LAYOUT_INDEXES, XCB_ATOM_CARDINAL, layoutOffsets, LEN(layoutOffsets));
    setWindowPropertyStrings(root, MPX_WM_WORKSPACE_LAYOUT_NAMES, ewmh->UTF8_STRING, &joiner);
    setWindowProperty(root, MPX_WM_WORKSPACE_ORDER, XCB_ATOM_CARDINAL, workspaceWindows, numWorkspaceWindows);
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        if((winInfo->mask ^ winInfo->savedMask) & (~EXTERNAL_MASKS)) {
            WindowMask mask = ~EXTERNAL_MASKS & winInfo->mask;
            TRACE("Saving window masks for window: %d", winInfo->id);
            setWindowPropertyInt(winInfo->id, MPX_WM_MASKS, XCB_ATOM_CARDINAL, mask);
            setWindowPropertyString(winInfo->id, MPX_WM_MASKS_STR, ewmh->UTF8_STRING, getMaskAsString(mask, NULL));
        }
    }
    freeBuffer(&joiner);
}
void addResumeCustomStateRules() {
    addBatchEvent(MONITOR_WORKSPACE_CHANGE, DEFAULT_EVENT(saveMonitorWorkspaceMapping, LOWEST_PRIORITY));
    addBatchEvent(SCREEN_CHANGE, DEFAULT_EVENT(loadSavedMonitorWorkspaceMapping));
    addBatchEvent(SCREEN_CHANGE, DEFAULT_EVENT(saveMonitorWorkspaceMapping, LOWEST_PRIORITY));
    addEvent(IDLE, DEFAULT_EVENT(saveCustomState, LOWER_PRIORITY));
    addEvent(X_CONNECTION, DEFAULT_EVENT(initSessionAtoms, HIGHEST_PRIORITY));
    addEvent(X_CONNECTION, DEFAULT_EVENT(loadSavedNonWindowState, HIGHER_PRIORITY));
    addEvent(X_CONNECTION, DEFAULT_EVENT(loadSavedWindowState));
}
