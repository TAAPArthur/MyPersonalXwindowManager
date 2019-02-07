/**
 * @file mywm-util.h
 *
 * @brief Calls to these function do not need a corresponding X11 function
 *
 *
 *
 * In general when creating an instance to a struct all values are set
 * to their defaults unless otherwise specified.
 *
 * The default for Node* is an empty node (multiple values in a struct may point to the same empty node)
 * The default for points is NULL
 * everything else 0
 *
 *
 */
#ifndef MYWM_XUTIL
#define MYWM_XUTIL

#include "util.h"

///Fixed size of all struct fields the keep an array of arbitary sized names
#ifndef NAME_BUFFER
    #define NAME_BUFFER 32
#endif

///Types of docks
typedef enum {DOCK_LEFT,DOCK_RIGHT,DOCK_TOP,DOCK_BOTTOM}DockTypes;

///Maximum value the property field in WindowInfo can have
#define WINDOW_STRUCT_ARRAY_SIZE    12
///holds data on a window
typedef struct window_info{
    /**Window id */
    unsigned int id;
    /**Window mask */
    unsigned int mask;
    /// set to 1 iff the window is a dock 
    int dock;
    /**xcb_atom representing the window type*/
    int type;
    /**string xcb_atom representing the window type*/
    char*typeName;
    /**class name of window*/
    char*className;
    /** instance name of window*/
    char*instanceName;
    /**title of window*/
    char*title;

    /**1 iff the window type was not explicilty set*/
    char implicitType;
    /**
     * Used to indicate the "last seen in" workspace.
     * A window should only be tiled if the this field matches
     * the tiling workspace's index
     *
     * Note that his field may not contain all the workspaces an window belongs to
     */
    int workspaceIndex;

    /**The time this window was minimized*/
    int minimizedTimeStamp;
    ///time the window was last pinged
    int pingTimeStamp;

    ///counter for how many clients have requested the this window to be locked
    /// when this field is zero, the the geometry field is allowed to be updated in response to XEvents
    char geometrySemaphore;
    ///Window gravity
    char gravity;
    /** Set to override tiling */
    short config[5];
    /** The last know size of the window */
    short geometry[5];

    /**id of src window iff window is a clone*/
    int cloneOrigin;
    /**List of clones*/
    Node*cloneList;
    ///x,y offset for cloned windows; they will clone the original starting at this offset
    int cloneOffset[2];
    /**
     * The window this window is a transient for
     */
    unsigned int transientFor;
    /**
     * Window id of related window
     */
    unsigned int groupId;
    /**
     * Indicator of the masks an event has. This will be bitwise added
     * to the NON_ROOT_DEFAULT_WINDOW_MASKS when processing a new window
     */
    int eventMasks;

    /**The dock is only applied to the primary monitor*/
    char onlyOnPrimary;
    /**Special properties the window may have
     * Like the reserved space on a monitor or an offset
     * for clone windows
     * */
    int properties[WINDOW_STRUCT_ARRAY_SIZE];
} WindowInfo;

struct binding_struct;
///holds data on a master device pair like the ids and focus history
typedef struct{
    /**keyboard master id;*/
    int id;
    /**pointer master id associated with id;*/
    int pointerId;
    /// the name of the master keyboard
    char name[NAME_BUFFER];
    /// the color windows when this device has the most recent focus
    unsigned int focusColor;

    /**Stack of windows in order of most recently focused*/
    Node* windowStack;
    /**Contains the window with current focus,
     * will be same as top of window stack if freezeFocusStack==0
     * */
    Node* focusedWindow;

    /**Time the focused window changed*/
    unsigned int focusedTimeStamp;
    /**Pointer to last binding triggered by this master device*/
    struct binding_struct* lastBindingTriggered;
    /**List of the active chains with the newest first*/
    Node*activeChains;
    /**When true, the focus stack won't be updated on focus change*/
    int freezeFocusStack;

    /** pointer to head of node list where the values point to WindowInfo */
    Node* windowsToIgnore;

    /**Index of active workspace; this workspace will be used for
     * all workspace operations if a workspace is not specified
     * */
    int activeWorkspaceIndex;
    int targetWindow;

    ///Starting point for operations like resizing the window with mouse
    short prevMousePos[2];
    ///Last recorded mouse position for a device event
    short currentMousePos[2];
    /**
     * stack of visited workspaces with most recent at top
     */
    Node*workspaceHistory;

} Master;

typedef struct Monitor Monitor;

typedef struct Layout Layout;

///metadata on Workspace
typedef struct{
    ///workspace index
    int id;
    ///user facing name of the workspace
    char*name;
    ///the monitor the workspace is on
    Monitor*monitor;


    /// is hiding all windows
    char showingDesktop;

    ///an windows stack
    Node*windows;

    ///the currently applied layout; does not have to be in layouts
    Layout* activeLayout;
    ///a circular list of layouts to cycle through
    Node*layouts;
}Workspace;




/**
 * Initializes a context object to default values.
 *
 * All Node points are set to point to an empty node.
 * Workspaces is set to point to the head of an array with size==
 * numberOfWorkspaces
 *
 * If a context already exists, it is first destroyed
 * @param numberOfWorkspaces    the number of workspaces
 */
void createContext(int numberOfWorkspaces);

/**
 * Destroys the resources related to our internal representation.
 * It does nothing if resources have already been cleared or never initally created
 */
void destroyContext();

/**
 * Creates a pointer to a WindowInfo object and sets its id to id
 *
 * The workspaceIndex of the window is set to NO_WORKSPACE
 * @param id    the id of the WindowInfo object
 * @return  pointer to the newly created object
 */
WindowInfo* createWindowInfo(unsigned int id);

/**
 * Clones a WindowInfo object and sets its id to id
 *
 * @param id    the id of the WindowInfo object
 * @param winInfo    the window to clone
 * @return  pointer to the newly created object
 */
WindowInfo*cloneWindowInfo(unsigned int id,WindowInfo*winInfo);

/**
 *
 * @return the number of workspaces
 * @see createContext
 */
int getNumberOfWorkspaces();

/**
 *
 * @return a list of all master devices
 */
Node* getAllMasters();
/**
 * Creates and adds a given master with id = keyboardMasterId
 * if a master with this id does not already exist to the head of the
 * master list.
 * Note the new master does not replace the active master unless there
 * the master list is empty
 * @param keyboardMasterId  the id of the master device.
 * @param pointerMasterId  the id of the associated master pointer
 * @param name the name of the master keyboard
 * @param focusColor
 * @return 1 iff a new master was inserted; 0 o.w.
 */
int addMaster(unsigned int keyboardMasterId,unsigned int pointerMasterId,char*name,int focusColor);

/**
 * Removes the master with the specifed id
 * @param id    the id to remove
 * @return 1 iff a node was removed 0 o.w.
 */
int removeMaster(unsigned int id);
/**
 * Removes a window from the master stack
 * @param master
 * @param winToRemove
 * @return 1 iff the window was actually removed
 */
int removeWindowFromMaster(Master*master,int winToRemove);

/**
 * @return the id of the active master
 */
int getActiveMasterKeyboardID();
/**
 * @return the pointer id of the active master
 */
int getActiveMasterPointerID();
/**
 * @return  whether of not the master window stack will be updated on focus change
*/
int isFocusStackFrozen();
/**
 * If value, the master window stack will not be updated on focus change
 * Else the the focused window will be shifted to the top of the master stack,
 * the master stack and the focused will remain in sync after all focus changes
 * @param value whether the focus stack is frozen or not
*/
void setFocusStackFrozen(int value);

/**
 *
 * @return Returns a list of windows that have been put in the cache
*/
Node*getWindowCache();
/**
 * Clears the window cache for the active master
 */
void clearWindowCache();
/**
 * Adds a window to the cache if not already present
 * @param winInfo  the window to add
 */
int updateWindowCache(WindowInfo*winInfo);

/**
 * Returns the top window in the stack relative to given master.
 * @return
 */
Node* getActiveMasterWindowStack(void);


/**
 * Returns the top window in the stack relative to given master.
 * @return
 */
Node* getWindowStackByMaster(Master*master);

/**
 * Get next/prev windows in master's stacking order
 * @param dir wheter to get teh next (>0) or the prev (<0) window
 * @return the next or previous window depending on dir
 */
Node* getNextWindowInFocusStack(int dir);

/**
 * Get the node containing the window master is
 * currently focused on. This should never be null
 * @param master
 * @return the currently focused window for master
 */
Node* getFocusedWindowByMaster(Master*master);
/**
 * Get the WindowInfo representing the window the master is
 * currently focused on.
 * This value will be should be updated whenever the focus for the active
 * master changes. If the focused window is deleted, then the this value is
 * set the next window if the master focus stack
 * @return the currently focused window for the active master
 */
WindowInfo* getFocusedWindow();
/**
 *
 * @return the node in the master window list representing the focused window
 */
Node* getFocusedWindowNode();
/**
 *
 * @param m
 * @return the time the master focused the window
 */
unsigned getFocusedTime(Master*m);

/**
 * Returns a struct with stored metadata on the given window
 * @param win
 * @return pointer to struct with info on the given window
 */
WindowInfo* getWindowInfo(unsigned int win);


/**
 * @return a list of windows with the most recently used windows first
 */
Node*getAllWindows();

/**
 *
 * @return list of docks
 */
Node*getAllDocks(void);



/**
 * To be called when a master focuses a given window.
 *
 * If the window is not in the window list, nothing is done
 * else
 * The window is added to the active master's stack
 * if the master is not frozen, then the window is added/shifted to the head of the master stack
 * The window is shifted the head of the global window list
 * The master is promoted to the head of the master stack
 *
 * Note that if a new window is focused, it won't be added to the master stack if it is
 * frozen
 * @param win
 */
void onWindowFocus(unsigned int win);


/**
 *
 * @return The master device currently interacting with the wm
 * @see setActiveMasterNodeById
 */
Master*getActiveMaster(void);
/**
 * The active master should be set whenever the user interacts with the
 * wm (key/mouse  binding, mouse press etc)
 * @param master the new active master
 */
void setActiveMaster(Master*master);
/**
 * @brief returns the master node with id == keyboardId
 * @param keyboardID id of the master device
 * @return the master device with the give node
 */
Master*getMasterById(int keyboardID);
/**
 * Return the first non-special workspace that a window is in
 * starting from the least recently focused window
 * @return the active workspace
 */
Workspace* getActiveWorkspace(void);


/**
 *
 * @return the active workspace index
 */
int getActiveWorkspaceIndex(void);
/**
 * Sets the active workspace index
 * @param index
 */
void setActiveWorkspaceIndex(int index);


/**
 * @param workspace
 * @return the windows stack of the workspace
 */
Node*getWindowStack(Workspace*workspace);



int getWorkspaceIndexOfWindow(WindowInfo*winInfo);
Workspace* getWorkspaceOfWindow(WindowInfo*winInfo);
Node* getWindowStackOfWindow(WindowInfo*winInfo);
/**
 *
 * @return the windows in the active workspace at the NORMAL_LAYER
 */
Node*getActiveWindowStack();


/**
 * @param index
 * @return the workspace at a given index
 */
Workspace* getWorkspaceByIndex(int index);

/**
 *
 * @param i the workspace index
 * @return 1 iff the workspace has been assigned a monitor
 */
int isWorkspaceVisible(int i);
/**
 *
 * @param index
 * @return if the workspace has at least one window explicitly assigned to it
 */
int isWorkspaceNotEmpty(int index);

///@see getNextWorkspace()
typedef enum{
    ///only return visible workspaces
    VISIBLE=1,
    ///only return invisible workspaces
    HIDDEN=2,
    ///only return non empty workspaces
    NON_EMPTY=1<<2,
    ///only return empty workspaces
    EMPTY=2<<2
}WorkSpaceFilter;
/**
 * Get the next workspace in the given direction according to the filters
 * @param dir the interval of workspaces to check
 * @param mask species a filter for workpspaces @see WorkSpaceFilter
 * @return the next workspace in the given direction that matches the criteria
 */
Workspace*getNextWorkspace(int dir,int mask);

/**
 * Returns the workspace currently displayed by the monitor or null

 * @param monitor
 * @return
 */
Workspace*getWorkspaceFromMonitor(Monitor*monitor);

/**
 * @param workspace
 * @return the monitor assoicated with the given workspace if any
 */
Monitor*getMonitorFromWorkspace(Workspace*workspace);

/**
 * Swaps the monitors assosiated with the given workspaces
 * @param index1
 * @param index2
 */
void swapMonitors(int index1,int index2);

/**
 * Get the master who most recently focused window.
 * Should be called to reset border focus after the active master's
 * focused has changed
 * @param win
 * @return
 */
Master* getLastMasterToFocusWindow(int win);


/**
 * Assgins a workspace to a monitor. The workspace is now considered visible
 * @param w
 * @param m
 */
void setMonitorForWorkspace(Workspace*w,Monitor*m);


/**
 *
 * @return list of all monitors
 */
Node*getAllMonitors();
void addMonitor(Monitor*m);

/**
 * The the currently used layout for the active workspace.
 * Note that this layout does not have to be in the list of layout for the active workspace
 * @param layout the new active layout
 */
void setActiveLayout(Layout*layout);

/**
 *
 * @return the active layout for the active workspace
 */
Layout* getActiveLayout();

/**
 * @param workspaceIndex
 * @return the active layout for the specified workspace
 */
Layout* getActiveLayoutOfWorkspace(int workspaceIndex);







/**
 * Creates a pointer to a Master instance and sets its id to id.
 * @param id keyboard id
 * @param pointerId pointer id
 * @param name the name of the master keyboard
 * @param focusColor
 * @return pointer to object
 */
Master *createMaster(int id,int pointerId,char*name,int focusColor);

/**
 * Init a an array of workspaces to default values.
 * @param numberOfWorkspaces total number of workspace to create
 * @return pointer to the first workspace
 */
Workspace*createWorkspaces(int numberOfWorkspaces);
/**
 * Does a simple seach to see if the window is in the workspace's stack
 * Does not handle advaned cases like cloning
 * @param winInfo
 * @param workspaceIndex
 * @return true if the window with the given id is the specifed workspace
 */
Node* isWindowInWorkspace(WindowInfo* winInfo,int workspaceIndex);
/**
 *
 * @param winInfo
 * @return 1 if the window is in a visible workspace
 * @see isWindowInWorkspace
 */
int isWindowInVisibleWorkspace(WindowInfo* winInfo);

/**
 * Removes window from all workspaces
 * @param winInfo
 */
void removeWindowFromAllWorkspaces(WindowInfo* winInfo);
/**
 * Removes a window from a workspace
 * @param winInfo
 * @param workspaceIndex
 * @return 1 iff the window was actually removed
 */
int removeWindowFromWorkspace(WindowInfo* winInfo,int workspaceIndex);
int removeWindowFromActiveWorkspace(WindowInfo* winInfo);
/**
 *
 * @param info
 * @param workspaceIndex
 * @return 1 iff the window was actually added
 * @see addWindowToWorkspaceAtLayer
 */
int addWindowToWorkspace(WindowInfo*info,int workspaceIndex);
int addWindowToActiveWorkspace(WindowInfo*winInfo);
/**
 * Removes window from context and master(s)/workspace lists.
 * The Nodes containing the window and the windows value are freeded also
 * @param winToRemove
 */
int removeWindow(unsigned int winToRemove);
/**
 * Frees winInfo
 *
*/
void deleteWindowInfo(WindowInfo*winInfo);
/**
 * Deletes the node from its list and frees the WindowInfo* value
 * @param winNode
 * @see removeWindow()
 */
void deleteWindowNode(Node*winNode);


/**
 * Adds a window to the list of windows in context iff it
 * isn't already in the list
 * Note that the creted window is not added to a master window list or
 * a workspace
 * @param wInfo    instance to add
 * @return 1 iff this pointer was added
 */
int addWindowInfo(WindowInfo* wInfo);
void markAsDock(WindowInfo*winInfo);

/**
 * Returns a list of clone windows for the given window
 * @param winInfo
 * @return
 */
Node* getClonesOfWindow(WindowInfo*winInfo);
int getLayer(WindowInfo*winInfo);


Node*getMonitors(void);
#endif
