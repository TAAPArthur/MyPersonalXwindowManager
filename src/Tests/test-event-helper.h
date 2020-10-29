#ifndef MPX_TEST_EVENT_HELPER
#define MPX_TEST_EVENT_HELPER

#include <assert.h>
#include "../bindings.h"
#include "../xutil/device-grab.h"
#include "../globals.h"
#include "../masters.h"
#include "../system.h"
#include "../xevent.h"
#include "../xutil/test-functions.h"
#include "../user-events.h"
#include "../util/time.h"
#include "test-mpx-helper.h"
#include "test-x-helper.h"
#include "tester.h"

static inline void exitFailure() {
    quit(10);
}

static inline void* getNextDeviceEvent() {
    flush();
    xcb_generic_event_t* event = xcb_wait_for_event(dis);
    DEBUG("Event received %d", ((xcb_generic_event_t*)event)->response_type);
    if(((xcb_generic_event_t*)event)->response_type == XCB_GE_GENERIC) {
        int type = loadGenericEvent((xcb_ge_generic_event_t*)event);
        xcb_input_key_press_event_t* dEvent = (xcb_input_key_press_event_t*) event;
        assertEquals(type, GENERIC_EVENT_OFFSET + dEvent->event_type);
        DEBUG("Detail %d Device type %d (%d) %s", dEvent->detail, dEvent->deviceid, dEvent->sourceid,
            eventTypeToString(GENERIC_EVENT_OFFSET + dEvent->event_type));
    }
    else if(((xcb_generic_event_t*)event)->response_type == 0) {
        ERROR("error waiting on event");
        applyEventRules(0, &event);
    }
    return event;
}
static inline void waitToReceiveInput(int mask, int detailMask) {
    flush();
    TRACE("waiting for input %d\n\n", mask);
    while(mask || detailMask) {
        xcb_input_key_press_event_t* e = (xcb_input_key_press_event_t*)getNextDeviceEvent();
        TRACE("type %d (%d); detail %d remaining mask:%d %d\n", e->response_type, (1 << e->event_type),
            e->detail,
            mask, detailMask);
        mask &= ~(1 << e->event_type);
        detailMask &= ~(1 << e->detail);
        free(e);
    }
}

static inline int waitForNormalEvent(int type) {
    flush();
    TRACE("Waiting for event of type %d\n", type);
    while(type) {
        xcb_generic_event_t* e = xcb_wait_for_event(dis);
        TRACE("Found event %p\n", e);
        if(!e)
            return 0;
        TRACE("type %d (%d) %s\n", e->response_type, e->response_type & 127,
            eventTypeToString(e->response_type & 127));
        int t = e->response_type & 127;
        free(e);
        if(type == t)
            break;
    }
    return 1;
}
static inline void generateMotionEvents(int num) {
    for(int i = 0; i < num; i++) {
        movePointerRelative(1, 1);
        movePointerRelative(-1, -1);
    }
    flush();
}
#endif
