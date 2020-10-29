#include "../globals.h"
#include "../xutil/device-grab.h"
#include "../xutil/test-functions.h"
#include "../system.h"
#include "../user-events.h"
#include "tester.h"
#include "test-x-helper.h"
#include "test-event-helper.h"

#include <unistd.h>

SCUTEST_ERR(no_display, 1) {
    assert(!hasXConnectionBeenOpened());
    setenv("DISPLAY", "1", 1);
    openXDisplay();
    assert(0);
}

SCUTEST_SET_ENV(openXDisplay, NULL);
SCUTEST(open_xdisplay) {
    assert(dis);
    assert(!xcb_connection_has_error(dis));
    assert(hasXConnectionBeenOpened());
    assert(screen);
    assert(root);
    assert(WM_DELETE_WINDOW);
    assert(!xcb_connection_has_error(dis));
    assert(!consumeEvents());
}
SCUTEST(get_key_code) {
    assert(dis);
    assert(getKeyCode(XK_A));
}

SCUTEST_ITER(get_set_atom, 2) {
    char buffer[MAX_NAME_LEN] = {0};
    char buffer2[MAX_NAME_LEN] = {0};
    for(int i = 0; i < (_i ? LEN(buffer) - 1 : 26); i++)
        buffer[i] = 'a' + (i % 26);
    xcb_atom_t test = getAtom(buffer);
    assert(test == getAtom(buffer));
    assertEqualsStr(buffer, getAtomName(test, buffer2));
}

SCUTEST(get_set_atom_bad) {
    assert(getAtom(NULL) == XCB_ATOM_NONE);
    assert(!getAtomName(-1, NULL));
}
SCUTEST(get_set_window_property) {
    for(int value = 0; value < 3; value++) {
        setWindowPropertyInt(root, ewmh->_NET_SUPPORTED, XCB_ATOM_CARDINAL, value);
        int v = getWindowPropertyValueInt(root, ewmh->_NET_SUPPORTED, XCB_ATOM_CARDINAL);
        assertEquals(value, v);
    }
}
SCUTEST(get_unset_window_property) {
    char buffer[1];
    assertEquals(0, getWindowPropertyValueInt(root, ewmh->_NET_SUPPORTED, XCB_ATOM_CARDINAL));
    assertEqualsStr("", getWindowPropertyString(root, WM_WINDOW_ROLE, XCB_ATOM_STRING, buffer));
}
SCUTEST(get_set_window_property_string) {
    char* buffer = "property";
    char buffer2[256] = {0};
    setWindowPropertyString(root, WM_WINDOW_ROLE, XCB_ATOM_STRING, buffer);
    assert(strcmp(buffer, getWindowPropertyString(root, WM_WINDOW_ROLE, XCB_ATOM_STRING, buffer2)) == 0);
}

SCUTEST(reopen_display) {
    closeConnection();
    openXDisplay();
    closeConnection();
    //will leak if vars aren't freed
    dis = NULL;
    ewmh = NULL;
    openXDisplay();
}

SCUTEST(create_destroy_window) {
    assert(!destroyWindow(createNormalWindow()));
}

SCUTEST(attemptToMapWindow) {
    WindowID win = createNormalWindow();
    assert(!isWindowMapped(win));
    mapWindow(win);
    assert(isWindowMapped(win));
    unmapWindow(win);
    assert(!isWindowMapped(win));
}

SCUTEST(private_window) {
    xcb_window_t win = getPrivateWindow();
    assert(win == getPrivateWindow());
    assert(xcb_request_check(dis, xcb_map_window_checked(dis, win)) == NULL);
    uint32_t pid;
    xcb_ewmh_get_wm_pid_reply(ewmh, xcb_ewmh_get_wm_pid(ewmh, getPrivateWindow()), &pid, NULL);
    assert(pid);
    assertEquals(pid, getpid());
}

SCUTEST(event_names) {
    for(int i = 0; i < NUMBER_OF_MPX_EVENTS; i++)
        assert(eventTypeToString(i));
}
SCUTEST(event_attributes) {
    for(int i = 0; i < LASTEvent; i++)
        assert(opcodeToString(i));
}
SCUTEST(catch_error_silent) {
    xcb_window_t win = createNormalWindow();
    CRASH_ON_ERRORS = -1;
    assert(catchErrorSilent(xcb_map_window_checked(dis, win)) == 0);
    assert(catchErrorSilent(xcb_map_window_checked(dis, 0)) == BadWindow);
}
SCUTEST_ITER_ERR(test_catch_error_silent, 2, 1) {
    xcb_window_t win = createNormalWindow();
    CRASH_ON_ERRORS = _i == 0 ? -1 : 0;
    suppressOutput();
    assert(catchError(xcb_map_window_checked(dis, win)) == 0);
    assert(catchError(xcb_map_window_checked(dis, 0)) == BadWindow);
    assert(_i);
    exit(1);
}

SCUTEST_ITER_ERR(crash_on_error, 2, 1) {
    CRASH_ON_ERRORS = -1;
    suppressOutput();
    if(_i) {
        grabDevice(100, 0);
        XSync(dpy, 0);
        assert(0);
    }
    else
        assert(catchError(xcb_map_window_checked(dis, 0)) == BadWindow);
}
SCUTEST(load_unknown_generic_events) {
    xcb_ge_generic_event_t event = {0};
    assert(loadGenericEvent(&event) == 0);
}
SCUTEST(load_generic_events) {
    addDefaultMaster();
    grabActivePointer();
    clickButton(1, getActiveMasterPointerID());
    xcb_ge_generic_event_t* event = getNextDeviceEvent();
    assertEquals(event->event_type, XCB_INPUT_BUTTON_PRESS);
    free(event);
    event = (xcb_ge_generic_event_t*)getNextDeviceEvent();
    assertEquals(event->event_type, XCB_INPUT_BUTTON_RELEASE);
    free(event);
}
SCUTEST_ITER(getButtonOrKey, 2) {
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
}
/**
 * Tests to see if events received actually make it to call back function
 * @param SCUTEST("test_regular_events",
 */
/*
*/
/*

SCUTEST(test_event_spam) {
    addDefaultMaster();
    grabPointer();
    EVENT_PERIOD = 5;
    POLL_COUNT = 100;
    POLL_INTERVAL = 10;
    getEventRules(PERIODIC).add(DEFAULT_EVENT(requestShutdown));
    getEventRules(IDLE).add(DEFAULT_EVENT(exitFailure));
    startWM();
    while(!isShuttingDown()) {
        lock();
        assert(!destroyWindow(createNormalWindow()));
        flush();
        unlock();
    }
}
SCUTEST(spam_mouse_motion) {
    addDefaultMaster();
    grabPointer();
    static int num = 1000;
    getBatchEventRules(XCB_GE_GENERIC).add(DEFAULT_EVENT(+[] {assertEquals(getNumberOfEventsTriggerSinceLastIdle(XCB_GE_GENERIC), 2 * num);}));
    generateMotionEvents(num);
    startWM();
    waitUntilIdle();
    assert(!consumeEvents());
}
SCUTEST(free_events_on_exit) {
    addDefaultMaster();
    grabPointer();
    generateMotionEvents(10000);
    getEventRules(XCB_GE_GENERIC).add(DEFAULT_EVENT(requestShutdown));
    runEventLoop();
    assert(consumeEvents());
}
SCUTEST(true_idle) {
    POLL_COUNT = 1;
    POLL_INTERVAL = 10;
    registerForWindowEvents(root, ROOT_EVENT_MASKS);
    getEventRules(TRUE_IDLE).add(DEFAULT_EVENT(requestShutdown));
    static int counter = 0;
    getEventRules(IDLE).add({(void(*)())[]() {if(++counter < 10)createNormalWindow();}, "_spam"});
    startWM();
    WAIT_UNTIL_TRUE(isShuttingDown());
    assertEquals(counter, 10);
}
*/
/* move to EWMH
SCUTEST(atom_mask_mapping) {
    int count = 0;
    for(int i = 0; i < 32; i++) {
        WindowMask mask = 1 << i;
        ArrayList<xcb_atom_t> atom;
        getAtomsFromMask(mask, atom);
        if(atom.size()) {
            assertEquals(getMaskFromAtom(atom[0]), mask);
            count++;
        }
    }
    assert(count);
    ArrayList<xcb_atom_t> s;
    getAtomsFromMask(0, s);
    assert(!s.size());
    assertEquals(getMaskFromAtom(0), NO_MASK);
}
*/
