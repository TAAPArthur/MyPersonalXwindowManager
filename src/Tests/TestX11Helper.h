
#ifndef TESTS_XUNITTESTS_H_
#define TESTS_XUNITTESTS_H_

#include "UnitTests.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib-xcb.h>

#include "../default-rules.h"
#include "../xsession.h"
#include "../windows.h"
#include "../logger.h"
#include "../events.h"
#include "../bindings.h"
#include "../globals.h"
#include "../test-functions.h"

#define START_MY_WM \
        runInNewThread(runEventLoop,NULL,"WM")

void destroyWindow(WindowID win);
int  createInputOnlyWindow(void);
int createUserIgnoredWindow(void);
int createNormalWindowWithType(xcb_atom_t type);
int  createUnmappedWindow(void);
void addDummyIgnoreRule(void);
int createIgnoredWindow(void);
int createNormalWindow(void);
int createInputWindow(int input);
int createNormalSubWindow(int parent);
int mapWindow(int win);
int  mapArbitraryWindow(void);
void createSimpleContext();
void createSimpleContextWthMaster();
void createContextAndSimpleConnection();
void destroyContextAndConnection();
void openXDisplay();
void triggerAllBindings(int mask);
void triggerBinding(Binding* b);
void* getNextDeviceEvent();
void waitToReceiveInput(int mask, int detail);
int consumeEvents();
int waitForNormalEvent(int mask);

int isWindowMapped(WindowID win);
Window setDock(WindowID id, int i, int size, int full);
Window createDock(int i, int size, int full);
void fullCleanup();

void loadSampleProperties(WindowInfo* winInfo);
int getExitStatusOfFork();
void waitForExit(int signal);
void waitForCleanExit();
void setProperties(WindowID win);
void checkProperties(WindowInfo* winInfo);
/**
 * checks to marking the window ids in stacking are in bottom to top order
 */
int checkStackingOrder(WindowID* stackingOrder, int num);
static inline void* isInList(ArrayList* list, int value){
    return find(list, &value, sizeof(int));
}
static inline void waitUntilIdle(void){
    static int idleCount;
    WAIT_UNTIL_TRUE(idleCount != getIdleCount());
    idleCount = getIdleCount();
}
#endif /* TESTS_UNITTESTS_H_ */
