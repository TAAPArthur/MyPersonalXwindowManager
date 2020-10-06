/**
 * @file xsession.h
 * @brief Create/destroy XConnection and set global X vars
 */

#ifndef MPX_XSESSION_H_
#define MPX_XSESSION_H_


#include <X11/Xlib.h>
#include <xcb/xcb_ewmh.h>
#include <string.h>
#include "../mywm-structs.h"
#include "../window-masks.h"
#include "../util/rect.h"
#include "../globals.h"

/**
 * The max number of devices the XServer can support.
 */
#ifndef MAX_NUMBER_OF_DEVICES
#define MAX_NUMBER_OF_DEVICES 40
#endif


/**
 * Shorthand macro to init a X11 atom
 * @param name the name of the atom to init
 */
#define CREATE_ATOM(name)name=getAtom(# name);
/**
 * The max number of master devices the XServer can support.
 *
 * Every call to create a new master creates 4 devices (The master pair and each
 * of their slave devices) In addition the first 2 devices are reserved.
 */
#define MAX_NUMBER_OF_MASTER_DEVICES ((MAX_NUMBER_OF_DEVICES - 2)/4)

/// the default screen index
extern const int defaultScreenNumber;

/**
 * The value of this property is the idle counter
 */
extern xcb_atom_t MPX_IDLE_PROPERTY;

/**
 * Holds the RESTART_COUNTER
 */
extern xcb_atom_t MPX_RESTART_COUNTER;
/**
 * Atom used to differentiate custom client messages
 */
extern xcb_atom_t MPX_WM_INTERPROCESS_COM;
/**
 * Atom used to communicate the return value of the command executed through
 * MPX_WM_INTERPROCESS_COM
 */
extern xcb_atom_t MPX_WM_INTERPROCESS_COM_STATUS;

/// backs X_CENTERED_MASK
extern xcb_atom_t MPX_WM_STATE_CENTER_X;
/// backs Y_CENTERED_MASK
extern xcb_atom_t MPX_WM_STATE_CENTER_Y;
/**
 * Custom atom store in window's state to indicate that it should not be tiled
 */
extern xcb_atom_t MPX_WM_STATE_NO_TILE;

/**
 * Custom atom store in window's state to indicate that this window should
 * have the ROOT_FULLSCREEN mask
 */
extern xcb_atom_t MPX_WM_STATE_ROOT_FULLSCREEN;

/// ICCCM client message to change window state to hidden;
extern xcb_atom_t WM_CHANGE_STATE;
/**
 * MPX_WM_DELETE_WINDOW atom
 * Used to send client messages to delete the window
*/
extern xcb_atom_t WM_DELETE_WINDOW;
/**
 * Atom for the MPX_WM_SELECTION for the default screen
 */
extern xcb_atom_t WM_SELECTION_ATOM;
extern xcb_atom_t MPX_WM_SELECTION_ATOM;
/// WM_STATE property of the icccm spec
extern xcb_atom_t WM_STATE;

/// Some windows have a applications specified role, (like browser) stored in this atom
extern xcb_atom_t WM_WINDOW_ROLE;

/**XDisplay instance (only used for events/device commands)*/
extern Display* dpy;
/**XCB display instance*/
extern xcb_connection_t* dis;
/**EWMH instance*/
extern xcb_ewmh_connection_t* ewmh;
/**Root window*/
extern WindowID root;
/**Default screen (assumed to be only 1*/
extern xcb_screen_t* screen;

/**
 * Opens a connection to the X Server designated by the DISPLAY env variable
 *
 * It will wait up to 100ms for a connection to become available. If an X Server on this specific display is not present, the program will exit with status 1
 *
 * @see connectToXserver
 */
void openXDisplay(void);

/**
 * Closes an open XConnection
 *
 * If is not safe to call this method on an already closed connection
 */
void closeConnection(void);

/**
 * Flush the X connection
 *
 * This method is just a wrapper for XFlush and xcb_flush
 */
static inline void flush(void) {
    xcb_flush(dis);
    XFlush(dpy);
}

/**
 * Returns the atom value for the given name.
 *
 * If the name is unmapped, then a new atom is created and assigned to the name
 *
 * @param name
 *
 * @return
 */
xcb_atom_t getAtom(const char* name);
/**
 * Returns a name for the given atom and stores the pointer in value
 *
 * Note that an atom can have multiple names and this method just returns one of them
 *
 * @param atom the atom whose name is wanted
 * @return the name of the atom
 */
char* getAtomName(xcb_atom_t atom, char* buffer);

/**
 *
 * @param keysym
 * @return the key code for the given keysym
 */
int getKeyCode(int keysym);

/**
 * Returns true if buttonOrKey is a valid button.
 * A valid button is less than 8.
 *
 * @param buttonOrKey
 *
 * @return 1 if buttonOrKey is a valid button value
 */
static inline bool isButton(int buttonOrKey) {return buttonOrKey && buttonOrKey < 8;}
/**
 * Checks to see if buttonOrKey is a button or not.
 * If it is, return buttonOrKey
 * Else convert it to a key code and return that value
 *
 * @param buttonOrKey
 *
 * @return the button detail or keyCode from buttonOrKey
 */
int getButtonDetailOrKeyCode(int buttonOrKey);
/**
 * If it does not already exist creates a window to be used as proof-of-life.
 * If it already exists, previously return the previously returned window
 *
 * The value will change on every new XConnection
 *
 * @return
 */
xcb_window_t getPrivateWindow(void);
/**
 * @see catchError
 */
int catchErrorSilent(xcb_void_cookie_t cookie);
/**
 * Catches an error and log it
 *
 * @param cookie the result of and xcb_.*_checked function
 *
 * @return the error_code if error was found or 0
 * @see logError
 */
int catchError(xcb_void_cookie_t cookie);
/**
 * Prints info related to the error
 *
 * It may trigger an assert; @see CRASH_ON_ERRORS
 *
 * @param e
 */
void logError(xcb_generic_error_t* e);
/**
 * Stringifies opcode
 * @param opcode a major code from a xcb_generic_error_t object
 * @return the string representation of type if known
 */
const char* opcodeToString(int opcode);
/**
 *
 *
 * @param atom
 * @see getAtomsFromMask
 *
 * @return the WindowMask(s) this atom represents
 */
WindowMask getMaskFromAtom(xcb_atom_t atom) ;
/**
 *
 *
 * @param masks
 * @param arr an sufficiently large array used to store the atoms retrieved from masks
 * @param action whether to return ACTION or STATE atoms
 *
 * @see getMaskFromAtom
 *
 */
int getAtomsFromMask(WindowMask masks, bool action, xcb_atom_t* arr);


/**
 * @return a unique X11 id
 */
uint32_t generateID();


/**
 * @param parent
 * @param clazz
 * @param mask
 * @param valueList
 * @param dims starting dimension of the new window
 *
 * @return the id of the newly created window
 */
//WindowID createWindow(WindowID parent = root, xcb_window_class_t clazz = XCB_WINDOW_CLASS_INPUT_OUTPUT, uint32_t mask = 0, uint32_t* valueList = NULL, const RectWithBorder& dims = {0, 0, 150, 150, 0});
WindowID createWindow(WindowID parent, xcb_window_class_t clazz, uint32_t mask, uint32_t* valueList,
    const Rect dims);

static inline WindowID createOverrideRedirectWindow() {
    uint32_t overrideRedirect = 1;
    return createWindow(root, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, &overrideRedirect, (Rect) {0, 0, 1, 1});
}

/**
 * Maps the window
 * @param id
 * @return id
 */
WindowID mapWindow(WindowID id);
/**
 * Unmaps the window
 * @param id
 */
void unmapWindow(WindowID id);
/**
 * Destroys win but not the underlying client.
 * The underlying client may choose to die if win is closed.
 *
 * @param win
 *
 * @return 0 on success or the error
 */
int destroyWindow(WindowID win);

void destroyWindowInfo(WindowInfo* winInfo);

/**
 * @return 1 iff a connection to the xserver has been opened
 */
bool hasXConnectionBeenOpened();

/**
 * Loads and returns info for an X11 property
 *
 * Wraps xcb_get_property and related methods
 *
 * @param win the window the property is stored on
 * @param atom the atom we want
 * @param type the type of atom (ie XCB_ATOM_STRING)
 *
 * @return xcb_get_property_reply_t or NULL
 */
xcb_get_property_reply_t* getWindowProperty(WindowID win, xcb_atom_t atom, xcb_atom_t type);

/**
 * Wrapper around getWindowProperty that retries the first value and converts it to an int
 *
 * @param win
 * @param atom
 * @param type
 *
 * @return the value of the window property
 */
int getWindowPropertyValueInt(WindowID win, xcb_atom_t atom, xcb_atom_t type);
/**
 * Wrapper around getWindowProperty that retries the first value and converts it to a string
 *
 * @param win
 * @param atom
 * @param type
 *
 * @return the value of the window property
 */
char* getWindowPropertyString(WindowID win, xcb_atom_t atom, xcb_atom_t type, char* result);

/// When NDEBUG is set XCALL will call the checked version of X and check for errors
#ifndef NDEBUG
#define XCALL(X, args...) catchError(_CAT(X,_checked)(args))
#else
#define XCALL(X, args...) X(args)
#endif
/**
 *
 * Wrapper for xcb_change_property with XCB_PROP_MODE_REPLACE
 *
 * @tparam T
 * @param win
 * @param atom
 * @param type
 * @param arr a array of values to set to win
 * @param len the number of members of arr
 */
#define setWindowProperty(win, atom, type, arr, len) \
    XCALL(xcb_change_property, dis, XCB_PROP_MODE_REPLACE, win, atom, type, sizeof((arr)[0]) * 8, len, arr);

/// @{
/**
 * Wrapper for setWindowProperty
 *
 * @param win the window to set the property on
 * @param atom the atom to set
 * @param type the type of atom
 * @param value the value to post
 */
static inline void setWindowPropertyInt(WindowID win, xcb_atom_t atom, xcb_atom_t type, uint32_t value) {
    setWindowProperty(win, atom, type, &value, 1);
}

static inline void setWindowPropertyString(WindowID win, xcb_atom_t atom, xcb_atom_t type, const char* value) {
    setWindowProperty(win, atom, type, value, strlen(value) + 1);
}
/// @}

/**
 * Removes the atom from the specified window
 *
 * @param win
 * @param atom
 */
static inline void clearWindowProperty(WindowID win, xcb_atom_t atom) {
    XCALL(xcb_delete_property, dis, win, atom);
}

void dumpAtoms(const xcb_atom_t* atoms, int numberOfAtoms);
#endif /* XSESSION_H_ */
