#include <assert.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <X11/extensions/XInput2.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xinput.h>

#include "bindings.h"
#include "boundfunction.h"
#include "xutil/device-grab.h"
#include "globals.h"
#include "masters.h"
#include "monitors.h"
#include "threads.h"
#include "user-events.h"
#include "util/logger.h"
#include "util/time.h"
#include "xutil/window-properties.h"
#include "windows.h"
#include "wmfunctions.h"
#include "workspaces.h"
#include "system.h"
#include "xutil/xsession.h"

WindowID isMPXManagerRunning(void) {
    xcb_get_selection_owner_reply_t* ownerReply = xcb_get_selection_owner_reply(dis, xcb_get_selection_owner(dis,
                MPX_WM_SELECTION_ATOM), NULL);
    WindowID win = 0;
    if(ownerReply && ownerReply->owner) {
        if(getWindowPropertyValueInt(ownerReply->owner, ewmh->_NET_WM_PID, XCB_ATOM_CARDINAL))
            win = ownerReply->owner;
    }
    free(ownerReply);
    return win;
}

void ownSelection(xcb_atom_t selectionAtom) {
    if(!STEAL_WM_SELECTION) {
        xcb_get_selection_owner_reply_t* ownerReply = xcb_get_selection_owner_reply(dis, xcb_get_selection_owner(dis,
                    selectionAtom), NULL);
        if(ownerReply->owner) {
            ERROR("Selection %d is already owned by window %d", selectionAtom, ownerReply->owner);
            quit(WM_ALREADY_RUNNING);
        }
        free(ownerReply);
    }
    DEBUG("Setting selection owner");
    if(catchError(xcb_set_selection_owner_checked(dis, getPrivateWindow(), selectionAtom,
                XCB_CURRENT_TIME)) == 0) {
        unsigned int data[5] = {XCB_CURRENT_TIME, selectionAtom, getPrivateWindow()};
        xcb_ewmh_send_client_message(dis, root, root, selectionAtom, 5, data);
    }
}

bool registerWindowInfo(WindowInfo* winInfo, xcb_get_window_attributes_reply_t* attr) {
    const bool freeAttr = !attr;
    const bool newlyCreated = !attr;
    if(!attr)
        attr = xcb_get_window_attributes_reply(dis, xcb_get_window_attributes(dis, winInfo->id), NULL);
    if(attr) {
        if(attr->override_redirect)
            winInfo->overrideRedirect = 1;
        if(attr->_class == XCB_WINDOW_CLASS_INPUT_ONLY)
            winInfo->inputOnly = 1;
        if(attr->map_state != XCB_MAP_STATE_UNMAPPED)
            addMask(winInfo, MAPPABLE_MASK | MAPPED_MASK);
        if(freeAttr)
            free(attr);
    }
    else {
        DEBUG("Could not load attributes %d ", winInfo->id);
        freeWindowInfo(winInfo);
        return 0;
    }
    DEBUG("Registering %d (%x)", winInfo->id, winInfo->id);
    if(newlyCreated)
        winInfo->creationTime = getTime();
    else
        loadWindowHints(winInfo);
    if(applyEventRules(POST_REGISTER_WINDOW, winInfo)) {
        if(hasMask(winInfo, MAPPABLE_MASK)) {
            DEBUG("Window is mappable %d", winInfo->id);
            if(!applyEventRules(CLIENT_MAP_ALLOW, winInfo))
                return 0;
        }
        return 1;
    }
    return 0;
}
bool registerWindow(WindowID win, WindowID parent, xcb_get_window_attributes_reply_t* attr) {
    assert(!getWindowInfo(win) && "Window registered exists");
    TRACE("processing %d (%x)", win, win);
    return registerWindowInfo(newWindowInfo(win, parent), attr);
}
void scan(xcb_window_t baseWindow) {
    assert(baseWindow);
    TRACE("Scanning children of window %d", baseWindow);
    xcb_query_tree_reply_t* reply;
    reply = xcb_query_tree_reply(dis, xcb_query_tree(dis, baseWindow), 0);
    if(reply) {
        xcb_get_window_attributes_reply_t* attr;
        int numberOfChildren = xcb_query_tree_children_length(reply);
        TRACE("detected %d kids", numberOfChildren);
        xcb_window_t* children = xcb_query_tree_children(reply);
        xcb_get_window_attributes_cookie_t cookies[numberOfChildren];
        for(int i = 0; i < numberOfChildren; i++)
            cookies[i] = xcb_get_window_attributes(dis, children[i]);
        // iterate in bottom to top order
        for(int i = 0; i < numberOfChildren; i++) {
            TRACE("processing child %d", children[i]);
            attr = xcb_get_window_attributes_reply(dis, cookies[i], NULL);
            if(registerWindow(children[i], baseWindow, attr)) {
                xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(dis, xcb_get_geometry(dis, children[i]), NULL);
                if(reply) {
                    getWindowInfo(children[i])->geometry = *(Rect*)&reply->x;
                    free(reply);
                }
            }
            if(attr)
                free(attr);
        }
        free(reply);
    }
}

static bool focusNextVisibleWindow(Master* master, WindowInfo* ignore) {
    if(getFocusedWindowOfMaster(master) && ignore != getFocusedWindowOfMaster(master) &&
        isNotInInvisibleWorkspace(getFocusedWindowOfMaster(master)) &&
        focusWindowInfoAsMaster(getFocusedWindowOfMaster(master), master)) {
        return 1;
    }
    FOR_EACH(WindowInfo*, winInfo, getMasterWindowStack(master)) {
        if(winInfo != ignore && getMasterWorkspaceIndex(master) == getWorkspaceIndexOfWindow(winInfo) && isFocusable(winInfo) &&
            focusWindowInfoAsMaster(winInfo, master))
            return 1;
    }
    FOR_EACH(WindowInfo*, winInfo, getMasterWindowStack(master)) {
        if(winInfo != ignore && isNotInInvisibleWorkspace(winInfo) && isFocusable(winInfo) &&
            focusWindowInfoAsMaster(winInfo, master))
            return 1;
    }
    return 0;
}
void updateFocusForAllMasters(WindowInfo* winInfo) {
    FOR_EACH(Master*, master, getAllMasters()) {
        if(getFocusedWindowOfMaster(master) == winInfo) {
            DEBUG("Trying to update focus from %d for Master %d", (winInfo ? winInfo->id : 0), master->id);
            if(!focusNextVisibleWindow(master, winInfo))
                DEBUG("Could not find window to update focus for master %d", master->id);
            else {
                DEBUG("Updated focus for master %d", master->id);
                raiseWindowInfo(getFocusedWindowOfMaster(master), 0);
            }
        }
    }
}

bool unregisterWindow(WindowInfo* winInfo, bool unregisterForEvents) {
    if(!winInfo)
        return 0;
    DEBUG("window %d has been removed", winInfo->id);
    if(unregisterForEvents)
        unregisterForWindowEvents(winInfo->id);
    bool result = 0;
    applyEventRules(UNREGISTER_WINDOW, winInfo);
    freeWindowInfo(winInfo);
    return result;
}

/**
 * @return 1 iff whether the window is mapped doesn't match if its workspace is visible
 */
static inline bool isOutOfSyncWithWorkspace(WindowInfo* winInfo) {
    return getWorkspaceOfWindow(winInfo) ? isWorkspaceVisible(getWorkspaceOfWindow(winInfo)) ^
        (hasAndHasNotMasks(winInfo, MAPPED_MASK, HIDDEN_MASK)) : 0;
}

void updateWindowWorkspaceState(WindowInfo* winInfo) {
    if(!isOutOfSyncWithWorkspace(winInfo))
        return;
    DEBUG("updating window workspace state: Visible: %d; Window: %d", isWorkspaceVisible(getWorkspaceOfWindow(winInfo)),
        winInfo->id);
    if(isNotInInvisibleWorkspace(winInfo) && isMappable(winInfo)) {
        if(!hasMask(winInfo, MAPPED_MASK)) {
            mapWindow(winInfo->id);
            addMask(winInfo, MAPPABLE_MASK | MAPPED_MASK);
        }
    }
    else {
        updateFocusForAllMasters(winInfo);
        unmapWindow(winInfo->id);
        removeMask(winInfo, MAPPED_MASK);
    }
}

void switchToWorkspace(int workspaceIndex) {
    if(!isWorkspaceVisible(getWorkspace(workspaceIndex))) {
        int currentIndex = getActiveWorkspaceIndex();
        /*
         * Each master can have independent active workspaces. While an active workspace is generally visible when it is set,
         * it can become invisible due to another master switching workspaces. So when the master in the invisible workspaces
         * wants its workspace to become visible it must first find a visible workspace to swap with
         */
        if(!isWorkspaceVisible(getActiveWorkspace())) {
            FOR_EACH(Workspace*, workspace, getAllWorkspaces()) {
                if(isWorkspaceVisible(workspace)) {
                    currentIndex = workspace->id;
                    break;
                }
            }
        }
        DEBUG("Swapping visible workspace %d with %d", currentIndex, workspaceIndex);
        FOR_EACH_R(WindowInfo*, winInfo, getWorkspaceWindowStack(getWorkspace(currentIndex))) {
            if(hasMask(winInfo, STICKY_MASK))
                moveToWorkspace(winInfo, workspaceIndex);
        }
        swapMonitors(workspaceIndex, currentIndex);
    }
    setActiveWorkspaceIndex(workspaceIndex);
}

void activateWorkspace(WorkspaceID workspaceIndex) {
    switchToWorkspace(workspaceIndex);
    FOR_EACH(WindowInfo*, winInfo, getActiveMasterWindowStack()) {
        if(getWorkspaceIndexOfWindow(winInfo) == workspaceIndex) {
            activateWindow(winInfo);
            return;
        }
    }
    ArrayList* list = getWorkspaceWindowStack(getWorkspace(workspaceIndex));
    if(list->size)
        activateWindow(getHead(list));
    else
        INFO("activateWorkspace: no window was activated");
}


bool activateWindow(WindowInfo* winInfo) {
    if(winInfo && isActivatable(winInfo)) {
        raiseWindowInfo(winInfo, 0);
        DEBUG("activating window %d in workspace %d", winInfo->id, getWorkspaceIndexOfWindow(winInfo));
        if(getWorkspaceIndexOfWindow(winInfo) != NO_WORKSPACE) {
            switchToWorkspace(getWorkspaceIndexOfWindow(winInfo));
        }
        if(!hasMask(winInfo, MAPPED_MASK)) {
            mapWindow(winInfo->id);
        }
        return focusWindowInfo(winInfo);
    }
    return 0;
}
void activateWindowUnchecked(WindowInfo* winInfo) {
    activateWindow(winInfo);
}


void configureWindow(WindowID win, uint32_t mask, uint32_t values[7]) {
    assert(mask);
    assert(mask < 128);
    INFO("Config %d: mask %d (%d bits)", win, mask, __builtin_popcount(mask));
    LOG_RUN(LOG_LEVEL_INFO, PRINT_ARR("Config values", values, __builtin_popcount(mask)));
    XCALL(xcb_configure_window, dis, win, mask, values);
}
void setWindowPosition(WindowID win, const Rect geo) {
    uint32_t values[4];
    copyTo(&geo, 1, values);
    uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT ;
    configureWindow(win, mask, values);
}
static inline int filterConfigValues(uint32_t* filteredArr, const WindowInfo* winInfo, const short values[5],
    WindowID sibling,
    int stackMode, int configMask) {
    int i = 0;
    int n = 0;
    if(configMask & XCB_CONFIG_WINDOW_X && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_MOVE_MASK, MAPPED_MASK))
        filteredArr[i++] = values[n - 1];
    else configMask &= ~XCB_CONFIG_WINDOW_X;
    if(configMask & XCB_CONFIG_WINDOW_Y && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_MOVE_MASK, MAPPED_MASK))
        filteredArr[i++] = values[n - 1];
    else configMask &= ~XCB_CONFIG_WINDOW_Y;
    if(configMask & XCB_CONFIG_WINDOW_WIDTH && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_RESIZE_MASK, MAPPED_MASK))
        filteredArr[i++] = values[n - 1];
    else configMask &= ~XCB_CONFIG_WINDOW_WIDTH;
    if(configMask & XCB_CONFIG_WINDOW_HEIGHT && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_RESIZE_MASK, MAPPED_MASK))
        filteredArr[i++] = values[n - 1];
    else configMask &= ~XCB_CONFIG_WINDOW_HEIGHT;
    if(configMask & XCB_CONFIG_WINDOW_BORDER_WIDTH && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_BORDER_MASK, MAPPED_MASK))
        filteredArr[i++] = values[n - 1];
    else configMask &= ~XCB_CONFIG_WINDOW_BORDER_WIDTH;
    if(configMask & XCB_CONFIG_WINDOW_SIBLING && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_RAISE_MASK, MAPPED_MASK))
        filteredArr[i++] = sibling;
    else configMask &= ~XCB_CONFIG_WINDOW_SIBLING;
    if(configMask & XCB_CONFIG_WINDOW_STACK_MODE && ++n && hasOrHasNotMasks(winInfo, EXTERNAL_RAISE_MASK, MAPPED_MASK))
        filteredArr[i++] = stackMode;
    else configMask &= ~XCB_CONFIG_WINDOW_STACK_MODE;
    return configMask;
}
int processConfigureRequest(WindowID win, const short values[5], WindowID sibling, int stackMode, int configMask) {
    assert(configMask);
    DEBUG("processing configure request window %d", win);
    uint32_t actualValues[7];
    WindowInfo* winInfo = getWindowInfo(win);
    int mask = filterConfigValues(actualValues, winInfo, values, sibling, stackMode, configMask);
    DEBUG("Mask filtered from %d to %d", configMask, mask);
    if(mask) {
        configureWindow(win, mask, actualValues);
    }
    else {
        INFO("configure request denied for window %d; configMasks %d (%d)", win, mask, configMask);
    }
    return mask;
}


static WindowID windowDivider[2] = {0};
WindowID getWindowDivider(bool upper) {
    if(!windowDivider[0]) {
        windowDivider[0] = createOverrideRedirectWindow();
        lowerWindow(windowDivider[0], 0);
        windowDivider[1] = createOverrideRedirectWindow();
    }
    return windowDivider[upper];
}

void raiseLowerWindow(WindowID win, WindowID sibling, bool above) {
    uint32_t values[] = {sibling, above ? XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW};
    int mask = XCB_CONFIG_WINDOW_STACK_MODE;
    if(sibling)
        mask |= XCB_CONFIG_WINDOW_SIBLING;
    TRACE("Modifying window stacking order; win:  %d  sibling: %d Above: %d", win, sibling, (int)above);
    configureWindow(win, mask, &values[!sibling]);
}
void raiseLowerWindowInfo(WindowInfo* winInfo, WindowID sibling, bool above) {
    if(!sibling)
        if(!above  && hasPartOfMask(winInfo, TOP_LAYER_MASKS) || above && hasPartOfMask(winInfo, BOTTOM_LAYER_MASKS)) {
            // lowering an TOP_LAYER or raises a BOTTOM_LAYER is really raising/lowering it relative to the UPPER/LOWER divider
            above = !above;
            sibling = getWindowDivider(above);
        }
    // raising a non TOP_LAYER or lowering a BOTTOM_LAYER is really lowering/raising it relative UPPER/LOWER divider
        else if(above && !hasPartOfMask(winInfo, TOP_LAYER_MASKS) || !above && !hasMask(winInfo, BOTTOM_LAYER_MASKS)) {
            sibling = getWindowDivider(above);
            above = !above;
        }
    raiseLowerWindow(winInfo->id, sibling, above);
}
