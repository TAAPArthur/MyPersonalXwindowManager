#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib-xcb.h>

#include "../../logger.h"
#include "../../globals.h"
#include "../../wm-rules.h"
#include "../../wmfunctions.h"
#include "../../functions.h"
#include "../../functions.h"
#include "../../devices.h"
#include "../../Extensions/mpx.h"
#include "../tester.h"
#include "../test-mpx-helper.h"
#include "../test-event-helper.h"
#include "../test-x-helper.h"

#define DIRNAME  "/tmp"
#define BASENAME "._dummy_mpx_info.txt"
static const char* ABS_PATH = DIRNAME "/" BASENAME;
static void mpxStartup(void) {
    MASTER_INFO_PATH = ABS_PATH;
    remove(ABS_PATH);
    onStartup();
}
SET_ENV(mpxStartup, fullCleanup);
MPX_TEST("test_start_mpx_empty", {
    //shouldn't crash
    startMPX();
    FILE* fp = fopen(MASTER_INFO_PATH.c_str(), "w");
    assert(fp);
    fclose(fp);
    assert(loadMPXMasterInfo());
    assert(loadMPXMasterInfo());
    startMPX();
});

MPX_TEST("test_save_load_mpx_bad", {
    assert(!loadMPXMasterInfo());
    assert(saveMPXMasterInfo());
    assert(loadMPXMasterInfo());
    assert(loadMPXMasterInfo());
    MASTER_INFO_PATH = "";
    assert(!saveMPXMasterInfo());
    assert(!loadMPXMasterInfo());
});
std::string names[] = {"t1", "t2", "t3", "t4"};
int colors[LEN(names)] = {0, 0xFF, 0xFF00, 0xFF0000};
int defaultColor = 0xABCDEF;
static void mpxResume() {
    setenv("HOME", DIRNAME, 1);
    MASTER_INFO_PATH = "~/" BASENAME;
    remove(ABS_PATH);
    onStartup();
    saveXSession();
    if(!fork()) {
        openXDisplay();
        getActiveMaster()->setFocusColor(defaultColor);
        for(auto name : names)
            createMasterDevice(name);
        initCurrentMasters();
        for(int i = 0; i < LEN(names); i++) {
            Master* m = getMasterByName(names[i]);
            assert(m);
            m->setFocusColor(colors[i]);
        }
        attachSlaveToMaster(getAllSlaves()[0], getMasterByName(names[0]));
        attachSlaveToMaster(getAllSlaves()[1], getMasterByName(names[1]));
        initCurrentMasters();
        createMasterDevice("_unseen_master_");
        assert(saveMPXMasterInfo());
        initCurrentMasters();
        destroyAllNonDefaultMasters();
        flush();
        fullCleanup();
        quit(0);
    }
    assertEquals(waitForChild(0), 0);
}
SET_ENV(mpxResume, fullCleanup);

MPX_TEST("save_load", {
    startMPX();
    assertEquals(getAllMasters().size(), LEN(names) + 1);
    assertEquals(getMasterByID(DEFAULT_KEYBOARD)->getFocusColor(), defaultColor);
    for(int i = 0; i < LEN(names); i++) {
        Master* m = getMasterByName(names[i]);
        assert(m);
        assertEquals(m->getFocusColor(), colors[i]);
    }
});

MPX_TEST("save_load_slaves", {
    startMPX();
    assertEquals(getAllMasters().size(), LEN(names) + 1);
    assert(getActiveMaster()->getSlaves().empty());
    assertEquals(getAllSlaves()[0]->getMaster(), getMasterByName(names[0]));
    assertEquals(getAllSlaves()[1]->getMaster(), getMasterByName(names[1]));

});
MPX_TEST("test_restart", {
    startMPX();
    createMasterDevice("test1");
    createMasterDevice("test1");
    createMasterDevice("test1");
    initCurrentMasters();
    assert(getAllMasters().size() > LEN(names) + 1 + 1);
    restartMPX();
    assertEquals(getAllMasters().size(), LEN(names) + 1);
});
MPX_TEST("auto_resume", {
    stopMPX();
    startWM();
    waitUntilIdle();
    lock();
    addAutoMPXRules();
    passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
    createMasterDevice(names[0]);
    unlock();
    consumeEvents();
    lock();
    applyEventRules(onXConnection);
    assertEquals(getAllSlaves()[0]->getMaster(), getMasterByName(names[0]));
    createMasterDevice(names[1]);
    unlock();
    waitUntilIdle();
    assertEquals(getAllSlaves()[1]->getMaster(), getMasterByName(names[1]));
    assertEquals(getAllMasters().size(), 3);
});
static void setup() {
    onStartup();
    startWM();
    waitUntilIdle();
}
SET_ENV(setup, fullCleanup);
MPX_TEST_ITER("test_split_master", 2, {
    static auto injectSlaveID = []() {
        xcb_input_key_press_event_t* event = (xcb_input_key_press_event_t*)getLastEvent();
        if(event->event_type == XCB_INPUT_MOTION) {
            event->sourceid = getMasterByID(event->deviceid, 0)->getSlaves()[0]->getID();
            return;
        }
        if(event->deviceid == event->sourceid)
            return;
        Master* m;
        if(m = getMasterByID(event->deviceid, 1))
            event->sourceid = m->getSlaves()[1]->getID();
        else if(m = getMasterByID(event->deviceid, 0))
            event->sourceid = m->getSlaves()[0]->getID();
        assert(m);
    };
    getEventRules(ProcessDeviceEvent).add(DEFAULT_EVENT(injectSlaveID), PREPEND_UNIQUE);
    Master* m = getActiveMaster();
    assertEquals(getAllMasters().size(), 1);
    assertEquals(m->getSlaves().size(), 2);
    assertEquals(getAllSlaves().size(), 2);
    splitMaster();
    sendKeyPress(getKeyCode(XK_A));
    if(_i)
        sendButtonPress(1);
    else {
        movePointer(0, 0);
        movePointer(1, 1);
    }
    waitUntilIdle();
    assertEquals(getAllMasters().size(), 2);
    assertEquals(m->getSlaves().size(), 0);
    assertEquals(getAllMasters()[1]->getSlaves().size(), 2);
});
