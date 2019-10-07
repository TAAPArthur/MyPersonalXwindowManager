#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib-xcb.h>

#include "tester.h"
#include "test-x-helper.h"
#include "test-event-helper.h"
#include "../state.h"
#include "../default-rules.h"
#include "../layouts.h"
#include "../wmfunctions.h"
#include "../window-properties.h"

static WindowInfo* addVisibleWindow(int i) {
    WindowInfo* winInfo = new WindowInfo(createNormalWindow());
    addWindowInfo(winInfo);
    getWorkspace(i)->getWindowStack().add(winInfo);
    winInfo->addMask(MAPPABLE_MASK | MAPPED_MASK);
    return winInfo;
}
SET_ENV(createXSimpleEnv, fullCleanup);
MPX_TEST("init", {
    markState();
    assert(updateState() == WORKSPACE_MONITOR_CHANGE);
});
MPX_TEST("test_no_state_change", {
    markState();
    assert(updateState() == WORKSPACE_MONITOR_CHANGE);
    assert(!updateState());
    markState();
    assert(!updateState());
});

static void setup() {
    DEFAULT_NUMBER_OF_WORKSPACES = 4;
    onStartup();
    assert(getAllMonitors().size() == 1);
    assert(getWorkspace(0)->isVisible());
    markState();
    updateState();
    getEventRules(TileWorkspace).add(new BoundFunction(incrementCount));
}
SET_ENV(setup, fullCleanup);
MPX_TEST("test_state_change_num_windows", {
    for(int i = getCount(); i < 10; i++) {
        addVisibleWindow(getActiveWorkspaceIndex());
        markState();
        assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE);
        assertEquals(getCount(), i + 1);
    }
});
MPX_TEST("test_mask_change", {
    WindowInfo* winInfo = addVisibleWindow(getActiveWorkspaceIndex());
    winInfo->addMask(MAPPED_MASK);

    markState();
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE);

    markState();
    winInfo->addMask(FULLSCREEN_MASK);
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE);

    winInfo->addMask(FULLY_VISIBLE);
    markState();
    assert(!updateState());
});
MPX_TEST("test_layout_change", {
    addVisibleWindow(getActiveWorkspaceIndex());
    updateState();
    Layout l = {"", NULL};
    getActiveWorkspace()->setActiveLayout(&l);
    markState();
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE);
    getActiveWorkspace()->setActiveLayout(NULL);
    markState();
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE);
});

MPX_TEST_ITER("test_num_workspaces_grow", 2, {
    int size = 10;
    addWorkspaces(size);
    updateState();
    markState();
    int num = getNumberOfWorkspaces();
    assert(num > 2);
    if(_i)
        addWorkspaces(size);
    else removeWorkspaces(size / 2);
    assert(num != getNumberOfWorkspaces());
    //detect change only when growing
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE | WORKSPACE_MONITOR_CHANGE);
});

MPX_TEST("test_on_workspace_change", {
    int size = getNumberOfWorkspaces();
    assert(getNumberOfWorkspaces() > 2);
    for(int i = 0; i < size; i++) {
        registerWindow(mapArbitraryWindow(), root);
        getAllWindows().back()->moveToWorkspace(i);
    }
    for(int i = 0; i < size; i++)
        assert(!getWorkspace(i)->getWindowStack().empty());
    markState();
    int count = getCount();
    // windows in invisible workspaces will be unmapped
    assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE | WINDOW_CHANGE);
    assertEquals(getCount(), ++count);
    startWM();
    waitUntilIdle();
    for(int i = 0; i < size; i++) {
        WindowInfo* winInfo = getWorkspace(i)->getWindowStack()[0];
        assert(winInfo);
        assert(winInfo->hasMask(MAPPABLE_MASK));
        bool mapped = winInfo->isNotInInvisibleWorkspace();
        assertEquals(mapped, isWindowMapped(winInfo->getID()));
        assertEquals(mapped, winInfo->hasMask(MAPPED_MASK));
    }
    markState();
    assert(!updateState());
    for(int i = 1; i < size; i++) {
        switchToWorkspace(i);
        markState();
        //workspace hasn't been tiled yet
        ATOMIC(assertEquals(updateState(), WORKSPACE_WINDOW_CHANGE | WORKSPACE_MONITOR_CHANGE));
        waitUntilIdle();
    }
    for(int i = 0; i < size; i++) {
        switchToWorkspace(i);
        markState();
        ATOMIC(assertEquals(updateState(), WORKSPACE_MONITOR_CHANGE));
        waitUntilIdle();
    }
});

MPX_TEST("test_on_invisible_workspace_window_add", {
    switchToWorkspace(0);
    for(int i = 0; i < 2; i++) {
        registerWindow(mapArbitraryWindow(), root);
        getAllWindows().back()->moveToWorkspace(i);
    }
    startWM();
    markState();
    waitUntilIdle();
    lock();
    registerWindow(mapArbitraryWindow(), root);
    getAllWindows().back()->moveToWorkspace(1);
    unlock();
    waitUntilIdle();

    markState();
    int mask = updateState();
    switchToWorkspace(1);
    markState();
    mask |= updateState();
    assert(mask & (WINDOW_CHANGE | WORKSPACE_WINDOW_CHANGE));
});
