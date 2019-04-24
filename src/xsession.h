/**
 * @file xsession.h
 * @brief Create/destroy XConnection and set global X vars
 */

#ifndef XSESSION_H_
#define XSESSION_H_


#include <X11/Xlib.h>
#include <xcb/xcb_ewmh.h>


///global graphics context
extern xcb_gcontext_t graphics_context;

/**
 * WM_TAKE_FOCUS atom
 * Used to send client messages to direct focus of an application
*/
extern xcb_atom_t WM_TAKE_FOCUS;

/**
 * WM_DELETE_WINDOW atom
 * Used to send client messages to delete the window
*/
extern xcb_atom_t WM_DELETE_WINDOW;

/**
 *Custom atom store in window's state to indicate that it should not be tield
 */
extern xcb_atom_t WM_STATE_NO_TILE;

/**
 *Custom atom store in window's state to indicate that this window should
 * have the ROOT_FULLSCREEN mask
 */
extern xcb_atom_t WM_STATE_ROOT_FULLSCREEN;

/**
 * Atom for the WM_SELECTION for the default screen
 */
extern xcb_atom_t WM_SELECTION_ATOM;

/**
 * Atom used to differenciate custom client messages
 * Atoms with the flag are used to remotly interact with the WM
 */
extern xcb_atom_t WM_INTERPROCESS_COM;

/**XDisplay instance (only used for events/device commands)*/
extern Display* dpy;
/**XCB display instance*/
extern xcb_connection_t* dis;
/**EWMH instance*/
extern xcb_ewmh_connection_t* ewmh;
/**Root window*/
extern int root;
/**Default screen (assumed to be only 1*/
extern xcb_screen_t* screen;

/**
 * Opens a connection to the Xserver
 *
 * This method creates an Xlib display and converts into an
 * xcb_connection_t instance and set the event queue to support xcb.
 * It also sets the close down mode to XCB_CLOSE_DOWN_DESTROY_ALL
 *
 * This method probably should not be called directly.
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
void flush(void);

#endif /* XSESSION_H_ */
