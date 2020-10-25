/**
 * @file masters.h
 * Contains methods related to the Master struct
 */
#ifndef MASTERS_H
#define MASTERS_H

#include "util/arraylist.h"
#include "globals.h"
#include "mywm-structs.h"
#include "slaves.h"

/// will match any mode
#define ANY_MODE ((uint32_t)-1)

/**
 * Creates a new master device and adds it to the global list
 *
 * @param pointerID
 * @param keyboardID
 * @param name
 *
 * @return
 */
Master* newMaster(MasterID pointerID, MasterID keyboardID, const char* name);

/**
 * Destorys a master and all of its resources
 *
 * @param master
 */
void freeMaster(Master* master);
/**
 *
 * @return a list of all master devices
 */
const ArrayList* getAllMasters(void);
/**
 *
 * @return The master device currently interacting with the wm
 * @see setActiveMasterNodeByID
 */
Master* getActiveMaster(void);
/**
 * The active master should be set whenever the user interacts with the
 * wm (key/mouse binding, mouse press etc)
 * @param master the new active master
 */
void setActiveMaster(Master* master);

struct Binding;
/// holds data on a master device pair like the ids and focus history
typedef struct Master {
    /// id of a master keyboard device
    const uint32_t id;
    /// pointer master id associated with id
    const MasterID pointerID;
    /// the name of the master keyboard
    char name[MAX_NAME_LEN];
    /// the color windows when this device has the most recent focus
    uint32_t focusColor;
    /**Stack of windows in order of most recently focused*/
    ArrayList slaves;

    /**Stack of windows in order of most recently focused*/
    ArrayList windowStack;
    /**
     * Contains the window with current focus,
     * will be same as top of window stack if freezeFocusStack==0
     *
     */
    uint32_t focusedWindowIndex;

    /**Time the focused window changed*/
    TimeStamp focusedTimeStamp;
    /// The current binding mode
    uint8_t bindingMode;
    /**When true, the focus stack won't be updated on focus change*/
    bool freezeFocusStack;



    SlaveID lastActiveSlave;
    void* windowMoveResizer;

    /// Index of active workspace;
    WorkspaceID activeWorkspaceIndex;
    ArrayList bindings;
} Master;

static inline const ArrayList* getSlaves(Master* master) {
    return &master->slaves;
}

static inline const ArrayList* getMasterWindowStack(Master* master) {
    return &master->windowStack;
}
static inline const ArrayList* getActiveMasterWindowStack() { return getMasterWindowStack(getActiveMaster());}

/**
 * Creates and adds the default master if it doesn't already exits
 */
void addDefaultMaster();
/// @return the keyboard id of the active master
static inline MasterID getActiveMasterKeyboardID(void) {return getActiveMaster()->id;}
/// @return the pointer id of the active master
static inline MasterID getActiveMasterPointerID(void) {return getActiveMaster()->pointerID;}
static inline MasterID getPointerID(Master* master) {return master->pointerID;}
static inline MasterID getKeyboardID(Master* master) {return master->id;}
/// @return the ID of the active Workspace or NO_WORKSPACE
static inline WorkspaceID getActiveWorkspaceIndex(void) {return getActiveMaster()->activeWorkspaceIndex ;}
static inline WorkspaceID getMasterWorkspaceIndex(Master* master) {return master->activeWorkspaceIndex ;}
/**
 * Sets the active workspace index
 * @param index
 */
static inline void setActiveWorkspaceIndex(WorkspaceID index) {
    getActiveMaster()->activeWorkspaceIndex = index;
}

/**
 * @param name
 * @return the master with the given name
 */
Master* getMasterByName(const char* name) ;
/**
 * @brief returns the master node with id == keyboardID
 * @param id id of the master device
 * @param pointer whether to look for a pointer or keyboard device
 * @return the master device with the give node
 */
Master* getMasterByID(MasterID id);

/**
 * @return whether of not the master window stack will be updated on focus change
*/
static inline int isFocusStackFrozen(void) {
    return getActiveMaster()->freezeFocusStack;
}

/**
 * If value, the master window stack will not be updated on focus change
 * Else the focused window will be shifted to the top of the master stack,
 * the master stack and the focused will remain in sync after all focus changes
 * @param value whether the focus stack is frozen or not
*/
void setFocusStackFrozen(int value);
/**
 * Only bindings whose mode == this->mode || mode == ANY_MODE will be triggered by this master
 *
 * @param mode
 * @see allowsMode
 */
static inline void setActiveMode(int mode) {getActiveMaster()->bindingMode = mode;}
/**
 * Returns the current binding mode for the active master
 *
 * @return
 */
static inline uint32_t getActiveMode(void) {
    return getActiveMaster()->bindingMode;
}
/**
 * To be called when a master focuses a given window.
 *
 * If the window is not in the window list, nothing is done
 * Else if the window is not in the master's window stack, it is added to the end.
 * Else if the master is not frozen, the window is shifted to the end of the stack
 *
 * In all but the first case, a call to getFocusedWindow() will return the window represented by win
 *
 * @param win
 */
void onWindowFocus(WindowID win);

void onWindowFocusForMaster(WindowID win, Master* master);
/**
 * Clears the focus stack
 */
void clearFocusStack(Master* master);
/**
 * Removes a given window from the focus stack
 *
 * @param win the window to remove
 *
 * @return the window removed or NULL
 */
WindowInfo* removeWindowFromFocusStack(Master* master, WindowID win);

/**
 * Get the WindowInfo representing the window the master is
 * currently focused on.
 * This value will be should be updated whenever the focus for the active
 * master changes. If the focused window is deleted, then the this value is
 * set the next window if the master focus stack
 * @return the currently focused window for the active master
 */
WindowInfo* getFocusedWindow(void);
WindowInfo* getFocusedWindowOfMaster(Master* master);

/**
 * If id, then this slave is associated with the master with this id.
 * Else this slave is disassociated with its current master.
 * @param slave
 * @param master of the new master or 0
 */
void setMasterForSlave(Slave* slave, MasterID master);
/**
 * @return the master associated with this slave or NULL
 */
Master* getMasterForSlave(Slave* slave);

#endif
