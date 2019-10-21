#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib-xcb.h>

#include "../devices.h"
#include "../wm-rules.h"
#include "../logger.h"
#include "../wmfunctions.h"
#include "../window-properties.h"
#include "../functions.h"
#include "../ewmh.h"
#include "../chain.h"
#include "../globals.h"
#include "../layouts.h"
#include "../xsession.h"

#include "tester.h"
#include "test-event-helper.h"
WindowID win;
static void setup() {
    LD_PRELOAD_INJECTION = 1;
    addDieOnIntegrityCheckFailRule();
    onSimpleStartup();
    focusWindow(root);
    saveXSession();
    createMasterDevice("name");
    initCurrentMasters();
    setActiveMaster(getAllMasters()[1]);
    focusWindow(root);
    win = mapArbitraryWindow();
}
SET_ENV(setup, fullCleanup);

MPX_TEST_ITER("dump_focus", 3, {
    assertEquals(getActiveFocus(DEFAULT_KEYBOARD), root);
    assertEquals(getActiveFocus(), root);
    assert(DEFAULT_POINTER != getActiveMasterPointerID());
    if(!spawn(NULL)) {
        if(_i == 0) {
            openXDisplay();
            assertEquals(getClientPointerForWindow(0), getActiveMasterPointerID());
            assertEquals(0, catchError(xcb_set_input_focus_checked(dis, XCB_INPUT_FOCUS_NONE, win, 0)));
            assertEquals(getActiveFocus(), win);
            closeConnection();
        }
        else if(_i == 1) {
            auto dpy = XOpenDisplay(NULL);
            XSetInputFocus(dpy, win, RevertToNone, 0);
            XCloseDisplay(dpy);
        }
        else {
            auto dis = xcb_connect(NULL, NULL);
            xcb_generic_error_t* e = xcb_request_check(dis, xcb_set_input_focus_checked(dis, XCB_INPUT_FOCUS_NONE, win, 0));
            assert(!e);
            xcb_disconnect(dis);
        }
        exit(0);
    }
    assertEquals(waitForChild(0), 0);
    assertEquals(getActiveFocus(DEFAULT_KEYBOARD), root);
    assertEquals(getActiveFocus(), win);
});
