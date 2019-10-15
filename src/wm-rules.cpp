/**
 * @file wm-rules.cpp
 * @copybrief wm-rules.h
 *
 */

#include <assert.h>

#include "mywm-structs.h"
#include "bindings.h"
#include "wm-rules.h"
#include "devices.h"
#include "globals.h"
#include "layouts.h"
#include "logger.h"
#include "masters.h"
#include "monitors.h"
#include "state.h"
#include "system.h"
#include "time.h"
#include "user-events.h"
#include "window-properties.h"
#include "windows.h"
#include "wmfunctions.h"
#include "workspaces.h"
#include "xsession.h"

void onXConnect(void) {
    addDefaultMaster();
    initCurrentMasters();
    assert(getActiveMaster() != NULL);
    detectMonitors();
    registerForEvents();
    scan(root);
}

void onHiearchyChangeEvent(void) {
    xcb_input_hierarchy_event_t* event = (xcb_input_hierarchy_event_t*)getLastEvent();
    if(event->flags & (XCB_INPUT_HIERARCHY_MASK_MASTER_ADDED | XCB_INPUT_HIERARCHY_MASK_SLAVE_ADDED)) {
        LOG(LOG_LEVEL_DEBUG, "detected new master\n");
        initCurrentMasters();
        return;
    }
    xcb_input_hierarchy_info_iterator_t iter = xcb_input_hierarchy_infos_iterator(event);
    while(iter.rem) {
        if(iter.data->flags & XCB_INPUT_HIERARCHY_MASK_MASTER_REMOVED) {
            LOG(LOG_LEVEL_DEBUG, "Master %d %d has been removed\n", iter.data->deviceid, iter.data->attachment);
            delete getAllMasters().removeElement(iter.data->deviceid);
        }
        else if(iter.data->flags & XCB_INPUT_HIERARCHY_MASK_SLAVE_REMOVED) {
            LOG(LOG_LEVEL_DEBUG, "Slave %d %d has been removed\n", iter.data->deviceid, iter.data->attachment);
            delete getAllSlaves().removeElement(iter.data->deviceid);
        }
        else if(iter.data->flags & XCB_INPUT_HIERARCHY_MASK_SLAVE_ATTACHED) {
            getAllSlaves().find(iter.data->deviceid)->setMasterID(iter.data->attachment);
        }
        else if(iter.data->flags & XCB_INPUT_HIERARCHY_MASK_SLAVE_REMOVED) {
            getAllSlaves().find(iter.data->deviceid)->setMasterID(0);
        }
        xcb_input_hierarchy_info_next(&iter);
    }
}

void onError(void) {
    LOG(LOG_LEVEL_ERROR, "error received in event loop\n");
    logError((xcb_generic_error_t*)getLastEvent());
}
void onConfigureNotifyEvent(void) {
    xcb_configure_notify_event_t* event = (xcb_configure_notify_event_t*)getLastEvent();
    if(event->override_redirect)return;
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo) {
        winInfo->setGeometry(&event->x);
        applyEventRules(onWindowMove, winInfo);
    }
}
void onConfigureRequestEvent(void) {
    xcb_configure_request_event_t* event = (xcb_configure_request_event_t*)getLastEvent();
    short values[5];
    int n = 0;
    for(int i = 0; i < LEN(values); i++)
        if(event->value_mask & (1 << i))
            values[n++] = (&event->x)[i];
    processConfigureRequest(event->window, values, event->sibling, event->stack_mode, event->value_mask);
}
bool addIgnoreOverrideRedirectWindowsRule(AddFlag flag) {
    return getEventRules(PreRegisterWindow).add({
        +[](WindowInfo * winInfo) {return !winInfo->isOverrideRedirectWindow();},
        FUNC_NAME,
        PASSTHROUGH_IF_TRUE
    }, flag);
}
static void onWindowDetection(WindowID id, WindowID parent, short* geo) {
    if(registerWindow(id, parent)) {
        WindowInfo* winInfo = getWindowInfo(id);
        if(geo)
            winInfo->setGeometry(geo);
        if(!IGNORE_SUBWINDOWS)
            scan(id);
        applyEventRules(onWindowMove, winInfo);
    }
}
void onCreateEvent(void) {
    xcb_create_notify_event_t* event = (xcb_create_notify_event_t*)getLastEvent();
    LOG(LOG_LEVEL_VERBOSE, "Detected create event for Window %d\n", event->window);
    if(getWindowInfo(event->window)) {
        LOG(LOG_LEVEL_VERBOSE, "Window %d is already in our records; Ignoring create event\n", event->window);
        return;
    }
    LOG(LOG_LEVEL_WARN, "%d %d\n", IGNORE_SUBWINDOWS, event->parent != root);
    if(IGNORE_SUBWINDOWS && event->parent != root) {
        LOG(LOG_LEVEL_VERBOSE, "Window %d's parent is not root but %d; Ignoring\n", event->window, event->parent);
        return;
    }
    onWindowDetection(event->window, event->parent, &event->x);
}
void onDestroyEvent(void) {
    xcb_destroy_notify_event_t* event = (xcb_destroy_notify_event_t*)getLastEvent();
    LOG(LOG_LEVEL_VERBOSE, "Detected destroy event for Window %d\n", event->window);
    unregisterWindow(getWindowInfo(event->window));
}
void onVisibilityEvent(void) {
    xcb_visibility_notify_event_t* event = (xcb_visibility_notify_event_t*)getLastEvent();
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo)
        if(event->state == XCB_VISIBILITY_FULLY_OBSCURED)
            winInfo->removeMask(FULLY_VISIBLE);
        else if(event->state == XCB_VISIBILITY_UNOBSCURED)
            winInfo->addMask(FULLY_VISIBLE);
        else {
            winInfo->removeMask(FULLY_VISIBLE);
            winInfo->addMask(PARTIALLY_VISIBLE);
        }
}
void onMapEvent(void) {
    xcb_map_notify_event_t* event = (xcb_map_notify_event_t*)getLastEvent();
    LOG(LOG_LEVEL_VERBOSE, "Detected map event for Window %d\n", event->window);
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo) {
        if(!winInfo->hasMask(MAPPABLE_MASK))
            applyEventRules(ClientMapAllow, winInfo);
        winInfo->addMask(MAPPABLE_MASK | MAPPED_MASK);
    }
}
void onMapRequestEvent(void) {
    xcb_map_request_event_t* event = (xcb_map_request_event_t*)getLastEvent();
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo) {
        winInfo->addMask(MAPPABLE_MASK);
        applyEventRules(ClientMapAllow, winInfo);
    }
    attemptToMapWindow(event->window);
}
void onUnmapEvent(void) {
    xcb_unmap_notify_event_t* event = (xcb_unmap_notify_event_t*)getLastEvent();
    LOG(LOG_LEVEL_VERBOSE, "Detected unmap event for Window %d\n", event->window);
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo) {
        winInfo->removeMask(FULLY_VISIBLE | MAPPED_MASK);
        // used to indicate that the window is no longer mappable
        if(isSyntheticEvent())
            winInfo->removeMask(MAPPABLE_MASK);
    }
}
void onReparentEvent(void) {
    xcb_reparent_notify_event_t* event = (xcb_reparent_notify_event_t*)getLastEvent();
    if(IGNORE_SUBWINDOWS) {
        if(event->parent == root)
            onWindowDetection(event->window, event->parent, NULL);
        else
            unregisterWindow(getWindowInfo(event->window));
    }
}

void onFocusInEvent(void) {
    xcb_input_focus_in_event_t* event = (xcb_input_focus_in_event_t*)getLastEvent();
    setActiveMasterByDeviceId(event->deviceid);
    WindowInfo* winInfo = getWindowInfo(event->event);
    if(winInfo) {
        if(!winInfo->hasMask(NO_RECORD_FOCUS))
            getActiveMaster()->onWindowFocus(winInfo->getID());
        setBorder(winInfo->getID());
    }
}

void onFocusOutEvent(void) {
    xcb_input_focus_out_event_t* event = (xcb_input_focus_out_event_t*)getLastEvent();
    setActiveMasterByDeviceId(event->deviceid);
    WindowInfo* winInfo = getWindowInfo(event->event);
    if(winInfo)
        resetBorder(winInfo->getID());
}

void onPropertyEvent(void) {
    xcb_property_notify_event_t* event = (xcb_property_notify_event_t*)getLastEvent();
    WindowInfo* winInfo = getWindowInfo(event->window);
    if(winInfo) {
        if(event->atom == ewmh->_NET_WM_STRUT || event->atom == ewmh->_NET_WM_STRUT_PARTIAL) {
            loadDockProperties(winInfo);
            markState();
        }
        else if(event->atom == ewmh->_NET_WM_USER_TIME);
        else loadWindowProperties(winInfo);
    }
}


static WindowInfo* getTargetWindow(int root, int event, int child) {
    int i;
    int list[] = {0, root, event, child};
    for(i = LEN(list) - 1; i >= 1 && !list[i]; i--);
    return getWindowInfo(list[i]);
}
void onDeviceEvent(void) {
    xcb_input_key_press_event_t* event = (xcb_input_key_press_event_t*)getLastEvent();
    LOG(LOG_LEVEL_VERBOSE, "device event seq: %d type: %d id %d (%d) flags %d windows: %d %d %d\n",
        event->sequence, event->event_type, event->deviceid, event->sourceid, event->flags,
        event->root, event->event, event->child);
    setActiveMasterByDeviceId(event->deviceid);
    if((event->flags & XCB_INPUT_KEY_EVENT_FLAGS_KEY_REPEAT) && getActiveMaster()->isIgnoreKeyRepeat())
        return;
    // TODO move/resize window setLastKnowMasterPosition(event->root_x >> 16, event->root_y >> 16);
    UserEvent userEvent = {event->mods.effective, event->detail, 1U << event->event_type,
                           (bool)((event->flags & XCB_INPUT_KEY_EVENT_FLAGS_KEY_REPEAT) ? 1 : 0),
                           .winInfo = getTargetWindow(event->root, event->event, event->child),
                          };
    checkBindings(userEvent);
}

void onGenericEvent(void) {
    int type = loadGenericEvent((xcb_ge_generic_event_t*)getLastEvent());
    if(type)
        applyEventRules(type, NULL);
}

void onSelectionClearEvent(void) {
    xcb_selection_clear_event_t* event = (xcb_selection_clear_event_t*)getLastEvent();
    if(event->owner == getPrivateWindow() && event->selection == WM_SELECTION_ATOM) {
        LOG(LOG_LEVEL_INFO, "We lost the WM_SELECTION; another window manager is taking over (%d)", event->owner);
        quit(0);
    }
}


void registerForEvents() {
    if(ROOT_EVENT_MASKS)
        registerForWindowEvents(root, ROOT_EVENT_MASKS);
    ArrayList<Binding*>& list = getDeviceBindings();
    LOG(LOG_LEVEL_DEBUG, "Grabbing %d buttons/keys\n", list.size());
    for(Binding* binding : list) {
        binding->grab();
    }
    LOG(LOG_LEVEL_DEBUG, "listening for device event;  masks: %d\n", ROOT_DEVICE_EVENT_MASKS);
    if(ROOT_DEVICE_EVENT_MASKS)
        passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
    registerForMonitorChange();
}
bool listenForNonRootEventsFromWindow(WindowInfo* winInfo) {
    uint32_t mask = winInfo->getEventMasks() ? winInfo->getEventMasks() : NON_ROOT_EVENT_MASKS;
    if(registerForWindowEvents(winInfo->getID(), mask) == 0) {
        passiveGrab(winInfo->getID(), NON_ROOT_DEVICE_EVENT_MASKS);
        LOG(LOG_LEVEL_DEBUG, "Listening for events %d on %d\n", NON_ROOT_EVENT_MASKS, winInfo->getID());
        return 1;
    }
    LOG(LOG_LEVEL_DEBUG, "Could not register for events %d\n", winInfo->getID());
    return 0;
}

void addAutoTileRules(AddFlag flag) {
    int events[] = {XCB_MAP_NOTIFY, XCB_UNMAP_NOTIFY, XCB_DESTROY_NOTIFY,
                    XCB_INPUT_KEY_PRESS + GENERIC_EVENT_OFFSET, XCB_INPUT_KEY_RELEASE + GENERIC_EVENT_OFFSET,
                    XCB_INPUT_BUTTON_PRESS + GENERIC_EVENT_OFFSET, XCB_INPUT_BUTTON_RELEASE + GENERIC_EVENT_OFFSET,
                    XCB_CLIENT_MESSAGE,
                    onScreenChange,
                   };
    for(int i = 0; i < LEN(events); i++)
        getEventRules(events[i]).add(DEFAULT_EVENT(markState), flag);
    getEventRules(onXConnection).add(PASSTHROUGH_EVENT(updateState, ALWAYS_PASSTHROUGH), flag);
    getEventRules(Periodic).add(PASSTHROUGH_EVENT(updateState, ALWAYS_PASSTHROUGH), flag);
    getEventRules(TileWorkspace).add(DEFAULT_EVENT(unmarkState), flag);
}
void assignDefaultLayoutsToWorkspace() {
    for(Workspace* w : getAllWorkspaces())
        if(w->getLayouts().size() == 0 && w->getActiveLayout() == NULL) {
            for(Layout* layout : getDefaultLayouts())
                w->getLayouts().add(layout);
            w->setActiveLayout(getDefaultLayouts()[0]);
        }
}

void addBasicRules(AddFlag flag) {
    getEventRules(0).add(DEFAULT_EVENT(onError), flag);
    getEventRules(XCB_VISIBILITY_NOTIFY).add(DEFAULT_EVENT(onVisibilityEvent), flag);
    getEventRules(XCB_CREATE_NOTIFY).add(DEFAULT_EVENT(onCreateEvent), flag);
    getEventRules(XCB_DESTROY_NOTIFY).add(DEFAULT_EVENT(onDestroyEvent), flag);
    getEventRules(XCB_UNMAP_NOTIFY).add(DEFAULT_EVENT(onUnmapEvent), flag);
    getEventRules(XCB_MAP_NOTIFY).add(DEFAULT_EVENT(onMapEvent), flag);
    getEventRules(XCB_MAP_REQUEST).add(DEFAULT_EVENT(onMapRequestEvent), flag);
    getEventRules(XCB_REPARENT_NOTIFY).add(DEFAULT_EVENT(onReparentEvent), flag);
    getEventRules(XCB_CONFIGURE_REQUEST).add(DEFAULT_EVENT(onConfigureRequestEvent), flag);
    getEventRules(XCB_CONFIGURE_NOTIFY).add(DEFAULT_EVENT(onConfigureNotifyEvent), flag);
    getEventRules(XCB_PROPERTY_NOTIFY).add(DEFAULT_EVENT(onPropertyEvent), flag);
    getEventRules(XCB_SELECTION_CLEAR).add(DEFAULT_EVENT(onSelectionClearEvent), flag);
    getEventRules(XCB_GE_GENERIC).add(DEFAULT_EVENT(onGenericEvent), flag);
    getEventRules(XCB_INPUT_FOCUS_IN + GENERIC_EVENT_OFFSET).add(DEFAULT_EVENT(onFocusInEvent), flag);
    getEventRules(XCB_INPUT_FOCUS_OUT + GENERIC_EVENT_OFFSET).add(DEFAULT_EVENT(onFocusOutEvent), flag);
    getEventRules(XCB_INPUT_HIERARCHY + GENERIC_EVENT_OFFSET).add(DEFAULT_EVENT(onHiearchyChangeEvent), flag);
    getEventRules(onXConnection).add(DEFAULT_EVENT(onXConnect), flag);
    getEventRules(onXConnection).add(DEFAULT_EVENT(assignDefaultLayoutsToWorkspace), flag);
    addIgnoreOverrideRedirectWindowsRule(flag);
    getEventRules(PreRegisterWindow).add(DEFAULT_EVENT(listenForNonRootEventsFromWindow), flag);
    getEventRules(PostRegisterWindow).add({+[](WindowInfo * winInfo) {if(winInfo->getWorkspace() == NULL)winInfo->moveToWorkspace(getActiveWorkspaceIndex());}, "_autoAddToWorkspace", PASSTHROUGH_IF_TRUE},
    flag);
    getEventRules(onScreenChange).add(DEFAULT_EVENT(detectMonitors), flag);
    getEventRules(ClientMapAllow).add(DEFAULT_EVENT(loadWindowProperties), flag);
    for(int i = XCB_INPUT_KEY_PRESS; i <= XCB_INPUT_MOTION; i++) {
        getEventRules(GENERIC_EVENT_OFFSET + i).add(DEFAULT_EVENT(onDeviceEvent), flag);
    }
}
