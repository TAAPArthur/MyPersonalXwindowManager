/**
 * @file windows.c
 * @copybrief windows.h
 *
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <X11/extensions/XInput2.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xinput.h>


#include "bindings.h"
#include "devices.h"
#include "events.h"
#include "ewmh.h"
#include "globals.h"
#include "logger.h"
#include "masters.h"
#include "monitors.h"
#include "mywm-util.h"
#include "windows.h"
#include "workspaces.h"
#include "xsession.h"


///list of all windows
static ArrayList windows;
ArrayList* getAllWindows(void){
    return &windows;
}
WindowInfo* createWindowInfo(WindowID id){
    WindowInfo* wInfo = calloc(1, sizeof(WindowInfo));
    wInfo->id = id;
    wInfo->workspaceIndex = NO_WORKSPACE;
    return wInfo;
}
int getMask(WindowInfo* winInfo){
    return winInfo->mask;
}
int hasPartOfMask(WindowInfo* winInfo, int mask){
    return (winInfo->mask & mask);
}
int hasMask(WindowInfo* winInfo, int mask){
    return hasPartOfMask(winInfo, mask) == mask;
}
void resetUserMask(WindowInfo* winInfo){
    winInfo->mask &= ~USER_MASKS;
}
int getUserMask(WindowInfo* winInfo){
    return getMask(winInfo)&USER_MASKS;
}
///return true iff mask as any USER_MASK bits set
int isUserMask(int mask){
    return mask & USER_MASKS ? 1 : 0;
}

void toggleMask(WindowInfo* winInfo, int mask){
    if(hasMask(winInfo, mask))
        removeMask(winInfo, mask);
    else
        addMask(winInfo, mask);
}
void addMask(WindowInfo* winInfo, int mask){
    winInfo->mask |= mask;
    if(SYNC_WINDOW_MASKS && isUserMask(mask))
        setXWindowStateFromMask(winInfo);
}
void removeMask(WindowInfo* winInfo, int mask){
    winInfo->mask &= ~mask;
    if(SYNC_WINDOW_MASKS && isUserMask(mask))
        setXWindowStateFromMask(winInfo);
}

void loadClassInfo(WindowInfo* info){
    WindowID win = info->id;
    xcb_icccm_get_wm_class_reply_t prop;
    xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_class(dis, win);
    if(!xcb_icccm_get_wm_class_reply(dis, cookie, &prop, NULL))
        return;
    if(info->className)
        free(info->className);
    if(info->instanceName)
        free(info->instanceName);
    info->className = calloc(1 + (strlen(prop.class_name)), sizeof(char));
    info->instanceName = calloc(1 + (strlen(prop.instance_name)), sizeof(char));
    strcpy(info->className, prop.class_name);
    strcpy(info->instanceName, prop.instance_name);
    xcb_icccm_get_wm_class_reply_wipe(&prop);
    LOG(LOG_LEVEL_VERBOSE, "class name %s instance name: %s \n", info->className, info->instanceName);
}
void loadTitleInfo(WindowInfo* winInfo){
    xcb_ewmh_get_utf8_strings_reply_t wtitle;
    xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_name(ewmh, winInfo->id);
    if(winInfo->title){
        free(winInfo->title);
        winInfo->title = NULL;
    }
    if(xcb_ewmh_get_wm_name_reply(ewmh, cookie, &wtitle, NULL)){
        winInfo->title = calloc(1 + wtitle.strings_len, sizeof(char));
        memcpy(winInfo->title, wtitle.strings, wtitle.strings_len * sizeof(char));
        xcb_ewmh_get_utf8_strings_reply_wipe(&wtitle);
    }
    else {
        xcb_icccm_get_text_property_reply_t icccmName;
        cookie = xcb_icccm_get_wm_name(dis, winInfo->id);
        if(xcb_icccm_get_wm_name_reply(dis, cookie, &icccmName, NULL)){
            winInfo->title = calloc(1 + icccmName.name_len, sizeof(char));
            memcpy(winInfo->title, icccmName.name, icccmName.name_len * sizeof(char));
            xcb_icccm_get_text_property_reply_wipe(&icccmName);
        }
    }
    if(winInfo->title)
        LOG(LOG_LEVEL_VERBOSE, "window title %s\n", winInfo->title);
}
void loadWindowType(WindowInfo* winInfo){
    xcb_ewmh_get_atoms_reply_t name;
    int foundType = 0;
    for(int i = 0; i < 10; i++)
        if(xcb_ewmh_get_wm_window_type_reply(ewmh,
                                             xcb_ewmh_get_wm_window_type(ewmh, winInfo->id), &name, NULL)){
            winInfo->type = name.atoms[0];
            xcb_ewmh_get_atoms_reply_wipe(&name);
            foundType = 1;
            break;
        }
        else msleep(10);
    if(!foundType){
        winInfo->implicitType = 1;
        winInfo->type = winInfo->transientFor ? ewmh->_NET_WM_WINDOW_TYPE_DIALOG : ewmh->_NET_WM_WINDOW_TYPE_NORMAL;
    }
    xcb_get_atom_name_reply_t* reply = xcb_get_atom_name_reply(
                                           dis, xcb_get_atom_name(dis, winInfo->type), NULL);
    if(!reply)return;
    if(winInfo->typeName)
        free(winInfo->typeName);
    winInfo->typeName = calloc(reply->name_len + 1, sizeof(char));
    memcpy(winInfo->typeName, xcb_get_atom_name_name(reply), reply->name_len * sizeof(char));
    LOG(LOG_LEVEL_VERBOSE, "window type %s\n", winInfo->typeName);
    free(reply);
}
void loadWindowHints(WindowInfo* winInfo){
    assert(winInfo);
    xcb_icccm_wm_hints_t hints;
    int inputFocus = 0;
    if(xcb_icccm_get_wm_hints_reply(dis, xcb_icccm_get_wm_hints(dis, winInfo->id), &hints, NULL)){
        winInfo->groupId = hints.window_group;
        inputFocus = hints.input;
        if(hints.initial_state == XCB_ICCCM_WM_STATE_NORMAL)
            addMask(winInfo, MAPPABLE_MASK);
    }
    else if(winInfo->type == ewmh->_NET_WM_WINDOW_TYPE_NORMAL)
        inputFocus = 1;
    if(inputFocus)
        addMask(winInfo, INPUT_MASK);
    else
        removeMask(winInfo, INPUT_MASK);
}

void loadProtocols(WindowInfo* winInfo){
    xcb_icccm_get_wm_protocols_reply_t reply;
    if(xcb_icccm_get_wm_protocols_reply(dis,
                                        xcb_icccm_get_wm_protocols(dis, winInfo->id, ewmh->WM_PROTOCOLS),
                                        &reply, NULL)){
        for(int i = 0; i < reply.atoms_len; i++)
            if(reply.atoms[i] == WM_DELETE_WINDOW)
                addMask(winInfo, WM_DELETE_WINDOW_MASK);
            else if(reply.atoms[i] == ewmh->_NET_WM_PING)
                addMask(winInfo, WM_PING_MASK);
        //else if(reply.atoms[i]==WM_TAKE_FOCUS)
        //addMask(winInfo, WM_TAKE_FOCUS_MASK);
        xcb_icccm_get_wm_protocols_reply_wipe(&reply);
    }
}
void loadWindowProperties(WindowInfo* winInfo){
    LOG(LOG_LEVEL_VERBOSE, "loading window properties %d\n", winInfo->id);
    loadClassInfo(winInfo);
    loadTitleInfo(winInfo);
    loadProtocols(winInfo);
    xcb_window_t prop;
    if(xcb_icccm_get_wm_transient_for_reply(dis,
                                            xcb_icccm_get_wm_transient_for(dis, winInfo->id), &prop, NULL)){
        winInfo->transientFor = prop;
    }
    loadWindowType(winInfo);
    loadWindowHints(winInfo);
    applyRules(getEventRules(PropertyLoad), winInfo);
}
void loadSavedAtomState(WindowInfo* winInfo){
    xcb_ewmh_get_atoms_reply_t reply;
    if(xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, winInfo->id), &reply, NULL)){
        setWindowStateFromAtomInfo(winInfo, reply.atoms, reply.atoms_len, XCB_EWMH_WM_STATE_ADD);
        xcb_ewmh_get_atoms_reply_wipe(&reply);
    }
}
void setXWindowStateFromMask(WindowInfo* winInfo){
    assert(winInfo);
    xcb_atom_t supportedStates[] = {SUPPORTED_STATES};
    xcb_ewmh_get_atoms_reply_t reply;
    int count = 0;
    int hasState = xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, winInfo->id), &reply, NULL);
    xcb_atom_t windowState[LEN(supportedStates) + (hasState ? reply.atoms_len : 0)];
    if(hasState){
        for(int i = 0; i < reply.atoms_len; i++){
            char isSupportedState = 0;
            for(int n = 0; n < LEN(supportedStates); n++)
                if(supportedStates[n] == reply.atoms[i]){
                    isSupportedState = 1;
                    break;
                }
            if(!isSupportedState)
                windowState[count++] = reply.atoms[i];
        }
        xcb_ewmh_get_atoms_reply_wipe(&reply);
    }
    if(hasMask(winInfo, STICKY_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_STICKY;
    if(hasMask(winInfo, HIDDEN_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_HIDDEN;
    if(hasMask(winInfo, Y_MAXIMIZED_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    if(hasMask(winInfo, X_MAXIMIZED_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    if(hasMask(winInfo, URGENT_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    if(hasMask(winInfo, FULLSCREEN_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_FULLSCREEN;
    if(hasMask(winInfo, ROOT_FULLSCREEN_MASK))
        windowState[count++] = WM_STATE_ROOT_FULLSCREEN;
    if(hasMask(winInfo, NO_TILE_MASK))
        windowState[count++] = WM_STATE_NO_TILE;
    if(hasMask(winInfo, ABOVE_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_ABOVE;
    if(hasMask(winInfo, BELOW_MASK))
        windowState[count++] = ewmh->_NET_WM_STATE_BELOW;
    xcb_ewmh_set_wm_state(ewmh, winInfo->id, count, windowState);
}

void setWindowStateFromAtomInfo(WindowInfo* winInfo, const xcb_atom_t* atoms, int numberOfAtoms, int action){
    if(!winInfo)
        return;
    LOG(LOG_LEVEL_TRACE, "Updating state of %d from %d atoms\n", winInfo->id, numberOfAtoms);
    int mask = 0;
    for(unsigned int i = 0; i < numberOfAtoms; i++){
        if(atoms[i] == ewmh->_NET_WM_STATE_STICKY)
            mask |= STICKY_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_HIDDEN)
            mask |= HIDDEN_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_MAXIMIZED_VERT)
            mask |= Y_MAXIMIZED_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ)
            mask |= X_MAXIMIZED_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
            mask |= URGENT_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_FULLSCREEN)
            mask |= FULLSCREEN_MASK;
        else if(atoms[i] == WM_STATE_ROOT_FULLSCREEN)
            mask |= ROOT_FULLSCREEN_MASK;
        else if(atoms[i] == WM_STATE_NO_TILE)
            mask |= NO_TILE_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_ABOVE)
            mask |= ABOVE_MASK;
        else if(atoms[i] == ewmh->_NET_WM_STATE_BELOW)
            mask |= BELOW_MASK;
        else if(atoms[i] == WM_TAKE_FOCUS)
            mask |= WM_TAKE_FOCUS_MASK;
        else if(atoms[i] == WM_DELETE_WINDOW)
            mask |= WM_DELETE_WINDOW_MASK;
    }
    if(action == XCB_EWMH_WM_STATE_TOGGLE){
        //if has masks and (atoms don't affect layer it causes it to be places in the same layer)
        if(hasMask(winInfo, mask))
            action = XCB_EWMH_WM_STATE_REMOVE;
        else
            action = XCB_EWMH_WM_STATE_ADD;
    }
    LOG(LOG_LEVEL_DEBUG, "Add %d mask %d\n", action == XCB_EWMH_WM_STATE_ADD, mask);
    if(action == XCB_EWMH_WM_STATE_REMOVE)
        removeMask(winInfo, mask);
    else
        addMask(winInfo, mask);
}
short* getConfig(WindowInfo* winInfo){
    return winInfo->config;
}
void setConfig(WindowInfo* winInfo, short* config){
    memcpy(winInfo->config, config, sizeof(winInfo->config));
}
short* getGeometry(WindowInfo* winInfo){
    return winInfo->geometry;
}
void setGeometry(WindowInfo* winInfo, short* geometry){
    if(winInfo && winInfo->geometrySemaphore == 0)
        memcpy(winInfo->geometry, geometry, LEN(winInfo->geometry)*sizeof(short));
}
void lockWindow(WindowInfo* winInfo){
    winInfo->geometrySemaphore++;
}
void unlockWindow(WindowInfo* winInfo){
    winInfo->geometrySemaphore--;
}

void registerWindow(WindowInfo* winInfo){
    LOG(LOG_LEVEL_DEBUG, "Registering %d (%x)\n", winInfo->id, winInfo->id);
    assert(winInfo);
    assert(!find(getAllWindows(), winInfo, sizeof(int)) && "Window registered exists");
    addWindowInfo(winInfo);
    Window win = winInfo->id;
    assert(win);
    if(LOAD_SAVED_STATE){
        loadSavedAtomState(winInfo);
    }
    if(registerForWindowEvents(win, NON_ROOT_EVENT_MASKS) != BadWindow){
        passiveGrab(win, NON_ROOT_DEVICE_EVENT_MASKS);
        applyRules(getEventRules(RegisteringWindow), winInfo);
    }
    else {
        LOG(LOG_LEVEL_TRACE, "Registeration failed %d (%x)\n", winInfo->id, winInfo->id);
        removeWindow(winInfo->id);
    }
}
int processNewWindow(WindowInfo* winInfo){
    LOG(LOG_LEVEL_TRACE, "processing %d (%x)\n", winInfo->id, winInfo->id);
    addMask(winInfo, DEFAULT_WINDOW_MASKS);
    if(winInfo->cloneOrigin == 0)
        loadWindowProperties(winInfo);
    if(!applyRules(getEventRules(ProcessingWindow), winInfo)){
        LOG(LOG_LEVEL_VERBOSE, "Window is to be ignored; freeing winInfo %d\n", winInfo->id);
        deleteWindowInfo(winInfo);
        return 0;
    }
    registerWindow(winInfo);
    return 1;
}
void scan(xcb_window_t baseWindow){
    LOG(LOG_LEVEL_TRACE, "Scanning children of %d\n", baseWindow);
    xcb_query_tree_reply_t* reply;
    reply = xcb_query_tree_reply(dis, xcb_query_tree(dis, baseWindow), 0);
    assert(reply && "could not query tree");
    if(reply){
        int numberOfChildren = xcb_query_tree_children_length(reply);
        LOG(LOG_LEVEL_TRACE, "detected %d kids\n", numberOfChildren);
        xcb_window_t* children = xcb_query_tree_children(reply);
        xcb_get_window_attributes_reply_t* attr;
        xcb_get_window_attributes_cookie_t cookies[numberOfChildren];
        for(int i = 0; i < numberOfChildren; i++)
            cookies[i] = xcb_get_window_attributes(dis, children[i]);
        // iterate in top to bottom order
        for(int i = numberOfChildren - 1; i >= 0; i--){
            LOG(LOG_LEVEL_TRACE, "processing child %d\n", children[i]);
            attr = xcb_get_window_attributes_reply(dis, cookies[i], NULL);
            assert(attr);
            if(attr->override_redirect || attr->_class == XCB_WINDOW_CLASS_INPUT_ONLY){
                LOG(LOG_LEVEL_VERBOSE, "Skipping child override redirect: %d class: %d\n", attr->override_redirect, attr->_class);
            }
            else {
                WindowInfo* winInfo = createWindowInfo(children[i]);
                winInfo->parent = baseWindow;
                //if the window is not unmapped
                if(attr->map_state)
                    addMask(winInfo, MAPPABLE_MASK | MAPPED_MASK);
                if(processNewWindow(winInfo) && !IGNORE_SUBWINDOWS)
                    scan(children[i]);
            }
            free(attr);
        }
        free(reply);
    }
}


void markAsDock(WindowInfo* winInfo){
    winInfo->dock = 1;
    if(indexOf(getAllWindows(), winInfo, sizeof(int)) != -1){
        addUnique(getAllDocks(), winInfo, sizeof(int));
        resizeAllMonitorsToAvoidStruct(winInfo);
    }
}

int addWindowInfo(WindowInfo* winInfo){
    if(addUnique(getAllWindows(), winInfo, sizeof(int))){
        if(winInfo->dock){
            addUnique(getAllDocks(), winInfo, sizeof(int));
            resizeAllMonitorsToAvoidStruct(winInfo);
        }
        return 1;
    }
    return 0;
}
void deleteWindowInfo(WindowInfo* winInfo){
    clearList(getClonesOfWindow(winInfo));
    char** fields = &winInfo->typeName;
    for(int i = 0; i < 4; i++)
        if(fields[i])
            free(fields[i]);
    free(winInfo);
}
int removeWindow(WindowID winToRemove){
    assert(winToRemove != 0);
    int index = indexOf(getAllWindows(), &winToRemove, sizeof(int));
    if(index == -1)
        return 0;
    WindowInfo* winInfo = getElement(getAllWindows(), index);
    FOR_EACH(Master*, master, getAllMasters()) removeWindowFromMaster(master, winToRemove);
    removeWindowFromWorkspace(winInfo);
    if(winInfo->dock){
        int index = indexOf(getAllDocks(), &winToRemove, sizeof(int));
        assert(index != -1);
        removeFromList(getAllDocks(), index);
        resizeAllMonitorsToAvoidAllStructs();
    }
    deleteWindowInfo(winInfo);
    removeFromList(getAllWindows(), index);
    return 1;
}
WindowInfo* getWindowInfo(WindowID win){
    int index = indexOf(getAllWindows(), &win, sizeof(int));
    return index == -1 ? NULL : getElement(getAllWindows(), index);
}
ArrayList* getClonesOfWindow(WindowInfo* winInfo){
    return &winInfo->cloneList;
}
void onWindowFocus(WindowID win){
    int index = indexOf(getAllWindows(), &win, sizeof(int));
    if(index == -1)return;
    WindowInfo* winInfo = getElement(getAllWindows(), index);
    int pos = indexOf(getActiveMasterWindowStack(), winInfo, sizeof(int));
    if(! isFocusStackFrozen()){
        if(pos == -1){
            addToList(getActiveMasterWindowStack(), winInfo);
            pos = getSize(getActiveMasterWindowStack()) - 1;
        }
        shiftToHead(getActiveMasterWindowStack(), pos);
    }
    else if(pos != -1){
        getActiveMaster()->focusedWindowIndex = pos;
    }
    getActiveMaster()->focusedTimeStamp = getTime();
}
