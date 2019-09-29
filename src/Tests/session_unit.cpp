#include "../layouts.h"
#include "../ewmh.h"
#include "../wmfunctions.h"
#include "../devices.h"
#include "../masters.h"
#include "../default-rules.h"
#include "../session.h"
#include "tester.h"
#include "test-mpx-helper.h"
#include "test-x-helper.h"
#include "test-event-helper.h"

SET_ENV(onStartup, fullCleanup);
MPX_TEST("test_sync_state", {
    saveXSession();
    activateWorkspace(0);
    setShowingDesktop(0);
    assert(!isShowingDesktop());
    assert(getActiveWorkspaceIndex() == 0);
    if(!fork()) {
        fullCleanup();
        openXDisplay();
        setActiveWorkspaceProperty(1);
        setShowingDesktop(1);
        assert(isShowingDesktop());
        flush();
        closeConnection();
        quit(0);
    }
    assertEquals(waitForChild(0), 0);
    syncState();
    assertEquals(getActiveWorkspaceIndex(), 1);
    assert(isShowingDesktop());
});

MPX_TEST("test_invalid_state", {
    WindowID win = mapArbitraryWindow();
    xcb_ewmh_set_wm_desktop(ewmh, win, getNumberOfWorkspaces() + 1);
    xcb_ewmh_set_current_desktop(ewmh, defaultScreenNumber, getNumberOfWorkspaces() + 1);
    flush();
    addResumeRules();
    syncState();
    scan(root);
    assert(getActiveWorkspaceIndex() < getNumberOfWorkspaces());
    assert(getWindowInfo(win)->getWorkspaceIndex() < getNumberOfWorkspaces());
});

static long rectToLong(const Rect& r) {
    return ((long)r.x) << 48L | ((long)r.y) << 32L | r.width << 16L | r.height;
}
const ArrayList<long>serializeState(uint8_t mask) {
    ArrayList<long> list;
    ArrayList<ArrayList<WindowInfo*>*> listsOfStacks;
    if(mask & 1) {
        list.add(getAllMasters().size());
        for(Master* master : getAllMasters()) {
            list.add(master->getID());
            listsOfStacks.add(&master->getWindowStack());
        }
    }
    if(mask & 2) {
        list.add(getNumberOfWorkspaces());
        for(Workspace* workspace : getAllWorkspaces()) {
            list.add(workspace->getID());
            list.add(workspace->getMonitor() ? workspace->getMonitor()->getID() : 0);
            list.add(workspace->getLayoutOffset());
            list.add(std::hash<std::string> {}(workspace->getActiveLayout() ? workspace->getActiveLayout()->getName() : ""));
            listsOfStacks.add(&workspace->getWindowStack());
        }
    }
    if(mask & 4) {
        list.add(getAllMonitors().size());
        for(Monitor* monitor : getAllMonitors()) {
            list.add(monitor->getID());
            list.add(rectToLong(monitor->getBase()));
            list.add(rectToLong(monitor->getViewport()));
            list.add(monitor->getWorkspace() ? monitor->getWorkspace()->getID() : 0);
        }
    }
    if(mask & 8)
        for(ArrayList<WindowInfo*>* stack : listsOfStacks) {
            list.add(0);
            for(WindowInfo* winInfo : *stack)
                list.add(winInfo->getID());
        }
    return list;
}

SET_ENV(onStartup, fullCleanup);
MPX_TEST_ITER("test_restore_state", 16, {
    int mask = _i;
    CRASH_ON_ERRORS = -1;
    saveCustomState();
    flush();
    loadCustomState();
    createMasterDevice("test");
    createMasterDevice("test2");
    initCurrentMasters();
    MONITOR_DUPLICATION_POLICY = 0;
    Monitor* ref = getActiveWorkspace()->getMonitor();
    createFakeMonitor(ref->getBase());
    createFakeMonitor(ref->getBase());
    // clear current monitors to ensure we load everything in the correct order
    getAllMonitors().deleteElements();
    detectMonitors();
    swapMonitors(0, 2);
    swapMonitors(1, 4);
    for(int i = 0; i < 10; i++)
        createNormalWindow();
    scan(root);
    for(WindowInfo* winInfo : getAllWindows())
        getActiveMaster()->onWindowFocus(winInfo->getID());
    setActiveMaster(getAllMasters()[1]);
    getActiveMaster()->onWindowFocus(getAllWindows()[0]->getID());
    getAllWindows()[1]->moveToWorkspace(!getActiveWorkspaceIndex());
    getAllWindows()[0]->moveToWorkspace(getActiveWorkspaceIndex());
    Layout l = Layout("VERY_VERY_VERY_VERY_VERY_VERY_VERY_VERY_LONG_NAME", NULL);
    registerLayouts(&l);
    setActiveLayout(&l);
    switchToWorkspace(1);
    setActiveLayout(NULL);
    const auto& list = serializeState(mask);
    saveCustomState();
    flush();
    if(!fork()) {
        saveXSession();
        RUN_AS_WM = 0;
        clearAllLists();
        RUN_AS_WM = 0;
        onStartup();
        loadCustomState();
        const auto& list2 = serializeState(mask);
        for(int i = 0; i < list.size(); i++) {
            if(list[i] != list2[i])
                std::cout << i << " " << list[i] << " " << list2[i] << "\n";
        }
        assertEquals(list.size(), list2.size());
        assert(list == list2);
        fullCleanup();
        quit(0);
    }
    assertEquals(waitForChild(0), 0);
    destroyAllNonDefaultMasters();
    clearFakeMonitors();
    initCurrentMasters();
    detectMonitors();
    loadCustomState();
    saveCustomState();
});
