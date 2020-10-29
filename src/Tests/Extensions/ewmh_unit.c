#include <unistd.h>
#include <signal.h>
#include "../../Extensions/ewmh.h"
#include "../../Extensions/ewmh-client.h"
#include "../../globals.h"
#include "../../layouts.h"
#include "../../wm-rules.h"
#include "../../wmfunctions.h"
#include "../../functions.h"

#include "../tester.h"
#include "../test-event-helper.h"
#include "../test-x-helper.h"
#include "../test-wm-helper.h"


SCUTEST(test_lose_wm_selection, .iter = 2, .timeout = 10) {
    STEAL_WM_SELECTION = _i;
    int pid = fork();
    createSigAction(SIGUSR1, requestShutdown);
    if(!pid) {
        openXDisplay();
        pause();
        broadcastEWMHCompilence();
        exit(0);
    }
    else {
        openXDisplay();
        registerForWindowEvents(root, ROOT_EVENT_MASKS);
        broadcastEWMHCompilence();
        assert(isMPXManagerRunningAsWM());
        assertEquals(0, kill(pid, SIGUSR1));
        int exitCode = waitForChild(0);
        if(STEAL_WM_SELECTION) {
            assertEquals(0, exitCode);
            waitForNormalEvent(XCB_SELECTION_CLEAR);
            waitForNormalEvent(XCB_CLIENT_MESSAGE);
        }
        else {
            assertEquals(WM_ALREADY_RUNNING, exitCode);
        }
        exit(0);
    }
}

SCUTEST_SET_ENV(onSimpleStartup, cleanupXServer);

SCUTEST_ITER(test_set_workspace_names, 2) {
    if(_i) {
        addWorkspaces(10);
        strcpy(getWorkspace(1)->name, "Really_long name with some special characters !@#$%$&%");
    }
    updateWorkspaceNames();
    xcb_ewmh_get_utf8_strings_reply_t  names;
    assert(xcb_ewmh_get_desktop_names_reply(ewmh, xcb_ewmh_get_desktop_names(ewmh, defaultScreenNumber), &names, NULL));
    int index = 0;
    FOR_EACH(Workspace*, w, getAllWorkspaces()) {
        assertEquals(0, strcmp(w->name, &names.strings[index]));
        index += strlen(w->name) + 1;
    }
    assertEquals(names.strings_len, index);
    xcb_ewmh_get_utf8_strings_reply_wipe(&names);
}

SCUTEST(test_private_window_properties) {
    broadcastEWMHCompilence();
    static char buffer[MAX_NAME_LEN];
    assert(getWindowTitle(getPrivateWindow(), buffer));
    assert(buffer[0]);
}
SCUTEST(test_sync_state) {
    addWorkspaces(2);
    setShowingDesktop(0);
    assert(!isShowingDesktop());
    setShowingDesktop(1);
    assert(isShowingDesktop());
    syncShowingDesktop();
    assert(isShowingDesktop());
}

static void ewmhSetup() {
    addEWMHRules();
    onDefaultStartup();
}
SCUTEST_SET_ENV(ewmhSetup, cleanupXServer);

SCUTEST(client_list) {
    for(int i = 0; i < 10; i++)
        createNormalWindow();
    runEventLoop();
    xcb_ewmh_get_windows_reply_t reply;
    assert(xcb_ewmh_get_client_list_reply(ewmh, xcb_ewmh_get_client_list(ewmh, defaultScreenNumber), &reply, NULL));
    int size = reply.windows_len;
    assertEquals(size, getAllWindows()->size);
    verifyWindowStack(getAllWindows(), reply.windows);
    xcb_ewmh_get_windows_reply_wipe(&reply);
    // we get focus out event before destroy event
    CRASH_ON_ERRORS = 0;
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        destroyWindowInfo(winInfo);
    }
    runEventLoop();
    assert(xcb_ewmh_get_client_list_reply(ewmh, xcb_ewmh_get_client_list(ewmh, defaultScreenNumber), &reply, NULL));
    assertEquals(reply.windows_len, 0);
    xcb_ewmh_get_windows_reply_wipe(&reply);
}
SCUTEST(test_toggle_show_desktop) {
    WindowID win = mapWindow(createNormalWindow());
    WindowID desktop = mapWindow(createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DESKTOP));
    WindowID stackingOrder[] = {desktop, win, desktop};
    flush();
    scan(root);
    lowerWindowInfo(getWindowInfo(desktop), 0);
    raiseWindowInfo(getWindowInfo(win), 0);
    assert(checkStackingOrder(stackingOrder, 2));
    setShowingDesktop(1);
    flush();
    assert(isShowingDesktop());
    assert(checkStackingOrder(stackingOrder + 1, 2));
    setShowingDesktop(0);
    flush();
    assert(!isShowingDesktop());
    assert(checkStackingOrder(stackingOrder, 2));
}

SCUTEST(test_client_show_desktop) {
    xcb_ewmh_request_change_showing_desktop(ewmh, defaultScreenNumber, 1);
    flush();
    runEventLoop();
    assert(isShowingDesktop());
    xcb_ewmh_request_change_showing_desktop(ewmh, defaultScreenNumber, 0);
    runEventLoop();
    assert(!isShowingDesktop());
}

SCUTEST_ITER(auto_resume_workspace, 2) {
    addWorkspaces(2);
    WindowID win = mapArbitraryWindow();
    scan(root);
    WindowInfo* winInfo = getWindowInfo(win);
    moveToWorkspace(winInfo, getNumberOfWorkspaces() - 1);
    setSavedWorkspaceIndex(winInfo);
    removeFromWorkspace(winInfo);
    autoResumeWorkspace(getWindowInfo(win));
    if(_i)
        removeWorkspaces(getNumberOfWorkspaces() - 1);
    autoResumeWorkspace(getWindowInfo(win));
    assertEquals(getWorkspaceIndexOfWindow(winInfo), getNumberOfWorkspaces() - 1);
}


SCUTEST_ITER(test_auto_focus, 5) {
    WindowID focusHolder = mapArbitraryWindow();
    focusWindow(focusHolder);
    Window win = mapArbitraryWindow();
    registerWindow(win, root, NULL);
    assert(focusHolder == getActiveFocus());
    xcb_map_notify_event_t event = {.window = win};
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
            master = getElement(getAllMasters(), 1);
            setClientPointerForWindow(win, master->id);
            break;
    }
    addMask(getWindowInfo(win), INPUT_MASK | MAPPABLE_MASK | MAPPED_MASK);
    applyEventRules(XCB_MAP_NOTIFY, &event);
    assert(autoFocused == (win == getActiveFocusOfMaster(master->id)));
}


SCUTEST(test_sync_window_state) {
    assert(MASKS_TO_SYNC);
    assert(!getAllWindows()->size);
    WindowID win = mapWindow(createNormalWindow());
    scan(root);
    WindowInfo* winInfo = getWindowInfo(win);
    addMask(winInfo, MASKS_TO_SYNC);
    setXWindowStateFromMask(winInfo, NULL, 0);
    assertEquals(winInfo->mask & MASKS_TO_SYNC, MASKS_TO_SYNC);
    unregisterWindow(winInfo, 0);
    scan(root);
    winInfo = getWindowInfo(win);
    loadSavedAtomState(winInfo);
    assertEquals(winInfo->mask & MASKS_TO_SYNC, MASKS_TO_SYNC);
}


SCUTEST_ITER(docks, 4 * 2) {
    WindowID win = createNormalWindow();
    setWindowType(win, ewmh->_NET_WM_WINDOW_TYPE_DOCK);
    int i = _i / 2;
    int full = _i % 2;
    int size = 10;
    int start = full ? 1 : 0;
    int end = full ? 2 : 0;
    setEWMHDockProperties(win, i, size, full, start, end);
    mapWindow(win);
    scan(root);
    WindowInfo* winInfo = getWindowInfo(win);
    assert(winInfo->dock);
    const DockProperties* prop = getDockProperties(getWindowInfo(win));
    assert(prop);
    assertEquals(prop->thickness, size);
    assertEquals(prop->start, start);
    assertEquals(prop->end, end);
    setEWMHDockProperties(win, i, 1, 1, 1, 1);
    runEventLoop();
    assertEquals(prop->thickness, 1);
    assertEquals(prop->start, 1);
    assertEquals(prop->end, 1);
}
SCUTEST_ITER(auto_tile_with_dock, 5) {
    assignUnusedMonitorsToWorkspaces();
    addAutoTileRules();
    bool premap = _i & 1;
    bool consume = _i & 2;
    WindowID win = createWindowWithType(ewmh->_NET_WM_WINDOW_TYPE_DOCK);
    if(premap)
        mapWindow(win);
    setEWMHDockProperties(win, DOCK_TOP, 100, 0, 0, 0);
    WindowID win2 = mapArbitraryWindow();
    WindowID win3 = mapArbitraryWindow();
    Monitor* m = getHead(getAllMonitors());
    void func(WindowInfo * winInfo) {
        assert(memcmp(&m->base, &winInfo->geometry, sizeof(Rect)));
    }
    addEvent(WINDOW_MOVE, DEFAULT_EVENT(func, LOWEST_PRIORITY));
    if(_i < 4) {
        scan(root);
        if(consume)
            consumeEvents();
        if(!premap)
            mapWindow(win);
    }
    else
        mapWindow(win);
    runEventLoop();
    assert(memcmp(&m->base, &m->view, sizeof(Rect)));
    assert(contains(m->view, getRealGeometry(win2)));
    assert(contains(m->view, getRealGeometry(win3)));
}

SCUTEST_SET_ENV(onSimpleStartup, cleanupXServer);

SCUTEST_ITER_ERR(test_client_close_window, 2, 1) {
    AUTO_FOCUS_NEW_WINDOW_TIMEOUT = -1;
    addEWMHRules();
    addIgnoreOverrideRedirectAndInputOnlyWindowsRule();
    WindowID win = mapWindow(createNormalWindow());
    scan(root);
    sendCloseWindowRequest(_i ? createOverrideRedirectWindow() : win);
    runEventLoop();
    assert(xcb_connection_has_error(dis));
    exit(1);
}

static void clientMessageSetup() {
    CRASH_ON_ERRORS = -1;
    SRC_INDICATION = -1;
    MASKS_TO_SYNC = ~EXTERNAL_MASKS;
    addEWMHRules();
    onDefaultStartup();
    // TODO addAutoTileRules();
    WindowID win1 = mapWindow(createNormalWindow());
    WindowID win2 = mapWindow(createNormalWindow());
    scan(root);
    assertEquals(getAllWindows()->size, 2);
    assert(focusWindow(win1));
    assert(focusWindow(win2));
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        assert(getWorkspaceOfWindow(winInfo));
    }
    moveToWorkspace(getWindowInfo(win1), 0);
    moveToWorkspace(getWindowInfo(win2), 1);
    runEventLoop();
}

SCUTEST_SET_ENV(clientMessageSetup, simpleCleanup);
SCUTEST(test_mpx_manger_running_as_wm) {
    assert(isMPXManagerRunningAsWM());
}

SCUTEST_ITER(test_client_activate_window, 2) {
    FOR_EACH(WindowInfo*, winInfo, getAllWindows()) {
        Workspace* w = getWorkspaceOfWindow(winInfo);
        assert(w);
        if(_i)
            setClientPointerForWindow(winInfo->id, getActiveMasterPointerID());
        assert(isActivatable(winInfo));
        sendActivateWindowRequest(winInfo->id);
        flush();
        runEventLoop();
        assertEquals(getActiveWorkspace(), w);
        assertEquals(getFocusedWindow(), winInfo);
    }
}
SCUTEST(test_client_activate_window_bad) {
    activateWindow(getHead(getAllWindows()));
    //don't crash
    sendActivateWindowRequest(0);
    sendActivateWindowRequest(1);
    runEventLoop();
    assertEquals(getActiveFocus(), *(WindowID*)getHead(getAllWindows()));
}

SCUTEST(test_client_change_desktop) {
    CRASH_ON_ERRORS = 0;
    for(int i = 0; i < getNumberOfWorkspaces(); i++) {
        sendChangeWorkspaceRequest(i);
        flush();
        runEventLoop();
        assert(getActiveWorkspaceIndex() == i);
    }
    sendChangeWorkspaceRequest(-2);
    assert(getActiveWorkspaceIndex() < getNumberOfWorkspaces());
}
SCUTEST_ITER(test_client_change_num_desktop, 2) {
    CRASH_ON_ERRORS = 0;
    assert(getNumberOfWorkspaces() >= 2);
    switchToWorkspace(_i);
    runEventLoop();
    for(int n = 0; n < 2; n++) {
        for(int i = 1; i < 3; i++) {
            sendChangeNumWorkspaceRequestAbs(i);
            flush();
            runEventLoop();
            assertEquals(getNumberOfWorkspaces(), i);
            assert(getActiveWorkspaceIndex() <= getNumberOfWorkspaces());
        }
    }
}

SCUTEST(test_client_set_sticky_window) {
    mapArbitraryWindow();
    WindowInfo* winInfo = getHead(getAllWindows());
    sendChangeWindowWorkspaceRequest(winInfo->id, -1);
    flush();
    runEventLoop();
    assert(hasMask(winInfo, STICKY_MASK));
}

SCUTEST(test_client_set_window_workspace) {
    int index = 3;
    WindowInfo* winInfo = getHead(getAllWindows());
    sendChangeWindowWorkspaceRequest(winInfo->id, index);
    runEventLoop();
    flush();
    assertEquals(getWorkspaceOfWindow(winInfo)->id, index);
    // just don't crash
    sendChangeWindowWorkspaceRequest(winInfo->id, -2);
    runEventLoop();
}
SCUTEST(test_client_set_window_unknown_state) {
    WindowInfo* winInfo = getHead(getAllWindows());
    catchError(xcb_ewmh_set_wm_state_checked(ewmh, winInfo->id, 1, &ewmh->MANAGER));
    setXWindowStateFromMask(winInfo, NULL, 0);
    xcb_ewmh_get_atoms_reply_t reply;
    assert(xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, winInfo->id), &reply, NULL));
    assertEquals(reply.atoms_len, 1);
    assertEquals(reply.atoms[0], ewmh->MANAGER);
    xcb_ewmh_get_atoms_reply_wipe(&reply);
}
SCUTEST_ITER(test_client_set_window_state, 3) {
    suppressOutput();
    setLogLevel(LOG_LEVEL_VERBOSE);
    source = (xcb_ewmh_client_source_type_t)_i;
    WindowInfo* winInfo = getHead(getAllWindows());
    sendChangeWindowStateRequest(winInfo->id, XCB_EWMH_WM_STATE_ADD, ewmh->_NET_WM_STATE_STICKY, 0);
}
SCUTEST_ITER(test_client_set_window_state2, 2) {
    WindowInfo* winInfo = getHead(getAllWindows());
    bool allowRequest = _i;
    if(!allowRequest)
        SRC_INDICATION = 0;
    xcb_ewmh_wm_state_action_t states[] = {XCB_EWMH_WM_STATE_ADD, XCB_EWMH_WM_STATE_REMOVE, XCB_EWMH_WM_STATE_TOGGLE, XCB_EWMH_WM_STATE_TOGGLE};
    for(int i = 0; i < LEN(states); i++) {
        sendChangeWindowStateRequest(createOverrideRedirectWindow(), states[i], ewmh->_NET_WM_STATE_MAXIMIZED_HORZ,
            ewmh->_NET_WM_STATE_MAXIMIZED_VERT);
        sendChangeWindowStateRequest(winInfo->id, states[i], ewmh->_NET_WM_STATE_MAXIMIZED_HORZ,
            ewmh->_NET_WM_STATE_MAXIMIZED_VERT);
        runEventLoop();
        if(i % 2 == 0 && allowRequest)
            assert(hasMask(winInfo, X_MAXIMIZED_MASK | Y_MAXIMIZED_MASK));
        else
            assert(!hasPartOfMask(winInfo, X_MAXIMIZED_MASK | Y_MAXIMIZED_MASK));
    }
}
SCUTEST_ITER(change_wm_state, 2) {
    Window win = mapArbitraryWindow();
    if(!_i)
        MASKS_TO_SYNC = 0;
    uint32_t state = XCB_ICCCM_WM_STATE_ICONIC;
    catchError(xcb_ewmh_send_client_message(dis, win, root, WM_CHANGE_STATE, sizeof(state), &state));
    runEventLoop();
    assertEquals(hasMask(getWindowInfo(win), HIDDEN_MASK), _i);
}
SCUTEST_ITER(test_handle_unsync_requests, 4) {
    MASKS_TO_SYNC = HIDDEN_MASK;
    ALLOW_SETTING_UNSYNCED_MASKS = _i / 2;
    bool masksAlreadySet = _i % 2;
    int newExpectedAtomsDistribution[][2] = {
        {0, 1},
        {1, 1}
    };
    int baseAtoms = 1;
    int expectedAtoms = baseAtoms + newExpectedAtomsDistribution[ALLOW_SETTING_UNSYNCED_MASKS][masksAlreadySet];
    WindowInfo* winInfo = getHead(getAllWindows());
    if(masksAlreadySet) {
        MASKS_TO_SYNC |= FULLSCREEN_MASK;
        addMask(winInfo, FULLSCREEN_MASK);
    }
    sendChangeWindowStateRequest(winInfo->id, XCB_EWMH_WM_STATE_ADD, ewmh->_NET_WM_STATE_HIDDEN,
        ewmh->_NET_WM_STATE_FULLSCREEN);
    runEventLoop();
    assert(hasMask(winInfo, HIDDEN_MASK));
    assertEquals(masksAlreadySet, hasMask(winInfo, FULLSCREEN_MASK));
    xcb_ewmh_get_atoms_reply_t reply;
    assert(xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, winInfo->id), &reply, NULL));
    dumpAtoms(reply.atoms, reply.atoms_len);
    assertEquals(reply.atoms_len, expectedAtoms);
    xcb_ewmh_get_atoms_reply_wipe(&reply);
    if(expectedAtoms) {
        sendChangeWindowStateRequest(winInfo->id, XCB_EWMH_WM_STATE_REMOVE, ewmh->_NET_WM_STATE_FULLSCREEN, 0);
        assert(xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, winInfo->id), &reply, NULL));
        assertEquals(reply.atoms_len, expectedAtoms);
        xcb_ewmh_get_atoms_reply_wipe(&reply);
    }
}

SCUTEST(test_client_request_frame_extents) {
    WindowInfo* winInfo = getHead(getAllWindows());
    xcb_ewmh_request_frame_extents(ewmh, defaultScreenNumber, winInfo->id);
    flush();
}

SCUTEST(test_client_request_restack) {
    //setActiveLayout(NULL);
    WindowInfo* winInfo = getHead(getAllWindows());
    WindowInfo* winInfo2 = getElement(getAllWindows(), 1);
    WindowID stackingOrder[] = {winInfo->id, winInfo2->id, winInfo->id};
    moveToWorkspace(winInfo2, 0);
    raiseWindow(winInfo2->id, 0);
    runEventLoop();
    assert(checkStackingOrder(stackingOrder, 2));
    addMask(winInfo, EXTERNAL_RAISE_MASK);
    //processConfigureRequest(winInfo->id, NULL, winInfo2->getID(), XCB_STACK_MODE_ABOVE,  XCB_CONFIG_WINDOW_STACK_MODE|XCB_CONFIG_WINDOW_SIBLING);
    xcb_ewmh_request_restack_window(ewmh, defaultScreenNumber, winInfo->id, winInfo2->id, XCB_STACK_MODE_ABOVE);
    flush();
    runEventLoop();
    assert(checkStackingOrder(stackingOrder + 1, 2));
}
SCUTEST(test_client_request_restack_bad) {
    xcb_ewmh_request_restack_window(ewmh, defaultScreenNumber, 0, 0, XCB_STACK_MODE_ABOVE);
}

static void wmMoveSetup() {
    clientMessageSetup();
    movePointer(0, 0);
    floatWindow(getHead(getAllWindows()));
}
SCUTEST_SET_ENV(wmMoveSetup, simpleCleanup);
SCUTEST_ITER(wm_move_resize_window, 4) {
    int deltaX = 10;
    int deltaY = 20;
    WindowInfo* winInfo = getHead(getAllWindows());
    Rect rect = getRealGeometry(winInfo->id);
    if(_i) {
        rect.width += deltaX * (_i & 1);
        rect.height += deltaY * (_i & 2 ? 1 : 0);
        xcb_ewmh_moveresize_direction_t action = _i == 1 ? XCB_EWMH_WM_MOVERESIZE_SIZE_RIGHT : _i == 2 ?
            XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOM : XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
        sendWMMoveResizeRequest(winInfo->id, 0, 0, action, 1);
    }
    else {
        rect.x += deltaX;
        rect.y += deltaY;
        sendWMMoveResizeRequest(winInfo->id, 0, 0, XCB_EWMH_WM_MOVERESIZE_MOVE, 1);
    }
    runEventLoop();
    assert(getActiveMaster()->windowMoveResizer);
    movePointer(deltaX, deltaY);
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(winInfo->id));
    // End
    clickButton(1, getActiveMasterPointerID());
    runEventLoop();
    movePointer(deltaX * 2, deltaY * 2);
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(winInfo->id));
}
SCUTEST(wm_move_resize_window_bad) {
    sendWMMoveResizeRequest(0, 0, 0, XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, 1);
}
SCUTEST(wm_move_resize_window_invert) {
    movePointer(20, 40);
    WindowInfo* winInfo = getHead(getAllWindows());
    setWindowPosition(winInfo->id, (Rect) {10, 20, 10, 20});
    Rect rect = {0, 0, 10, 20};
    xcb_ewmh_moveresize_direction_t action = XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
    sendWMMoveResizeRequest(winInfo->id, 0, 0, action, 1);
    runEventLoop();
    movePointer(0, 0);
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(winInfo->id));
}
SCUTEST_ITER(wm_move_resize_window_cancel_commit, 2) {
    WindowInfo* winInfo = getHead(getAllWindows());
    Rect rect = getRealGeometry(winInfo->id);
    sendWMMoveResizeRequest(winInfo->id, 0, 0, XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, 1);
    runEventLoop();
    movePointer(10, 10);
    flush();
    runEventLoop();
    if(_i)
        commitWindowMoveResize();
    Rect temp = getRealGeometry(winInfo->id);
    assert(memcmp(&rect, &temp, sizeof(Rect)));
    sendWMMoveResizeRequest(winInfo->id, 0, 0, XCB_EWMH_WM_MOVERESIZE_CANCEL, 1);
    runEventLoop();
    if(_i) {
        temp = getRealGeometry(winInfo->id);
        assert(memcmp(&rect, &temp, sizeof(Rect)));
    }
    else
        assertEqualsRect(rect, getRealGeometry(winInfo->id));
}
SCUTEST(wm_move_resize_window_no_change) {
    WindowInfo* winInfo = getHead(getAllWindows());
    consumeEvents();
    startWindowMove(winInfo);
    assertEquals(consumeEvents(), 0);
}
SCUTEST(wm_move_resize_window_zero) {
    WindowInfo* winInfo = getHead(getAllWindows());
    setWindowPosition(winInfo->id, (Rect) {0, 0, 150, 150});
    Rect rect = getRealGeometry(winInfo->id);
    movePointer(rect.width, rect.height);
    startWindowResize(winInfo);
    movePointer(0, 0);
    updateWindowMoveResize(winInfo);
    Rect target = {rect.x, rect.y, 1, 1};
    assertEqualsRect(target, getRealGeometry(winInfo->id));
}
SCUTEST(test_client_request_move_resize) {
    Rect rect = {1, 2, 3, 4};
    WindowInfo* winInfo = getHead(getAllWindows());
    addMask(winInfo, EXTERNAL_MOVE_MASK | EXTERNAL_RESIZE_MASK);
    sendMoveResizeRequest(winInfo->id, &rect.x,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT);
    flush();
    runEventLoop();
    assertEqualsRect(rect, getRealGeometry(winInfo->id));
}

SCUTEST(event_speed) {
    grabActivePointer();
    startWindowMove(getHead(getAllWindows()));
    generateMotionEvents(100);
    flush();
    runEventLoop();
    assert(!consumeEvents());
}
