/**
 * @file monitors.h
 */
#ifndef MONITORS_H_
#define MONITORS_H_

#include "mywm-util.h"

/**
 * Represents a rectangular region where workspaces will be tiled
 */
typedef struct Monitor{
    /**id for monitor*/
    long id;
    /**1 iff the monitor is the primary*/
    char primary;
    /**The unmodified size of the monitor*/
    ///@{
    short int x;
    short int y;
    short int width;
    short int height;
    ///@}
    /** The modified size of the monitor after docks are avoided */
    ///@{
    short int viewX;
    short int viewY;
    short int viewWidth;
    short int viewHeight;
    ///@}

} Monitor;

/**
 * True if the monitor is marked as primary
 * @param monitor
 * @return
 */
int isPrimary(Monitor*monitor);
/**
 *
 * @param id id of monitor TODO need to convert handle long to int conversion
 * @param primary   if the monitor the primary
 * @param geometry an array containing the x,y,width,height of the monitor
 * @return 1 iff a new monitor was added
 */
int updateMonitor(long id,int primary,short geometry[4]);
/**
 * Removes a monitor and frees related info
 * @param id identifier of the monitor
 * @return 1 iff the montior was removed
 */
int removeMonitor(unsigned long id);
/**
 * Resets the viewport to be the size of the rectangle
 * @param m
 */
void resetMonitor(Monitor*m);


/**
 * Add properties to winInfo that will be used to avoid docks
 * @param winInfo the window in question
 * @param numberofProperties number of properties
 * @param properties list of properties
 * @see xcb_ewmh_wm_strut_partial_t
 * @see xcb_ewmh_get_extents_reply_t
 * @see avoidStruct
 */
void setDockArea(WindowInfo* winInfo,int numberofProperties,int properties[WINDOW_STRUCT_ARRAY_SIZE]);

/**
 * Reads (one of) the struct property to loads the info into properties and
 * add a dock to the list of docks.
 * @param info the dock
 * @return true if properties were successfully loaded
 */
int loadDockProperties(WindowInfo* info);

/**
 * Query for all monitors
 */
void detectMonitors(void);

void avoidStruct(WindowInfo*info);
/**
 *
 * @param arg1 x,y,width,height
 * @param arg2 x,y,width,height
 * @return 1 iff the 2 specified rectangles intersect
 */
int intersects(short int arg1[4],short int arg2[4]);

/**
 * Resizes the monitor such that its viewport does not intersec the given dock
 * @param m
 * @param winInfo the dock to avoid
 * @return if the size was changed
 */
int resizeMonitorToAvoidStruct(Monitor*m,WindowInfo*winInfo);
/**
 * @see resizeMonitorToAvoidStruct
 */
void resizeAllMonitorsToAvoidAllStructs(void);
/**
 * @param winInfo the dock to avoid
 * @see resizeMonitorToAvoidStruct
 */
void resizeAllMonitorsToAvoidStruct(WindowInfo*winInfo);

/**
 *
 * @return the width of the root window
 */
int getRootWidth(void);
/**
 *
 * @return the height of the root window
 */
int getRootHeight(void);
#endif
