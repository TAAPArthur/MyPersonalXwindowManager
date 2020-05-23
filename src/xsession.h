/**
 * @file xsession.h
 * @brief Create/destroy XConnection and set global X vars
 */

#ifndef MPX_XSESSION_H_
#define MPX_XSESSION_H_


#include <X11/Xlib.h>
#include <xcb/xcb_ewmh.h>
#include <memory>
#include <string>
#include "mywm-structs.h"
#include "window-masks.h"
#include "rect.h"
#include "globals.h"

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


///global graphics context
extern xcb_gcontext_t graphics_context;

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
/// WM_STATE property of the icccm spec
extern xcb_atom_t WM_STATE;

/**
 * MPX_WM_TAKE_FOCUS atom
 * Used to send client messages to direct focus of an application
*/
extern xcb_atom_t WM_TAKE_FOCUS;
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
 * @copydoc getAtom(const char*)
 */
static inline xcb_atom_t getAtom(std::string name) {
    return getAtom(name.c_str());
}
/**
 * Returns a name for the given atom and stores the pointer in value
 *
 * Note that an atom can have multiple names and this method just returns one of them
 *
 * @param atom the atom whose name is wanted
 * @return the name of the atom
 */
std::string getAtomName(xcb_atom_t atom);

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
static inline bool isButton(int buttonOrKey) {return buttonOrKey < 8;}
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
 * @param xlibError 1 iff the error didn't come from xcb
 */
void logError(xcb_generic_error_t* e, bool xlibError = 0);
/**
 * Stringifies type
 *
 * @param type a regular event type
 *
 * @return the string representation of type if known
 */
const char* eventTypeToString(int type);
/**
 * Stringifies opcode
 * @param opcode a major code from a xcb_generic_error_t object
 * @return the string representation of type if known
 */
const char* opcodeToString(int opcode);
/**
 * @param atoms
 * @param numberOfAtoms
 * @return a str representation of the list of atoms
 */
std::string getAtomsAsString(const xcb_atom_t* atoms, int numberOfAtoms);
/**
 * @return 1 iff the last event was synthetic (not sent by the XServer)
 */
int isSyntheticEvent();
/**
 * Returns a monotonically increasing counter indicating the number of times the event loop has been idle. Being idle means event loop has nothing to do at the moment which means it has responded to all prior events
*/
int getIdleCount(void);
/**
 * @return the sequence number of the last event to be queued or 0
 */
uint32_t getLastDetectedEventSequenceNumber();

/**
 * Continually listens and responds to event and applying corresponding Rules.
 * This method will only exit when the x connection is lost
 */
void runEventLoop();
/**
 * To be called when a generic event is received
 * loads info related to the generic event which can be accessed by getLastEvent()
 */
int loadGenericEvent(xcb_ge_generic_event_t* event);
/**
 *Set the last event received.
 * @param event the last event received
 * @see getLastEvent
 */
void setLastEvent(void* event);
/**
 * Retrieves the last event received
 * @see getLastEvent
 */
xcb_generic_event_t* getLastEvent(void);
/**
 * @return the sequence number of the last event that started being processed
 */
uint16_t getCurrentSequenceNumber(void);
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
void getAtomsFromMask(WindowMask masks, ArrayList<xcb_atom_t>& arr, bool action = 0);


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
WindowID createWindow(WindowID parent = root, xcb_window_class_t clazz = XCB_WINDOW_CLASS_INPUT_OUTPUT,
    uint32_t mask = 0, uint32_t* valueList = NULL, const RectWithBorder& dims = {0, 0, 150, 150, 0});

static inline WindowID createOverrideRedirectWindow(xcb_window_class_t clazz = XCB_WINDOW_CLASS_INPUT_ONLY) {
    uint32_t overrideRedirect = 1;
    return createWindow(root, clazz, XCB_CW_OVERRIDE_REDIRECT, &overrideRedirect, {0, 0, 1, 1});
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
std::shared_ptr<xcb_get_property_reply_t> getWindowProperty(WindowID win, xcb_atom_t atom, xcb_atom_t type);

/**
 * Wrapper around getWindowProperty that retries the first value and converts it to an int
 *
 * @param win
 * @param atom
 * @param type
 *
 * @return the value of the window property
 */
int getWindowPropertyValue(WindowID win, xcb_atom_t atom, xcb_atom_t type);
/**
 * Wrapper around getWindowProperty that retries the first value and converts it to a string
 *
 * @param win
 * @param atom
 * @param type
 *
 * @return the value of the window property
 */
std::string getWindowPropertyValueString(WindowID win, xcb_atom_t atom, xcb_atom_t type);

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
template <typename T>
void setWindowProperty(WindowID win, xcb_atom_t atom, xcb_atom_t type, T* arr, uint32_t len) {
    XCALL(xcb_change_property, dis, XCB_PROP_MODE_REPLACE, win, atom, type, sizeof(T) * 8, len, arr);
}

/// @{
/**
 * Wrapper for setWindowProperty
 *
 * @param win the window to set the property on
 * @param atom the atom to set
 * @param type the type of atom
 * @param value the value to post
 */
static inline void setWindowProperty(WindowID win, xcb_atom_t atom, xcb_atom_t type, uint32_t value) {
    setWindowProperty(win, atom, type, &value, 1);
}

static inline void setWindowProperty(WindowID win, xcb_atom_t atom, xcb_atom_t type, std::string value) {
    setWindowProperty(win, atom, type, value.c_str(), value.length() + 1);
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
#endif /* XSESSION_H_ */
