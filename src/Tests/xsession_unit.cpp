#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/keysym.h>
#include <string>

#include "tester.h"
#include "test-x-helper.h"
#include "test-event-helper.h"

#include "../user-events.h"
#include "../globals.h"
#include "../xsession.h"
#include "../system.h"
#include "../logger.h"
#include "../device-grab.h"

MPX_TEST_ERR("no_display", 1, {
    suppressOutput();
    setenv("DISPLAY", "1", 1);
    openXDisplay();
    assert(0);
});

SET_ENV(openXDisplay, NULL);
MPX_TEST("open_xdisplay", {
    assert(dis);
    assert(!xcb_connection_has_error(dis));
    assert(screen);
    assert(root);
    assert(WM_DELETE_WINDOW);
    assert(WM_TAKE_FOCUS);
    assert(!xcb_connection_has_error(dis));
});
MPX_TEST("get_key_code", {
    assert(dis);
    assert(getKeyCode(XK_A));
});

MPX_TEST("get_set_atom", {
    char buffer[256];
    for(int i = 0; i < LEN(buffer); i++)
        buffer[i] = 'a';
    buffer[LEN(buffer) - 1] = 0;
    xcb_atom_t test = getAtom(buffer);
    assert(test == getAtom(buffer));
    std::string str = getAtomName(test);
    assert(str == buffer);
});

MPX_TEST("get_set_atom_bad", {
    assert(getAtom(NULL) == XCB_ATOM_NONE);
    std::string str = getAtomName(-1);
    assert(str == "");
});

MPX_TEST("get_max_devices", {
    int maxMasters = getMaxNumberOfMasterDevices();
    assertEquals(maxMasters, getMaxNumberOfMasterDevices());
    assertEquals(maxMasters, getMaxNumberOfMasterDevices(1));
    assert(maxMasters);
    assert(maxMasters * 4 <= getMaxNumberOfDevices());
});
MPX_TEST("get_max_devices_closed_display", {
    closeConnection();
    getMaxNumberOfMasterDevices();
});
MPX_TEST("reopen_display", {
    closeConnection();
    openXDisplay();
    closeConnection();
    //will leak if vars aren't freed
    dis = NULL;
    ewmh = NULL;
    openXDisplay();
});

MPX_TEST("create_destroy_window", {
    assert(!destroyWindow(createWindow()));
});

MPX_TEST("attempToMapWindow", {
    WindowID win = createWindow();
    assert(!isWindowMapped(win));
    mapWindow(win);
    assert(isWindowMapped(win));
    unmapWindow(win);
    assert(!isWindowMapped(win));
});

MPX_TEST("private_window", {
    xcb_window_t win = getPrivateWindow();
    assert(win == getPrivateWindow());
    assert(xcb_request_check(dis, xcb_map_window_checked(dis, win)) == NULL);
});

MPX_TEST("event_names", {
    for(int i = 0; i < MPX_LAST_EVENT; i++)
        assert(eventTypeToString(i));
});
MPX_TEST("event_attributes", {
    for(int i = 0; i < LASTEvent; i++)
        assert(opcodeToString(i));
});
MPX_TEST("dump_atoms", {
    suppressOutput();
    dumpAtoms(&WM_SELECTION_ATOM, 1);
    xcb_atom_t arr[] = {WM_DELETE_WINDOW, WM_TAKE_FOCUS};
    dumpAtoms(arr, 2);
});
MPX_TEST("catch_error_silient", {
    xcb_window_t win = createNormalWindow();
    CRASH_ON_ERRORS = -1;
    assert(catchErrorSilent(xcb_map_window_checked(dis, win)) == 0);
    assert(catchErrorSilent(xcb_map_window_checked(dis, 0)) == BadWindow);
});
MPX_TEST_ITER_ERR("catch_error_silient", 2, 1, {
    xcb_window_t win = createNormalWindow();
    CRASH_ON_ERRORS = _i == 0 ? -1 : 0;
    suppressOutput();
    assert(catchError(xcb_map_window_checked(dis, win)) == 0);
    assert(catchError(xcb_map_window_checked(dis, 0)) == BadWindow);
    assert(_i);
    exit(1);
});
MPX_TEST_ITER("catch_error_silient", 2, {
    xcb_window_t win = createNormalWindow();
    CRASH_ON_ERRORS = 0;
    suppressOutput();
    if(_i) {
        grabDevice(100, 0);
        XSync(dpy, 0);
        return;
    }
    assert(catchError(xcb_map_window_checked(dis, win)) == 0);
    assert(catchError(xcb_map_window_checked(dis, 0)) == BadWindow);
});

MPX_TEST_ITER_ERR("crash_on_error", 2, 1, {
    CRASH_ON_ERRORS = -1;
    suppressOutput();
    if(_i) {
        grabDevice(100, 0);
        XSync(dpy, 0);
        assert(0);
    }
    else
        assert(catchError(xcb_map_window_checked(dis, 0)) == BadWindow);
});
MPX_TEST("load_unknown_generic_events", {
    xcb_ge_generic_event_t event = {0};
    assert(loadGenericEvent(&event) == 0);
});
MPX_TEST("load_unknown_generic_events", {
    addDefaultMaster();
    grabPointer();
    clickButton(1);
    xcb_generic_event_t* event;
    event = xcb_wait_for_event(dis);
    assert(loadGenericEvent((xcb_ge_generic_event_t*)event) == GENERIC_EVENT_OFFSET + XCB_INPUT_BUTTON_PRESS);
    free(event);
    event = xcb_wait_for_event(dis);
    assert(loadGenericEvent((xcb_ge_generic_event_t*)event) == GENERIC_EVENT_OFFSET + XCB_INPUT_BUTTON_RELEASE);
    free(event);
});
MPX_TEST_ITER("getButtonOrKey", 2, {
    if(_i) {
        assert(getButtonDetailOrKeyCode(XK_A) > 8);
        assertEquals(getButtonDetailOrKeyCode(XK_A), getButtonDetailOrKeyCode(XK_A));
        assert(getButtonDetailOrKeyCode(XK_B) > 8);
        assert(getButtonDetailOrKeyCode(XK_A) != getButtonDetailOrKeyCode(XK_B));
    }
    else {
        for(int i = 1; i < 8; i++)
            assertEquals(getButtonDetailOrKeyCode(i), i);
    }
});
SET_ENV(openXDisplay, fullCleanup);
/**
 * Tests to see if events received actually make it to call back function
 * @param MPX_TEST("test_regular_events",
 */
MPX_TEST("test_regular_events", {
    static volatile int count = 2;
    static volatile int batchCount = -count;
    POLL_COUNT = 0;
    int idleCount = getIdleCount();
    xcb_generic_event_t event = {0};
    static int i;
    ROOT_DEVICE_EVENT_MASKS = XCB_INPUT_XI_EVENT_MASK_HIERARCHY;
    registerForWindowEvents(root, ROOT_EVENT_MASKS);
    passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
    startWM();
    uint32_t lastSeqNumber = 0;

    void (*func)(void) = []() {
        assertEquals(batchCount, -count);
        count++;
        if(i == ExtraEvent)
            return;
        assert(i == (((xcb_generic_event_t*)getLastEvent())->response_type & 127));
        if(i > 2 && i < XCB_GE_GENERIC)
            assert(isSyntheticEvent());
    };
    void (*batchFuncEven)(void) = []() {
        assert(batchCount-- % 2 == 0);
    };
    void (*batchFuncOdd)(void) = []() {
        assert(batchCount-- % 2 != 0);
    };
    for(i = count; i < LASTEvent; i++) {
        event.response_type = i;
        // sequence number doesn't appear to be set for some events for some reason
        event.sequence = lastSeqNumber + 1;
        getEventRules(i).add(DEFAULT_EVENT(func));
        getBatchEventRules(i).add(DEFAULT_EVENT((i % 2 == 0 ? batchFuncEven : batchFuncOdd)));
        switch(i) {
            case ExtraEvent:
                event.response_type = -1;
                xcb_send_event(dis, 0, root, ROOT_EVENT_MASKS, (char*) &event);
                break;
            case 0:
                //generate error
                xcb_send_event(dis, 0, root, ROOT_EVENT_MASKS, (char*) &event);
                break;
            case XCB_CLIENT_MESSAGE:
                assert(!catchError(xcb_ewmh_send_client_message(dis, root, root, 1, 0, 0)));
                break;
            case XCB_GE_GENERIC:
                createMasterDevice("test");
                break;
            default:
                assert(!catchError(xcb_send_event_checked(dis, 0, root, ROOT_EVENT_MASKS, (char*) &event)));
                break;
        }
        flush();
        WAIT_UNTIL_TRUE(getIdleCount() > idleCount);
        assertEquals(count, i + 1);
        assertEquals(batchCount, -count);
        assert(lastSeqNumber < getLastDetectedEventSequenceNumber());
        lastSeqNumber = getLastDetectedEventSequenceNumber();
        idleCount = getIdleCount();
    }
});

MPX_TEST("test_event_spam", {
    addDefaultMaster();
    grabPointer();
    EVENT_PERIOD = 5;
    POLL_COUNT = 100;
    POLL_INTERVAL = 10;
    getEventRules(Periodic).add(DEFAULT_EVENT(requestShutdown));
    getEventRules(Idle).add(DEFAULT_EVENT(exitFailure));
    startWM();
    while(!isShuttingDown()) {
        lock();
        clickButton(1);
        flush();
        unlock();
    }
});
MPX_TEST("spam_mouse_motion", {
    addDefaultMaster();
    grabPointer();
    static int num = 100000;
    getBatchEventRules(XCB_GE_GENERIC).add(DEFAULT_EVENT(+[]{assertEquals(getNumberOfEventsTriggerSinceLastIdle(XCB_GE_GENERIC), 2 * num);}));
    generateMotionEvents(num);
    startWM();
    waitUntilIdle();
    assert(!consumeEvents());
});
MPX_TEST("true_idle", {
    POLL_COUNT = 1;
    POLL_INTERVAL = 10;
    registerForWindowEvents(root, ROOT_EVENT_MASKS);
    getEventRules(TrueIdle).add(DEFAULT_EVENT(requestShutdown));
    static int counter = 0;
    getEventRules(Idle).add({(void(*)())[]() {if(++counter < 10)createNormalWindow();}, "_spam"});
    startWM();
    WAIT_UNTIL_TRUE(isShuttingDown());
    assertEquals(counter, 10);
});
MPX_TEST("atom_mask_mapping", {
    for(int i = 0; i < 32; i++) {
        WindowMask mask = 1 << i;
        xcb_atom_t atom;
        int count = getAtomsFromMask(mask, &atom);
        if(count) {
            assert(atom);
            assertEquals(getMaskFromAtom(atom), mask);
        }
    }
    assert(!getAtomsFromMask(0, NULL));
    assertEquals(getMaskFromAtom(0), NO_MASK);

});
