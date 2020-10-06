#include "../devices.h"
#include "../functions.h"
#include "../globals.h"
#include "../layouts.h"
#include "../windows.h"
#include "../wm-rules.h"
#include "../settings.h"
#include "../wmfunctions.h"
#include "test-event-helper.h"
#include "test-wm-helper.h"
#include "tester.h"


SCUTEST_SET_ENV(onDefaultStartup, cleanupXServer);

SCUTEST(test_focus_window_visible_after_window_destroy) {
    WindowID win = mapWindow(createNormalWindow());
    WindowID winNextFocused = mapWindow(createNormalWindow());
    WindowID win2 = mapWindow(createNormalWindow());
    WindowID winFocused = mapWindow(createNormalWindow());
    runEventLoop();
    focusWindow(win);
    focusWindow(win2);
    focusWindow(winNextFocused);
    focusWindow(winFocused);
    runEventLoop();
    assertEquals(getActiveMasterWindowStack()->size, 4);
    assertEquals(getFocusedWindow()->id, winFocused);
    assert(checkStackingOrder((WindowID[]) {win, winNextFocused, win2, winFocused}, 4));
    destroyWindow(winFocused);
    runEventLoop();
    assert(checkStackingOrder((WindowID[]) {win, win2, winNextFocused}, 3));
}
SCUTEST(test_tile_before_map) {
    WindowID win = mapWindow(createNormalWindow());
    registerWindow(win, root, NULL);
    assert(getWorkspaceOfWindow(getWindowInfo(win)));
    switchToWorkspace(1);
    runEventLoop();
    setWindowPosition(win, (Rect) {0, 1, 2, 3});
    switchToWorkspace(0);
    markWorkspaceOfWindowDirty(getWindowInfo(win));
    consumeEvents();
    addEvent(XCB_MAP_NOTIFY, DEFAULT_EVENT(fail));
    addEvent(XCB_CONFIGURE_NOTIFY, DEFAULT_EVENT(incrementCount));
    addEvent(XCB_CONFIGURE_NOTIFY, DEFAULT_EVENT(requestShutdown));
    runEventLoop();
    assert(getCount());
}
SCUTEST(auto_window_map) {
    WindowID win = mapWindow(createNormalWindow());
    scan(root);
    WindowInfo* winInfo = getWindowInfo(win);
    assert(winInfo);
    assert(getWorkspaceOfWindow(winInfo));
    assert(isWindowMapped(winInfo->id));
    assert(hasMask(winInfo, MAPPED_MASK));
}
SCUTEST_ITER(maintain_focus_when_moving_window_to_another_workspace, 2) {
    MONITOR_DUPLICATION_POLICY = 0;
    addFakeMonitor((Rect) {100, 0, 100, 100});
    assignUnusedMonitorsToWorkspaces();
    mapWindow(createNormalWindow());
    WindowID win = mapWindow(createNormalWindow());
    scan(root);
    focusWindow(win);
    runEventLoop();
    assertEquals(getActiveFocus(), win);
    moveToWorkspace(getWindowInfo(win), _i);
    runEventLoop();
    assertEquals(getActiveFocus(), win);
}
SCUTEST(test_remember_last_focused_window_in_workspace2, .iter = 2) {
    WindowID win[] = {createNormalWindow(), createNormalWindow(), createNormalWindow()};
    switchToWorkspace(_i);
    runEventLoop();
    for(int i = LEN(win) - 1; i >= 0; --i)
        focusWindow(mapWindow(win[i]));
    focusWindow(mapWindow(win[1]));
    runEventLoop();
    activateWorkspace(!_i);
    runEventLoop();
    assertEquals(getActiveFocus(), root);
    activateWorkspace(_i);
    runEventLoop();
    assertEquals(getActiveFocus(), getFocusedWindow()->id);
    assertEquals(getActiveFocus(), win[1]);
}

SCUTEST(click_to_focus) {
    setActiveLayout(&GRID);
    WindowID wins[] = {mapArbitraryWindow(), mapArbitraryWindow()};
    runEventLoop();
    for(int i = 0; i < LEN(wins); i++) {
        warpPointer(1, 1, wins[i], getActiveMasterPointerID());
        clickButton(1, getActiveMasterPointerID());
        runEventLoop();
        assert(getFocusedWindow());
        assertEquals(getFocusedWindow()->id, wins[i]);
    }
}

SCUTEST(transfer_focus_within_workspace) {
    CRASH_ON_ERRORS = 0;
    addFakeMonitor((Rect) {100, 0, 100, 100});
    assignUnusedMonitorsToWorkspaces();
    WindowID win = mapArbitraryWindow();
    WindowID win2 = mapArbitraryWindow();
    WindowID win3 = mapArbitraryWindow();
    runEventLoop();
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        isWindowMapped(winInfo->id);
        focusWindow(winInfo->id);
    }
    moveToWorkspace(getWindowInfo(win2), 1);
    runEventLoop();
    assertEquals(getFocusedWindow(), getWindowInfo(win3));
    assertEquals(3, getActiveMasterWindowStack()->size);
    destroyWindow(win3);
    runEventLoop();
    assertEquals(getFocusedWindow(), getWindowInfo(win));
    destroyWindow(win);
    runEventLoop();
    assertEquals(getFocusedWindow(), getWindowInfo(win2));
}
SCUTEST_ITER(stable, 2) {
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        raiseLowerWindow(winInfo->id, 0, _i);
    }
    runEventLoop();
    assert(!consumeEvents());
}


SCUTEST(test_dock_not_auto_in_workspace) {
    void func(WindowInfo * winInfo) {
        winInfo->dock = 1;
    }
    addEvent(CLIENT_MAP_ALLOW, DEFAULT_EVENT(func, HIGHEST_PRIORITY));
    WindowID win = mapWindow(createNormalWindow());
    runEventLoop();
    assert(!getWorkspaceOfWindow(getWindowInfo(win)));
}


void replayKeyboardEvent();
static Binding customBindings[] = {
    {0, XK_Super_L, {grabActiveKeyboard, {0}} },
    {0, XK_Super_L, {setFocusStackFrozen, {1}} },
    {Mod4Mask, XK_Super_L, {setFocusStackFrozen, {0}}, .flags = {.mask = XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE}},
    {Mod4Mask, XK_Super_L, {ungrabActiveKeyboard, {0}}, .flags = {.mask = XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE}},
    {Mod4Mask, XK_A, {incrementCount}},
};
static void customBindingsSetup() {
    ROOT_DEVICE_EVENT_MASKS = 0;
    addBindings(customBindings, LEN(customBindings));
    addShutdownOnIdleRule();
    loadSettings();
    onStartup();
    runEventLoop();
}
SCUTEST_SET_ENV(customBindingsSetup, cleanupXServer);
SCUTEST(test_custom_bindings) {
    sendKeyPress(getKeyCode(XK_Super_L), getActiveMasterKeyboardID());
    sendKeyPress(getKeyCode(XK_A), getActiveMasterKeyboardID());
    runEventLoop();
    assert(isFocusStackFrozen());
    assertEquals(1, getCount());
    sendKeyRelease(getKeyCode(XK_A), getActiveMasterKeyboardID());
    sendKeyPress(getKeyCode(XK_A), getActiveMasterKeyboardID());
    runEventLoop();
    assert(isFocusStackFrozen());
    assertEquals(2, getCount());
    sendKeyRelease(getKeyCode(XK_A), getActiveMasterKeyboardID());
    sendKeyRelease(getKeyCode(XK_Super_L), getActiveMasterKeyboardID());
    runEventLoop();
    assert(!isFocusStackFrozen());
    assertEquals(2, getCount());
    typeKey(getKeyCode(XK_A), getActiveMasterKeyboardID());
    runEventLoop();
}



static void bindingsSetup() {
    ROOT_DEVICE_EVENT_MASKS = 0;
    addShutdownOnIdleRule();
    loadSettings();
    onStartup();
}
SCUTEST_SET_ENV(bindingsSetup, cleanupXServer);
SCUTEST_ITER(cycle_window, 3) {
    mapArbitraryWindow();
    sendKeyPress(getKeyCode(XK_Alt_L), getActiveMasterKeyboardID());
    if(_i == 2) {
        sendKeyPress(getKeyCode(XK_Shift_L), getActiveMasterKeyboardID());
    }
    for(int i = 0; i <= _i; i++) {
        typeKey(getKeyCode(XK_Tab), getActiveMasterKeyboardID());
        runEventLoop();
        assertEquals(getActiveMaster()->bindings.size, 1);
    }
    assert(isFocusStackFrozen());
    sendKeyRelease(getKeyCode(XK_Tab), getActiveMasterKeyboardID());
    sendKeyRelease(getKeyCode(XK_Alt_L), getActiveMasterKeyboardID());
    runEventLoop();
    assert(!isFocusStackFrozen());
    assertEquals(getActiveMaster()->bindings.size, 0);
}
SCUTEST_ITER(move_window, 2) {
    sendKeyPress(getKeyCode(XK_Super_L), getActiveMasterKeyboardID());
    bool move = !_i % 2;
    WindowID win = mapArbitraryWindow();
    setWindowPosition(win, (Rect) {0, 0, 10, 10});
    runEventLoop();
    Rect rect = getRealGeometry(win);
    if(move) {
        rect.x++;
        rect.y++;
    }
    else {
        rect.width++;
        rect.height++;
    }
    warpPointer(1, 1, win, getActiveMasterPointerID());
    sendButtonPress(move ? Button1 : Button3, getActiveMasterPointerID());
    runEventLoop();
    movePointerRelative(1, 1);
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(win));
    sendButtonRelease(move ? Button1 : Button3, getActiveMasterPointerID());
    runEventLoop();
    movePointerRelative(1, 1);
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(win));
}
SCUTEST(test_destroy_master_during_chain) {
    createMasterDevice("test");
    initCurrentMasters();
    setActiveMaster(getMasterByName("test"));
    sendKeyPress(getKeyCode(XK_Super_L), getActiveMasterKeyboardID());
    bool move = !_i % 2;
    WindowID win = mapArbitraryWindow();
    setWindowPosition(win, (Rect) {0, 0, 10, 10});
    runEventLoop();
    warpPointer(1, 1, win, getActiveMasterPointerID());
    sendButtonPress(move ? Button1 : Button3, getActiveMasterPointerID());
    runEventLoop();
    destroyMasterDevice(getActiveMasterKeyboardID());
    runEventLoop();
}
