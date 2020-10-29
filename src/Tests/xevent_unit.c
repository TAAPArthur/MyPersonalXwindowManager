#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>

#include "tester.h"
#include "test-x-helper.h"
#include "test-event-helper.h"

static void setup() {
    openXDisplay();
    addShutdownOnIdleRule();
    addXIEventSupport();
    ROOT_DEVICE_EVENT_MASKS = XCB_INPUT_XI_EVENT_MASK_HIERARCHY;
    registerForWindowEvents(root, ROOT_EVENT_MASKS);
    passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
}
SCUTEST_SET_ENV(setup, simpleCleanup);

static void test_synthetic_event_helper(xcb_generic_event_t* event) {
    assert(isSyntheticEvent(event));
    incrementCount();
}

SCUTEST(test_synthetic_event) {
    xcb_generic_event_t event = {0};
    event.response_type = XCB_UNMAP_NOTIFY;
    addEvent(XCB_UNMAP_NOTIFY, DEFAULT_EVENT(test_synthetic_event_helper));
    assert(!catchError(xcb_send_event_checked(dis, 0, root, ROOT_EVENT_MASKS, (char*) &event)));
    runEventLoop();
    assert(getCount());
}
SCUTEST(test_idle, .iter = 2, .timeout = 1) {
    if(_i)
        addEvent(IDLE, DEFAULT_EVENT(incrementCount));
    else
        addEvent(TRUE_IDLE, DEFAULT_EVENT(incrementCount));
    runEventLoop();
}

static void test_xi_event_helper(xcb_input_hierarchy_event_t* event) {
    assert(event);
    incrementCount();
    assertEquals(event->event_type, XCB_INPUT_HIERARCHY);
}

SCUTEST(test_xi_event) {
    createMasterDevice("test");
    addEvent(XCB_GE_GENERIC + XCB_INPUT_HIERARCHY, DEFAULT_EVENT(test_xi_event_helper));
    runEventLoop();
    assert(getCount());
}

SCUTEST(test_backlog_of_events, .iter = 3, .timeout = 5) {
    int totalEvents = MPX_EVENT_QUEUE_SIZE * (_i + 1);
    addEvent(XCB_UNMAP_NOTIFY, DEFAULT_EVENT(incrementCount));
    xcb_generic_event_t event = {.response_type = XCB_UNMAP_NOTIFY};
    for(int i = 0; i < totalEvents ; i++)
        xcb_send_event(dis, 0, root, ROOT_EVENT_MASKS, (char*) &event);
    runEventLoop();
    assertEquals(totalEvents, getCount());
}

SCUTEST_SET_ENV(NULL, simpleCleanup, .timeout = 1);
static int fds[4];
static void verifyFDs(int i, int mask) {
    assertEquals(fds[0], i);
    assertEquals(mask, POLLIN);
}
SCUTEST(test_extra_events) {
    pipe(fds);
    pipe(fds + 2);
    if(fork()) {
        addExtraEvent(fds[0], POLLIN, incrementCount);
        addExtraEvent(fds[0], POLLIN, verifyFDs);
        addExtraEvent(fds[0], POLLIN, incrementCount);
        addExtraEvent(fds[2], POLLIN, requestShutdown);
        openXDisplay();
        runEventLoop();
        assertEquals(2, getCount());
        assertEquals(0, waitForChild(0));
        return;
    }
    // wakeup WM
    write(fds[1], &fds, sizeof(int));
    write(fds[1], &fds, sizeof(int));
    write(fds[3], &fds, sizeof(int));
    exit(0);
}
SCUTEST(test_extra_events_close) {
    pipe(fds);
    pipe(fds + 2);
    if(fork()) {
        close(fds[1]);
        addExtraEvent(fds[0], POLLIN | POLLHUP, incrementCount);
        addExtraEvent(fds[1], POLLIN, incrementCount);
        addExtraEvent(fds[2], POLLIN, requestShutdown);
        addExtraEvent(fds[0], POLLIN, incrementCount);
        openXDisplay();
        runEventLoop();
        assertEquals(1, getCount());
        assertEquals(0, waitForChild(0));
        return;
    }
    close(fds[0]);
    close(fds[1]);
    // wakeup WM
    write(fds[3], &fds, sizeof(int));
    exit(0);
}
