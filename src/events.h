/**
 * @file events.h
 *
 * @brief Handles geting and processing XEvents
 */

#ifndef EVENTS_H_
#define EVENTS_H_

#include <pthread.h>
#include <xcb/xcb.h>

#include <X11/extensions/XInput2.h>
#include <xcb/xinput.h>
#include "mywm-structs.h"

/// The last supported standard x event
#define GENERIC_EVENT_OFFSET (LASTEvent-1)
/// max value of supported X events (not total events)
#define LAST_REAL_EVENT  (GENERIC_EVENT_OFFSET+XI_LASTEVENT+1)

//TODO consistent naming
enum {
    /**
     * X11 events that are >= LASTEvent but not the first XRANDR event.
     * In other words events that are unknown/unaccounted for.
     * The value 1 is safe to use because is reserved for X11 replies
     */
    ExtraEvent = 1,
    ///if all rules are passed through, then the window is added as a normal window
    onXConnection = LAST_REAL_EVENT,
    /// Run after properties have been loaded
    PropertyLoad,
    /// deterime if a newly detected window should be recorded/monitored/controlled by us
    ProcessingWindow,
    /// called after the newly created window has been added to a workspace
    RegisteringWindow,
    /// triggered when root screen is changed
    onScreenChange,
    /// when a workspace is tiled
    TileWorkspace,
    /**
     * Called anytime a managed window is configured. The filtering out of ignored windows is one of the main differences between this and XCB_CONFIGURE_NOTIFY. THe other being that the WindowInfo object will be passed in when the rule is applied.
     */
    onWindowMove,
    /// called after a set number of events or when the connection is idle
    Periodic,
    /// called when the connection is idle
    Idle,
    /// max value of supported events
    NUMBER_OF_EVENT_RULES
};

/**
 * execute code in an atomic manner
 * @param code
 */
#define ATOMIC(code...) do {lock();code;unlock();}while(0)
/**
 * Locks/unlocks the global mutex
 *
 * It is not safe to modify most structs from multiple threads so the main event loop lock/unlocks a
 * global mutex. Any addition thread that runs alongside the main thread of if in general, there
 * is a race, lock/unlock should be used
 */
void lock(void);
///cpoydoc lock(void)
void unlock(void);

/**
 * Requests all threads to terminate
 */
void requestShutdown(void);
/**
 * Indicate to threads that the system is shutting down;
 */
int isShuttingDown(void);

/**
 *Returns a monotonically increasing counter indicating the number of times the event loop has been idel. Being idle means event loop has nothing to do at the moment which means it has responded to all prior events
*/
int getIdleCount(void);
/**
 * Runs method in a new thread
 * @param method the method to run
 * @param arg the argument to pass into method
 * @param detached creates a detached thread; When a detached thread terminates, its resources are automatically released back to the system without the need for another thread to join with the terminated thread.
 * @return a pthread identifier
 */
pthread_t runInNewThread(void* (*method)(void*), void* arg, int detached);
/**
 * @param i index of eventRules
 * @return the specified event list
 */
ArrayList* getEventRules(int i);
/**
 * Removes all rules from the all eventRules
 */
void clearAllRules(void);

/**
 * @param i index of batch event rules
 * @return the specified event list
 */
ArrayList* getBatchEventRules(int i);
/**
 * Increment the counter for a batch rules.
 * All batch rules with a non zero counter will be triggerd
 * on the next call to incrementBatchEventRuleCounter()
 *
 * @param i the event type
 */
void incrementBatchEventRuleCounter(int i);
/**
 * For every event rule that has been triggered since the last call
 * incrementBatchEventRuleCounter(), trigger the corrosponding batch rules
 *
 */
void applyBatchRules(void);
/**
 * Clear all batch event rules
 */
void clearAllBatchRules(void);

/**
 * TODO rename
 * Continually listens and responds to event and applying corresponding Rules.
 * This method will only exit when the x connection is lost
 * @param arg unused
 */
void* runEventLoop(void* arg);
/**
 * Attemps to translate the generic event receive into an extension event and applyies corresponding Rules.
 */
void onGenericEvent(void);
/**
 * TODO rename
 * Register ROOT_EVENT_MASKS
 * @see ROOT_EVENT_MASKS
 */
void registerForEvents();

/**
 * Declare interest in select window events
 * @param window
 * @param mask
 * @return the error code if an error occured
 */
int registerForWindowEvents(WindowID window, int mask);

/**
 * Apply the list of rules designated by the type
 *
 * @param type
 * @param winInfo
 * @return the result
 */
int applyEventRules(int type, WindowInfo* winInfo);

/**
 * To be called when a generic event is received
 * loads info related to the generic event which can be accesed by getLastEvent()
 */
int loadGenericEvent(xcb_ge_generic_event_t* event);

/**
 * Grab all specified keys/buttons and listen for select device events on events
 */
void registerForDeviceEvents();
/**
 * Request to be notifed when info related to the monitor/screen changes
 * This method does nothing if compiled without XRANDR support
 */
void registerForMonitorChange();

/**
 * @return 1 iff the last event was synthetic (not sent by the XServer)
 */
int isSyntheticEvent();
#endif /* EVENTS_H_ */
