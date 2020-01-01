#include "../bindings.h"
#include "../devices.h"
#include "../ewmh.h"
#include "../extra-rules.h"
#include "../extra-rules.h"
#include "../globals.h"
#include "../layouts.h"
#include "../logger.h"
#include "../state.h"
#include "../window-properties.h"
#include "../wm-rules.h"
#include "../wmfunctions.h"
#include "test-event-helper.h"
#include "tester.h"

SET_ENV(onSimpleStartup, fullCleanup);
MPX_TEST("test_print_method", {
    addPrintStatusRule();
    printStatusMethod = incrementCount;
    // set to an open FD
    STATUS_FD = 1;
    applyEventRules(IDLE, NULL);
    assert(getCount());
});

MPX_TEST("test_die_on_idle", {
    addDieOnIdleRule();
    createNormalWindow();
    flush();
    atexit(fullCleanup);
    runEventLoop();
});
MPX_TEST("shutdown_on_idle", {
    addShutdownOnIdleRule();
    createNormalWindow();
    runEventLoop();
    assert(isShuttingDown());
});
MPX_TEST("test_desktop_rule", {
    addDesktopRule();
    Monitor* m = getActiveWorkspace()->getMonitor();
    WindowID win = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DESKTOP));
    setActiveLayout(GRID);
    scan(root);
    WindowInfo* winInfo = getWindowInfo(win);
    winInfo->moveToWorkspace(0);
    assert(winInfo->hasMask(STICKY_MASK | NO_TILE_MASK | BELOW_MASK));
    assert(!winInfo->hasMask(ABOVE_MASK));
    m->setViewport({10, 20, 100, 100});
    retile();
    flush();
    assertEquals(m->getViewport(), getRealGeometry(winInfo->getID()));
});
MPX_TEST("test_float_rule", {
    addFloatRule();
    mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DIALOG));
    mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DESKTOP));
    mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_SPLASH));
    scan(root);
    for(WindowInfo* winInfo : getAllWindows())
        assert(winInfo->hasMask(FLOATING_MASK));
});
MPX_TEST_ITER("test_ignored_windows", 2, {
    addUnknownInputOnlyWindowIgnoreRule();
    if(_i)
        createTypelessInputOnlyWindow();
    else
        mapWindow(createTypelessInputOnlyWindow());
    startWM();
    waitUntilIdle();
    assertEquals(getAllWindows().size(), _i);
});

MPX_TEST_ITER("detect_dock", 2, {
    addAvoidDocksRule();
    if(_i)
        addNoDockFocusRule();
    WindowID win = mapWindow(createNormalWindow());
    setWindowType(win, &ewmh->_NET_WM_WINDOW_TYPE_DOCK, 1);
    assert(catchError(xcb_ewmh_set_wm_strut_checked(ewmh, win, 0, 0, 1, 0)) == 0);
    scan(root);
    assert(getWindowInfo(win)->isDock());
    assert(getWindowInfo(win)->hasMask(INPUT_MASK) == !_i);
    auto prop = getWindowInfo(win)->getDockProperties();
    assert(prop);
    for(int i = 0; i < 4; i++)
        if(i == 2)
            assert(prop[i] == 1);
        else
            assert(prop[i] == 0);
});

MPX_TEST("test_always_on_top_bottom", {
    addAlwaysOnTopBottomRules();
    //windows are in same spot
    WindowID bottom = createNormalWindow();
    WindowID bottom2 = createNormalWindow();
    WindowID normal = createNormalWindow();
    WindowID top = createNormalWindow();
    WindowID top2 = createNormalWindow();
    scan(root);
    getWindowInfo(bottom)->addMask(ALWAYS_ON_BOTTOM_MASK);
    getWindowInfo(bottom2)->addMask(ALWAYS_ON_BOTTOM_MASK);
    getWindowInfo(top)->addMask(ALWAYS_ON_TOP_MASK);
    getWindowInfo(top2)->addMask(ALWAYS_ON_TOP_MASK);
    raiseWindow(normal);
    raiseWindow(bottom);
    lowerWindow(top);
    setActiveLayout(NULL);
    flush();
    startWM();
    waitUntilIdle();
    WindowID stackingOrder[][3] = {
        {bottom, normal, top},
        {bottom2, normal, top2},
        {bottom, normal, top2},
        {bottom2, normal, top},
    };
    for(int i = 0; i < LEN(stackingOrder); i++)
        assert(checkStackingOrder(stackingOrder[i], 3));
    msleep(POLL_COUNT* POLL_INTERVAL * 2);
    assert(consumeEvents() == 0);
});

MPX_TEST_ITER("primary_monitor_windows", 3, {
    addAutoTileRules();
    getEventRules(POST_REGISTER_WINDOW).add(DEFAULT_EVENT(+[](WindowInfo * winInfo) {winInfo->addMask(PRIMARY_MONITOR_MASK);}), PREPEND_ALWAYS);
    addStickyPrimaryMonitorRule();
    addFakeMonitor({100, 100, 200, 200});
    addFakeMonitor({300, 100, 100, 100});
    Monitor* realMonitor = getAllMonitors()[0];
    Monitor* m = getAllMonitors()[1];
    m->setPrimary(1);
    detectMonitors();
    assert(m->getWorkspace());
    WindowID win = mapWindow(createNormalWindow());
    startWM();
    waitUntilIdle();
    WindowInfo* winInfo = getWindowInfo(win);
    if(_i == 1) {
        winInfo->moveToWorkspace(realMonitor->getWorkspace()->getID());
        winInfo->addMask(NO_TILE_MASK);
    }
    else if(_i == 2) {
        winInfo->setTilingOverrideEnabled(-1);
        winInfo->setTilingOverride({0, 0, 100, 100});
    }

    mapWindow(createNormalWindow());
    waitUntilIdle();
    assert(m->getBase().contains(getRealGeometry(win)));
});

MPX_TEST("test_focus_follows_mouse", {
    addEWMHRules();
    addFocusFollowsMouseRule();
    setActiveLayout(GRID);
    mapArbitraryWindow();
    mapArbitraryWindow();
    scan(root);
    retile();
    WindowInfo* winInfo = getAllWindows()[0];
    WindowInfo* winInfo2 = getAllWindows()[1];
    WindowID id1 = winInfo->getID();
    WindowID id2 = winInfo2->getID();
    focusWindow(id1);
    movePointer(0, 0, id1);
    flush();
    startWM();
    WAIT_UNTIL_TRUE(getActiveFocus(getActiveMaster()->getID()) == id1);
    for(int i = 0; i < 4; i++) {
        int id = (i % 2 ? id1 : id2);
        int n = 0;
        WAIT_UNTIL_TRUE(getActiveFocus(getActiveMaster()->getID()) == id,
            movePointer(n, n, id);
            n = !n;
            flush()
        );
    }
});

MPX_TEST_ITER("test_auto_focus", 5, {
    addAutoFocusRule();
    WindowID focusHolder = mapArbitraryWindow();

    focusWindow(focusHolder);
    Window win = mapArbitraryWindow();
    registerWindow(win, root);
    assert(focusHolder == getActiveFocus(getActiveMasterKeyboardID()));
    xcb_map_notify_event_t event = {0};
    event.window = win;
    setLastEvent(&event);

    auto autoFocus = []() {
        getAllWindows().back()->addMask(INPUT_MASK | MAPPABLE_MASK | MAPPED_MASK);
        applyEventRules(XCB_MAP_NOTIFY, NULL);
    };
    bool autoFocused = 1;
    Master* master = getActiveMaster();
    switch(_i) {
        case 0:
            AUTO_FOCUS_NEW_WINDOW_TIMEOUT = 1000;
            break;
        case 1:
            AUTO_FOCUS_NEW_WINDOW_TIMEOUT = -1;
            break;
        case 2:
            AUTO_FOCUS_NEW_WINDOW_TIMEOUT = 0;
            autoFocused = 0;
            break;
        case 3:
            AUTO_FOCUS_NEW_WINDOW_TIMEOUT = -1;
            autoFocused = 0;
            event.window = 0;
            break;
        case 4:
            AUTO_FOCUS_NEW_WINDOW_TIMEOUT = 1000;
            createMasterDevice("test");
            initCurrentMasters();
            master = getAllMasters()[1];
            setClientPointerForWindow(win, master->getID());
            break;
    }
    autoFocus();
    assert(autoFocused == (win == getActiveFocus(master->getID())));
});
MPX_TEST_ITER("ignore_small_window", 3, {
    addIgnoreSmallWindowRule();
    WindowID win = mapArbitraryWindow();
    xcb_size_hints_t hints = {0};
    if(_i)
        xcb_icccm_size_hints_set_size(&hints, _i - 1, 1, 1);
    else
        xcb_icccm_size_hints_set_base_size(&hints, 1, 1);
    assert(!catchError(xcb_icccm_set_wm_size_hints_checked(dis, win, XCB_ATOM_WM_NORMAL_HINTS, &hints)));
    flush();
    scan(root);
    if(_i != 1)
        assert(getWindowInfo(win));
    else
        assert(getWindowInfo(win) == NULL);
});
MPX_TEST("test_detect_sub_windows", {
    NON_ROOT_EVENT_MASKS |= XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    addScanChildrenRule();
    WindowID win = createUnmappedWindow();
    WindowID win2 = createNormalSubWindow(win);
    flush();
    startWM();
    waitUntilIdle();
    WindowID win3 = createNormalSubWindow(win2);
    waitUntilIdle();
    assert(getAllWindows().find(win));
    assert(getAllWindows().find(win2));
    assert(getAllWindows().find(win3));
});
MPX_TEST("test_key_repeat", {
    getEventRules(XCB_INPUT_KEY_PRESS).add(DEFAULT_EVENT(+[]() {exit(10);}));
    addIgnoreKeyRepeat();
    xcb_input_key_press_event_t event = {.flags = XCB_INPUT_KEY_EVENT_FLAGS_KEY_REPEAT};
    setLastEvent(&event);
    assert(!applyEventRules(XCB_INPUT_KEY_PRESS));
});

MPX_TEST("test_transient_windows_always_above", {
    addKeepTransientsOnTopRule();
    WindowID win = mapArbitraryWindow();
    WindowID win2 = mapArbitraryWindow();
    WindowID win3 = mapArbitraryWindow();
    setWindowTransientFor(win2, win);
    registerWindow(win, root);
    registerWindow(win2, root);
    WindowInfo* winInfo = getWindowInfo(win);
    WindowInfo* winInfo2 = getWindowInfo(win2);
    winInfo->moveToWorkspace(getActiveWorkspaceIndex());
    winInfo2->moveToWorkspace(getActiveWorkspaceIndex());
    loadWindowProperties(winInfo2);
    assert(winInfo2->getTransientFor() == winInfo->getID());
    winInfo2->moveToWorkspace(getActiveWorkspaceIndex());
    assert(winInfo->isActivatable()&& winInfo2->isActivatable());
    assert(winInfo2->isInteractable());
    WindowID stack[] = {win, win2, win3};
    startWM();
    waitUntilIdle();
    for(int i = 0; i < 4; i++) {
        assert(checkStackingOrder(stack, 2));
        if(i % 2)
            activateWindow(winInfo);
        else
            activateWindow(winInfo2);
        waitUntilIdle(1);
    }
});
MPX_TEST_ITER("border_for_floating", 2, {
    DEFAULT_BORDER_WIDTH = 1;
    addAutoTileRules();
    addFloatRule();
    addEWMHRules();
    WindowID win = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DIALOG));
    startWM();
    waitUntilIdle();
    if(_i)
        assertEquals(getRealGeometry(win).border, DEFAULT_BORDER_WIDTH);
    else {
        floatWindow(getWindowInfo(win));
        mapArbitraryWindow();
        startWM();
        waitUntilIdle();
        retile();
        assertEquals(getRealGeometry(win).border, DEFAULT_BORDER_WIDTH);
    }
});
MPX_TEST("ignore_non_top_level_windows", {
    addIgnoreNonTopLevelWindowsRule();
    WindowID win = createNormalWindow();
    WindowID parent = createNormalWindow();
    startWM();
    waitUntilIdle();
    assert(getWindowInfo(win));
    assert(getWindowInfo(parent));
    xcb_reparent_window_checked(dis, win, parent, 0, 0);
    waitUntilIdle();
    assert(!getWindowInfo(win));
    xcb_reparent_window_checked(dis, win, root, 0, 0);
    waitUntilIdle();
    assert(getWindowInfo(win));
});
MPX_TEST("moveNonTileableWindowsToWorkspaceBounds", {
    addEWMHRules();
    addFloatRule();
    addMoveNonTileableWindowsToWorkspaceBounds();
    getActiveMaster()->setWorkspaceIndex(0);
    Monitor* m = getAllMonitors()[0];
    Rect dims = m->getBase();
    dims.x += m->getBase().width;
    Monitor* m2 = new Monitor(1, dims);
    getAllMonitors().add(m2);
    m2->assignWorkspace(getWorkspace(1));
    startWM();
    WindowID win = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DIALOG));
    waitUntilIdle();
    assert(getRealGeometry(win).isAtOrigin());
    getActiveMaster()->setWorkspaceIndex(1);
    WindowID win2 = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DIALOG));
    waitUntilIdle();
    assert(getRealGeometry(win) != getRealGeometry(win2));
    assert(dims.intersects(getRealGeometry(win2)) || dims.contains(getRealGeometry(win2)));
});
MPX_TEST_ITER("unmanaged_windows_above", 2, {
    MASKS_TO_SYNC |= ABOVE_MASK | BELOW_MASK;
    bool above = _i;
    addEWMHRules();
    addConvertNonManageableWindowMask();
    addIgnoreOverrideRedirectWindowsRule(ADD_REMOVE);
    startWM();
    waitUntilIdle();
    WindowID win = createOverrideRedirectWindow();
    sendChangeWindowStateRequest(win, XCB_EWMH_WM_STATE_ADD, above ? ewmh->_NET_WM_STATE_ABOVE : ewmh->_NET_WM_STATE_BELOW);
    waitUntilIdle();
    mapWindow(win);
    waitUntilIdle();
    assert(getWindowInfo(win)->hasMask(above ? ALWAYS_ON_TOP_MASK : ALWAYS_ON_BOTTOM_MASK));
});
