#include <assert.h>
#include <string.h>
#include "unistd.h"

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/Xlib-xcb.h>

#include "../globals.h"
#include "../util/logger.h"
#include "../monitors.h"
#include "../boundfunction.h"
#include "../system.h"
#include "../util/debug.h"
#include "../util/time.h"
#include "../user-events.h"
#include "xsession.h"


xcb_atom_t MPX_IDLE_PROPERTY;
xcb_atom_t MPX_RESTART_COUNTER;
xcb_atom_t MPX_WM_INTERPROCESS_COM;
xcb_atom_t MPX_WM_INTERPROCESS_COM_STATUS;
xcb_atom_t MPX_WM_STATE_CENTER_X;
xcb_atom_t MPX_WM_STATE_CENTER_Y;
xcb_atom_t MPX_WM_STATE_NO_TILE;
xcb_atom_t MPX_WM_STATE_ROOT_FULLSCREEN;
xcb_atom_t WM_CHANGE_STATE;
xcb_atom_t WM_DELETE_WINDOW;
xcb_atom_t WM_SELECTION_ATOM;
xcb_atom_t MPX_WM_SELECTION_ATOM;
xcb_atom_t WM_STATE;
xcb_atom_t WM_WINDOW_ROLE;
xcb_atom_t OPTION_NAME;
xcb_atom_t OPTION_VALUES;

Display* dpy;
xcb_connection_t* dis;
WindowID root;
xcb_ewmh_connection_t* ewmh;
xcb_screen_t* screen;
const int defaultScreenNumber = 0;

/**
 * Window created by us to show to the world that an EWMH compliant WM is active
 */
static WindowID compliantWindowManagerIndicatorWindow;


WindowID getPrivateWindow(void) {
    if(!compliantWindowManagerIndicatorWindow) {
        compliantWindowManagerIndicatorWindow = createOverrideRedirectWindow(XCB_WINDOW_CLASS_INPUT_ONLY);
        xcb_ewmh_set_wm_pid(ewmh, compliantWindowManagerIndicatorWindow, getpid());
    }
    return compliantWindowManagerIndicatorWindow;
}

typedef struct AtomMaskPair {
    uint32_t atom;
    WindowMask mask;
} AtomMaskPair;

static AtomMaskPair atomStateToMask[13];
static AtomMaskPair atomActionToMask[9];

static void createMaskAtomMapping() {
    AtomMaskPair _atomStateToMask[] = {
        {ewmh->_NET_WM_STATE_MODAL, MODAL_MASK},
        {ewmh->_NET_WM_STATE_ABOVE, ABOVE_MASK | NO_TILE_MASK},
        {MPX_WM_STATE_CENTER_X, X_CENTERED_MASK},
        {MPX_WM_STATE_CENTER_Y, Y_CENTERED_MASK},
        {MPX_WM_STATE_NO_TILE, NO_TILE_MASK},
        {MPX_WM_STATE_ROOT_FULLSCREEN, ROOT_FULLSCREEN_MASK},
        {ewmh->_NET_WM_STATE_BELOW, BELOW_MASK | NO_TILE_MASK},
        {ewmh->_NET_WM_STATE_FULLSCREEN, FULLSCREEN_MASK},
        {ewmh->_NET_WM_STATE_HIDDEN, HIDDEN_MASK},
        {ewmh->_NET_WM_STATE_STICKY, STICKY_MASK},
        {ewmh->_NET_WM_STATE_DEMANDS_ATTENTION, URGENT_MASK},
        {ewmh->_NET_WM_STATE_MAXIMIZED_HORZ, X_MAXIMIZED_MASK},
        {ewmh->_NET_WM_STATE_MAXIMIZED_VERT, Y_MAXIMIZED_MASK},
    };
    AtomMaskPair _atomActionToMask[] = {
        {ewmh->_NET_WM_ACTION_ABOVE, ABOVE_MASK | NO_TILE_MASK},
        {ewmh->_NET_WM_ACTION_BELOW, BELOW_MASK | NO_TILE_MASK},
        {ewmh->_NET_WM_ACTION_FULLSCREEN, FULLSCREEN_MASK},
        {ewmh->_NET_WM_ACTION_MINIMIZE, HIDDEN_MASK},
        {ewmh->_NET_WM_ACTION_MOVE, EXTERNAL_MOVE_MASK},
        {ewmh->_NET_WM_ACTION_RESIZE, EXTERNAL_RESIZE_MASK},
        {ewmh->_NET_WM_ACTION_STICK, STICKY_MASK},
        {ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ, X_MAXIMIZED_MASK},
        {ewmh->_NET_WM_ACTION_MAXIMIZE_VERT, Y_MAXIMIZED_MASK},
    };
    assert(sizeof(atomStateToMask) == sizeof(_atomStateToMask));
    memcpy(atomStateToMask, _atomStateToMask, sizeof(atomStateToMask));
    assert(sizeof(atomActionToMask) == sizeof(_atomActionToMask));
    memcpy(atomActionToMask, _atomActionToMask, sizeof(atomActionToMask));
}
WindowMask getMaskFromAtom(xcb_atom_t atom) {
    for(int i = 0; i < LEN(atomStateToMask); i++)
        if(atomStateToMask[i].atom == atom)
            return atomStateToMask[i].mask;
    return NO_MASK;
}
int getAtomsFromMask(WindowMask mask, bool action, xcb_atom_t* arr) {
    AtomMaskPair* pair = action ? atomActionToMask : atomStateToMask;
    size_t size = action ? LEN(atomActionToMask) : LEN(atomStateToMask);
    int count = 0;
    for(int i = 0; i < size; i++) {
        if((pair[i].mask & mask) == pair[i].mask)
            arr[count++] = pair[i].atom;
    }
    DEBUG("getAtomsFromMask found %d atoms for Mask %s", count, getMaskAsString(mask, NULL));
    return count;
}

bool hasXConnectionBeenOpened() {
    return dpy ? 1 : 0;
}

static int handleXLibError(Display* dpy __attribute__((unused)), XErrorEvent* e) {
    xcb_generic_error_t error = {.response_type = (uint8_t)e->type, .error_code = e->error_code, .resource_id = (uint32_t)e->resourceid, .minor_code = e->minor_code, .major_code = e->request_code};
    ERROR("XLib error");
    logError(&error);
    return 0;
}
void openXDisplay(void) {
    //XInitThreads();
    DEBUG(" connecting to XServer ");
    dpy = XOpenDisplay(NULL);
    if(!dpy) {
        ERROR(" Failed to connect to xserver");
        exit(X_ERROR);
    }
    dis = XGetXCBConnection(dpy);
    assert(!xcb_connection_has_error(dis));
    xcb_intern_atom_cookie_t* cookie;
    bool applyRule = ewmh == NULL;
    ewmh = (xcb_ewmh_connection_t*)malloc(sizeof(xcb_ewmh_connection_t));
    cookie = xcb_ewmh_init_atoms(dis, ewmh);
    xcb_ewmh_init_atoms_replies(ewmh, cookie, NULL);
    CREATE_ATOM(MPX_IDLE_PROPERTY);
    CREATE_ATOM(MPX_RESTART_COUNTER);
    CREATE_ATOM(MPX_WM_INTERPROCESS_COM);
    CREATE_ATOM(MPX_WM_INTERPROCESS_COM_STATUS);
    CREATE_ATOM(MPX_WM_STATE_CENTER_X);
    CREATE_ATOM(MPX_WM_STATE_CENTER_Y);
    CREATE_ATOM(MPX_WM_STATE_NO_TILE);
    CREATE_ATOM(MPX_WM_STATE_ROOT_FULLSCREEN);
    CREATE_ATOM(OPTION_NAME);
    CREATE_ATOM(OPTION_VALUES);
    CREATE_ATOM(WM_CHANGE_STATE);
    CREATE_ATOM(WM_DELETE_WINDOW);
    CREATE_ATOM(WM_STATE);
    CREATE_ATOM(WM_WINDOW_ROLE);
    screen = ewmh->screens[0];
    setRootDims(&screen->width_in_pixels);
    root = screen->root;
    char selectionName[32];
    sprintf(selectionName, "WM_S%d", defaultScreenNumber);
    WM_SELECTION_ATOM = getAtom(selectionName);
    sprintf(selectionName, "MPX_WM_S%d", defaultScreenNumber);
    MPX_WM_SELECTION_ATOM = getAtom(selectionName);
    XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
    XSetErrorHandler(handleXLibError);
    compliantWindowManagerIndicatorWindow = 0;
    createMaskAtomMapping();
    if(applyRule)
        applyEventRules(X_CONNECTION, NULL);
}

void closeConnection(void) {
    if(ewmh) {
        DEBUG("closing X connection");
        xcb_ewmh_connection_wipe(ewmh);
        free(ewmh);
        ewmh = NULL;
        compliantWindowManagerIndicatorWindow = 0;
        if(dpy)
            XCloseDisplay(dpy);
    }
}
