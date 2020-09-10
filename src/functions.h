/**
 * @file functions.h
 *
 * @brief functions that users may add to their config that are not used elsewhere
 */


#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_


#include "boundfunction.h"
#include "bindings.h"
#include "masters.h"
#include "monitors.h"
#include "wmfunctions.h"

/// Direction towards the head of the list
#define UP -1
/// Direction away from the head of the list
#define DOWN 1

/// arguments to findAndRaise
enum WindowAction {
    /// @{ performs the specified action
    ACTION_NONE, ACTION_RAISE, ACTION_FOCUS, ACTION_ACTIVATE, ACTION_LOWER
    /// @}
};

/// Flags to findAndRaise
struct FindAndRaiseArg {
    /// checkLocalFirst checks the master window stack before checking all windows
    bool checkLocalFirst = 1;
    /// cache ignores recently chosen windows. The cache is cleared if the focused window doesn't match rule or a window cannot be found otherwise
    bool cache = 1;
    /// only consider activatable windows
    bool includeNonActivatable = 0;
};

/// Determines what WindowInfo field to match on
enum RaiseOrRunType {
    MATCHES_TITLE,
    MATCHES_CLASS,
    MATCHES_ROLE,
} ;
/**
 * Attempts to find a that rule matches a managed window.
 * First the active Master's window& stack is checked ignoring the master's window cache.
 * Then all windows are checked
 * and finally the master's window complete window& stack is checked
 * @param rule
 * @param action
 * @param arg
 * @param master
 * @return 1 if a matching window was found
 */
WindowInfo* findAndRaise(const BoundFunction& rule, WindowAction action = ACTION_ACTIVATE, FindAndRaiseArg arg = {},
    Master* master = getActiveMaster());


/**
 * Wrapper around findAndRaise
 * @param s string to match against
 * @param matchType what property of the window to match
 * @param action what to do if a window was found
 *
 * @return if a matching window was found
 */
bool findAndRaise(std::string s, RaiseOrRunType matchType = MATCHES_CLASS, WindowAction action = ACTION_ACTIVATE);
/**
 * Finds windows with the same class as the currenrtly focused window
 * @see findAndRaise
 *
 * @param winInfo
 *
 * @return
 */
static inline bool findAndRaiseMatching(const WindowInfo* winInfo) {return findAndRaise(winInfo->getClassName());}
/**
 * Tries to find a window by checking all the managed windows, and if it finds one it activates it
 *
 * @param rule
 *
 * @return the window found or NULL
 */
static inline WindowInfo* findAndRaiseSimple(const BoundFunction& rule) {return findAndRaise(rule, ACTION_ACTIVATE, {.checkLocalFirst = 0, .cache = 0}, 0);}
/**
 * @param winInfo
 * @param str
 *
 * @return 1 iff the window class or instance name is equal to str
 */
bool matchesClass(WindowInfo* winInfo, std::string str);
/**
 * @param winInfo
 * @param str
 *
 * @return if the window title is equal to str
 */
bool matchesTitle(WindowInfo* winInfo, std::string str);

/**
 * @param winInfo
 * @param str
 *
 * @return if the window role is equal to str
 */
bool matchesRole(WindowInfo* winInfo, std::string str);
/**
 * Tries to raise a window that matches s. If it cannot find one, spawn cmd
 *
 * @param s
 * @param cmd command to spawn
 * @param matchType whether to use matchesClass or matchesTitle
 * @param silent if 1 redirect stderr and out of child process to /dev/null
 * @param preserveSession if true, spawn won't double fork
 *
 * @return 0 iff a window was found else the pid of the spawned process
 */
int raiseOrRun(std::string s, std::string cmd, RaiseOrRunType matchType = MATCHES_CLASS, bool silent = 1,
    bool preserveSession = 0);
/**
 * Tries to raise a window with class (or resource) name equal to s.
 * If not it spawns s
 *
 * @param s the class or instance name to be raised or the program to spawn
 *
 * @return 0 iff the program was spawned
 */
static inline int raiseOrRun(std::string s) {
    return raiseOrRun(s, s);
}

/**
 * Call to stop cycling windows
 *
 * Unfreezes the master window& stack
 */
void endCycleWindows(Master* m = getActiveMaster());
/**
 * Freezes and cycle through windows in the active master's& stack
 * @param delta
 * @param filter only cycle throught windows matching filter
 */
void cycleWindowsMatching(int delta, bool(*filter)(WindowInfo*) = NULL);
static inline void cycleWindows(int delta) {cycleWindowsMatching(delta);}


/**
 * Activates a window with URGENT_MASK
 *
 * @param action
 *
 * @return 1 iff there was an urgent window
 */
bool activateNextUrgentWindow(WindowAction action = ACTION_ACTIVATE);
/**
 * Removes HIDDEN_MASK from a recent hidden window
 * @param action
 *
 * @return 1 a window was found and changed
 */
bool popHiddenWindow(WindowAction action = ACTION_ACTIVATE);

//////Run or Raise code

/**
 * Checks to see if any window in searchList matches rule ignoring any in ignoreList
 * @param rule
 * @param searchList
 * @param ignoreList can be NULL
 * @param includeNonActivatable
 * @return the first window that matches rule or NULL
 */
WindowInfo* findWindow(const BoundFunction& rule, const ArrayList<WindowInfo*>& searchList,
    ArrayList<WindowID>* ignoreList = NULL, bool includeNonActivatable = 0);


/**
 * Send the active window to the first workspace with the given name
 * @param winInfo
 * @param name
 */
void sendWindowToWorkspaceByName(WindowInfo* winInfo, std::string name);

/**
 * Swap the active window with the window dir windows away from its current position in the worked space& stack. The stack wraps around if needed
 * @param dir
 * @param stack
 */
void swapPosition(int dir, ArrayList<WindowInfo*>& stack = getActiveWindowStack());
/**
 * Shifts the focus up or down the window& stack
 * @param index
 * @param filter ignore windows not matching filter
 * @param stack
 * @return
 */
int shiftFocus(int index,  bool(*filter)(WindowInfo*) = NULL, ArrayList<WindowInfo*>& stack = getActiveWindowStack());


/**
 * Shifts the focused window to the top of the window& stack if it isn't already
 * If it is already at the top, then shifts the next window to the top and focuses it
 * @param stack
 */
void shiftTopAndFocus(ArrayList<WindowInfo*>& stack = getActiveWindowStack());



/**
 * activates the workspace that contains the mouse pointer
 * @see getLastKnownMasterPosition()
 */
void activateWorkspaceUnderMouse(void);
/**
 * Moves the active pointer so that is is the center of winInfo
 *
 * @param winInfo
 */
void centerMouseInWindow(WindowInfo* winInfo);

/**
 * switches to an adjacent workspace
 *
 * @param dir either UP or DOWN
 */
static inline void swapWorkspace(int dir) {
    switchToWorkspace(getAllWorkspaces().getNextIndex(getActiveWorkspaceIndex(), dir));
}

/**
 * Swaps monitors with an adjacent workspace
 *
 * @param dir either UP or DOWN
 */
static inline void shiftWorkspace(int dir) {
    swapMonitors(getActiveWorkspaceIndex(), (getAllWorkspaces().getNextIndex(getActiveWorkspaceIndex(), dir)));
}

static inline bool matchesFocusedWindowClass(WindowInfo* winInfo) {
    return matchesClass(winInfo, getFocusedWindow()->getClassName());
}
#endif
