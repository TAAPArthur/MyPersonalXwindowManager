#include "../bindings.h"
#include "../devices.h"
#include "../logger.h"
#include "../masters.h"
#include "../monitors.h"
#include "../mywm-structs.h"
#include "../system.h"
#include "../user-events.h"
#include "../window-properties.h"
#include "../windows.h"
#include "../wmfunctions.h"
#include "../workspaces.h"
#include "experimental-rules.h"

void addDieOnIdleRule(AddFlag flag) {
    getEventRules(TRUE_IDLE).add(DEFAULT_EVENT(+[]() {quit(0);}), flag);
}
void addFloatRule(AddFlag flag) {
    getEventRules(CLIENT_MAP_ALLOW).add(new BoundFunction(+[](WindowInfo * winInfo) {
        if(winInfo->getType() != ewmh->_NET_WM_WINDOW_TYPE_NORMAL &&
            winInfo->getType()  != ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) floatWindow(winInfo);
    }, FUNC_NAME), flag);
}

void addFocusFollowsMouseRule(AddFlag flag) {
    NON_ROOT_DEVICE_EVENT_MASKS |= XCB_INPUT_XI_EVENT_MASK_ENTER;
    getEventRules(GENERIC_EVENT_OFFSET + XCB_INPUT_ENTER).add(DEFAULT_EVENT(focusFollowMouse), flag);
}

void addScanChildrenRule(AddFlag flag) {
    getEventRules(POST_REGISTER_WINDOW).add(DEFAULT_EVENT(scan), flag);
}

void focusFollowMouse() {
    xcb_input_enter_event_t* event = (xcb_input_enter_event_t*)getLastEvent();
    setActiveMasterByDeviceID(event->deviceid);
    WindowID win = event->event;
    WindowInfo* winInfo = getWindowInfo(win);
    LOG(LOG_LEVEL_DEBUG, "focus following mouse %d win %d", getActiveMaster()->getID(), win);
    if(winInfo)
        focusWindow(winInfo);
}
