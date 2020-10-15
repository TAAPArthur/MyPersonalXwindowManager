#include "../Extensions/ewmh-client.h"
#include "../communications.h"
#include "../system.h"
#include "../windows.h"
#include "../wmfunctions.h"
#include "test-event-helper.h"
#include "test-wm-helper.h"
#include "tester.h"
#include <signal.h>


uint32_t wmPid;
static void restartWM() {
    WindowID win = getWMPrivateWindow();
    setWindowPropertyInt(win, WM_STATE, XCB_ATOM_CARDINAL, 1);
    send("restart", "");
    WAIT_UNTIL_TRUE(!hasOutStandingMessages());
    WAIT_UNTIL_TRUE(!getWindowPropertyValueInt(win, WM_STATE, XCB_ATOM_CARDINAL));
    WAIT_UNTIL_TRUE(isMPXManagerRunning());
    WAIT_UNTIL_TRUE(getWMIdleCount());
}
static void cleanup() {
    assert(wmPid);
    kill(wmPid, SIGINT);
    WAIT_UNTIL_TRUE(!isMPXManagerRunning());
}

static void initSetup() {
    wmPid = spawnChild("./mpxmanager --log-level 2");
    createXSimpleEnv();
    WAIT_UNTIL_TRUE(isMPXManagerRunning());
    waitUntilWMIdle();
}
SCUTEST_SET_ENV(initSetup, cleanup, .timeout = 4);
SCUTEST(remember_mapped_windows) {
    Window win = mapWindow(createNormalWindow());
    waitUntilWMIdle();
    assert(isWindowMapped(win));
    unmapWindow(win);
    restartWM();
    waitUntilWMIdle();
    assert(isWindowMapped(win));
}

SCUTEST(test_long_lived) {
    for(int i = 0; i < 3; i++) {
        mapWindow(createNormalWindow());
        waitUntilWMIdle();
    }
    assertEquals(0, spawnAndWait("./mpxmanager sum"));
}

SCUTEST(remember_focus, .iter = 2) {
    WindowID win = mapArbitraryWindow();
    waitUntilWMIdle();
    if(_i) {
        WindowID win2 = mapArbitraryWindow();
        waitUntilWMIdle();
        focusWindow(win2);
    }
    focusWindow(win);
    waitUntilWMIdle();
    assertEquals(getActiveFocus(), win);
    for(int i = 0; i < 2; i++) {
        restartWM();
        assert(isWindowMapped(win));
        assertEquals(getActiveFocus(), win);
    }
}

SCUTEST_ITER(wm_restart_recursive, 3) {
    for(int i = 0; i < 5; i++) {
        WindowID win = getWMPrivateWindow();
        setWindowPropertyInt(win, WM_WINDOW_ROLE, XCB_ATOM_CARDINAL, 1);
        if(_i == 0)
            restartWM();
        else if(_i == 1)
            kill(wmPid, SIGUSR1);
        else if(_i == 2)
            kill(wmPid, SIGHUP);
        WAIT_UNTIL_TRUE(!getWindowPropertyValueInt(win, WM_WINDOW_ROLE, XCB_ATOM_CARDINAL));
        WAIT_UNTIL_TRUE(isMPXManagerRunning());
    }
}
SCUTEST(test_send_as) {
    char buffer[255];
    createMasterDevice("test");
    createMasterDevice("test2");
    initCurrentMasters();
    sprintf(buffer, "./mpxmanager --log-level 0 --as %d focus-win %d", getMasterByName("test")->id, root);
    spawnAndWait(buffer);
    assertEquals(getActiveFocusOfMaster(getMasterByName("test")->id), root);
    assert(getActiveFocusOfMaster(getMasterByName("test2")->id) != root);
}

SCUTEST(test_send_multi_func) {
    INFO("\n\n\n\n\n\n\n\n\n");
    WindowID win = getWMPrivateWindow();
    setWindowPropertyInt(win, WM_WINDOW_ROLE, XCB_ATOM_CARDINAL, 1);
    INFO("\n\n\n\n\n\n\n\n\n");
    sendAs("raise-or-run", 0, "NotFound", "./mpxmanager restart");
    WAIT_UNTIL_TRUE(!getWindowPropertyValueInt(win, WM_WINDOW_ROLE, XCB_ATOM_CARDINAL));
    WAIT_UNTIL_TRUE(isMPXManagerRunning());
}

SCUTEST(no_expose_on_restart) {
    NON_ROOT_EVENT_MASKS |= XCB_EVENT_MASK_EXPOSURE;
    WindowID wins[] = {mapArbitraryWindow(), mapArbitraryWindow()};
    waitUntilWMIdle();
    for(int i = 0; i < LEN(wins); i++) {
        registerForWindowEvents(wins[i], XCB_EVENT_MASK_EXPOSURE);
        assert(isWindowMapped(wins[i]));
    }
    consumeEvents();
    restartWM();
    waitUntilWMIdle();
    assertEquals(0, consumeEvents());
}
/*TODO
SCUTEST(relative_position_after_restart) {
    send("auto-focus-new-window-timeout", "0");
    WindowID dock = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DOCK));
    WindowID above = mapArbitraryWindow();
    WindowID normal = mapArbitraryWindow();
    WindowID bottom = mapWindow(createNormalWindow());
    waitUntilWMIdle();
    sendChangeWindowStateRequest(above, XCB_EWMH_WM_STATE_ADD, ewmh->_NET_WM_STATE_ABOVE);
    sendChangeWindowStateRequest(bottom, XCB_EWMH_WM_STATE_ADD, ewmh->_NET_WM_STATE_BELOW);
    waitUntilWMIdle();
    WindowID relativePos[] = {bottom, normal, above};
    WindowID relativePos2[] = {bottom, normal, dock};
    assert(checkStackingOrder(relativePos, LEN(relativePos)));
    assert(checkStackingOrder(relativePos2, LEN(relativePos)));
    restartWM();
    waitUntilWMIdle();
    assert(checkStackingOrder(relativePos, LEN(relativePos)));
    assert(checkStackingOrder(relativePos2, LEN(relativePos)));
    WindowID normal2 = mapArbitraryWindow();
    waitUntilWMIdle();
    WindowID relativePos3[] = {bottom, normal2, above};
    assert(checkStackingOrder(relativePos3, LEN(relativePos)));
}
*/

static void extSetup() {
    setenv("ALLOW_MPX_EXT", "1", 1);
    initSetup();
}

static inline void triggerKeyCombo(const KeySym* keys, int num) {
    for(int i = 0; i < num; i++) {
        sendKeyPress(getKeyCode(keys[i]), getActiveMasterKeyboardID());
        flush();
    }
    flush();
    for(int i = num - 1; i >= 0; i--) {
        sendKeyRelease(getKeyCode(keys[i]), getActiveMasterKeyboardID());
        flush();
    }
}
SCUTEST_SET_ENV(extSetup, cleanup);
SCUTEST(hide_only_window, .iter = 4, .timeout = 4) {
    Window win = mapWindow(createNormalWindow());
    waitUntilWMIdle();
    assert(isWindowMapped(win));
    if(_i > 1) {
        WindowID w = createWindow(root, XCB_WINDOW_CLASS_INPUT_OUTPUT, 0, NULL, (Rect) {0, 0, 1, 1});
        xcb_icccm_wm_hints_t hints = {.input = 0};
        catchError(xcb_icccm_set_wm_hints_checked(dis, w, &hints));
        waitUntilWMIdle();
        mapWindow(w);
        if(_i == 2)
            win = mapWindow(createNormalWindow());
        waitUntilWMIdle();
    }
    focusWindow(win);
    if(!_i) {
        sendChangeWindowStateRequest(win, XCB_EWMH_WM_STATE_ADD, ewmh->_NET_WM_STATE_HIDDEN, 0);
    }
    else {
        triggerKeyCombo((KeySym[]) {XK_Super_L, XK_y}, 2);
    }
    waitUntilWMIdle();
    assert(!isWindowMapped(win));
    if(_i) {
        sendChangeWindowStateRequest(win, XCB_EWMH_WM_STATE_REMOVE, ewmh->_NET_WM_STATE_HIDDEN, 0);
    }
    else {
        triggerKeyCombo((KeySym[]) {XK_Super_L, XK_Shift_L, XK_y}, 3);
    }
    waitUntilWMIdle();
    assert(isWindowMapped(win));
}
