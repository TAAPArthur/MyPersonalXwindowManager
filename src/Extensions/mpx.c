#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "../boundfunction.h"
#include "../devices.h"
#include "../util/logger.h"
#include "../masters.h"
#include "../windows.h"
#include "../slaves.h"
#include "../util/string-array.h"
#include "../util/time.h"
#include "../xutil/test-functions.h"
#include "../xutil/xsession.h"
#include "mpx.h"

/**
 * Holds info to match a master devices with a set of slaves
 */
typedef struct {
    /// The name of the master device pair (w/o Keyboard or Pointer suffix)
    char masterName[MAX_NAME_LEN];
    char parentName[MAX_NAME_LEN];
    /// the focus color to give the master
    int focusColor;
    /// list of names of slaves
    StringJoiner slaveNames;
} MPXMasterInfo ;
static ArrayList masterInfoList;
static MPXMasterInfo* newMPXMasterInfo(const char* name, int focusColor) {
    MPXMasterInfo* info = calloc(sizeof(MPXMasterInfo), 1);
    info->focusColor = focusColor;
    strncpy(info->masterName, name, MAX_NAME_LEN - 1);
    addElement(&masterInfoList, info);
    return info;
}

MPXMasterInfo* getMasterInfoForMaster(Master* master) {
    FOR_EACH(MPXMasterInfo*, masterInfo, &masterInfoList) {
        if(master == getMasterByName(masterInfo->masterName))
            return masterInfo;
    }
    return newMPXMasterInfo(master->name, master->focusColor);
}
Master* getParentMaster() {
    MPXMasterInfo* masterInfo = getMasterInfoForMaster(getActiveMaster());
    return getMasterByName(masterInfo->parentName);
}
Master* getLastChildOfMaster() {
    FOR_EACH_R(MPXMasterInfo*, masterInfo, &masterInfoList) {
        if(strcmp(getActiveMaster()->name, masterInfo->parentName) == 0) {
            Master* m = getMasterByName(masterInfo->masterName);
            if(m)
                return m;
        }
    }
    return NULL;
}

Master* getChildOfMasterByActiveSlave() {
    Slave* slave = getSlaveByID(getActiveMaster()->lastActiveSlave);
    if(!slave)
        return NULL;
    FOR_EACH_R(MPXMasterInfo*, masterInfo, &masterInfoList) {
        if(isInBuffer(&masterInfo->slaveNames, slave->name)) {
            Master* m = getMasterByName(masterInfo->masterName);
            if(m && m != getActiveMaster())
                return m;
        }
    }
    TRACE("Could not find child master owning slave %d", getActiveMaster()->lastActiveSlave);
    return NULL;
}

bool attachSlaveToProperMaster(Master* m) {
    bool found = 0;
    MPXMasterInfo* masterInfo = getMasterInfoForMaster(m);
    FOR_EACH(Slave*, slave, getAllSlaves()) {
        if(isInBuffer(&masterInfo->slaveNames, slave->name)) {
            attachSlaveToMaster(slave, m);
            found = 1;
        }
    }
    return found;
}
static inline void attachAllSlavesToMaster(Master* currentMaster, Master* newMaster) {
    FOR_EACH(Slave*, slave, getSlaves(currentMaster)) {
        attachSlaveToMaster(slave, newMaster);
    }
}
void attachActiveSlaveToLastChildOfMaster() {
    Slave* slave = getSlaveByID(getActiveMaster()->lastActiveSlave);
    Master* master = getLastChildOfMaster();
    if(master && slave)
        attachSlaveToMaster(slave, master);
}

void splitMaster(void) {
    INFO("Started splitting master");
    char name[32];
    sprintf(name, "dummy%d", getTime());
    createMasterDevice(name);
    initCurrentMasters();
    MPXMasterInfo* masterInfo = getMasterInfoForMaster(getMasterByName(name));
    strcpy(masterInfo->parentName, getActiveMaster()->name);
}
void toggleSlaves(Master* parent, Master* child) {
    if(!parent || !child) {
        INFO("Not changing slaves Parent %p Child %p", parent, child);
        return;
    }
    MPXMasterInfo* masterInfo = getMasterInfoForMaster(child);
    bool parentHasChildSlave = 0;
    FOR_EACH(Slave*, slave, getSlaves(parent)) {
        if(isInBuffer(&masterInfo->slaveNames, slave->name)) {
            parentHasChildSlave = 1;
            break;
        }
    }
    // attempt to attach the active master's slaves back to itself
    if(parentHasChildSlave) {
        INFO("Giving slaves to child");
        attachSlaveToProperMaster(child);
    }
    else {
        INFO("Giving slaves to parent");
        // give parent slave(s)
        attachAllSlavesToMaster(child, parent);
    }
}
void toggleParentMaster() {
    toggleSlaves(getParentMaster(), getActiveMaster());
}
void toggleChildMaster() {
    toggleSlaves(getActiveMaster(), getChildOfMasterByActiveSlave());
}

void swapSlaves(Master* master1, Master* master2) {
    attachAllSlavesToMaster(master1, master2);
    attachAllSlavesToMaster(master2, master1);
}

void swapXDevices(Master* master1, Master* master2) {
    if(master1 == master2)
        return;
    DEBUG("Swapping %d(%d) with %d (%d)", master1->id, getPointerID(master1), master2->id, getPointerID(master2));
    //swap keyboard focus
    xcb_input_xi_set_focus(dis, getActiveFocus(master1->id), 0, master2->id);
    xcb_input_xi_set_focus(dis, getActiveFocus(master2->id), 0, master2->id);
    short pos1[2];
    short pos2[2];
    if(getMousePosition(getPointerID(master1), root, pos1) && getMousePosition(getPointerID(master2), root, pos2)) {
        warpPointer(pos1[0], pos1[1], root, getPointerID(master2));
        warpPointer(pos2[0], pos2[1], root, getPointerID(master1));
    }
    swapSlaves(master1, master2);
    flush();
}
void swapXDevicesWithChild() {
    Master* parent = getParentMaster(getActiveMaster());
    if(parent)
        swapXDevices(getActiveMaster(), parent);
}
void swapXDevicesWithClientMaster() {
    Master* master = getClientMaster(getFocusedWindow()->id);
    if(master != getActiveMaster()) {
        swapXDevices(getActiveMaster(), master);
    }
}

static void autoAttachSlave(xcb_input_hierarchy_event_t* event) {
    if(event->flags & (XCB_INPUT_HIERARCHY_MASK_MASTER_ADDED | XCB_INPUT_HIERARCHY_MASK_SLAVE_ADDED |
            XCB_INPUT_HIERARCHY_MASK_DEVICE_ENABLED))
        restoreMPX();
}
void addAutoMPXRules(void) {
    addEvent(GENERIC_EVENT_OFFSET + XCB_INPUT_HIERARCHY, DEFAULT_EVENT(autoAttachSlave));
    addEvent(X_CONNECTION, DEFAULT_EVENT(restoreMPX));
}

void restoreMPX(void) {
    if(!masterInfoList.size)
        loadMPXMasterInfo();
    FOR_EACH(Master*, m, getAllMasters()) {
        MPXMasterInfo* masterInfo = getMasterInfoForMaster(m);
        TRACE("setting color to %0x", masterInfo->focusColor);
        m->focusColor = masterInfo->focusColor;
        attachSlaveToProperMaster(m);
    }
    initCurrentMasters();
}
void startMPX(void) {
    INFO("Starting MPX");
    if(!masterInfoList.size)
        loadMPXMasterInfo();
    FOR_EACH(MPXMasterInfo*, masterInfo, &masterInfoList) {
        const char* name = masterInfo->masterName;
        if(!getMasterByName(name)) {
            createMasterDevice(name);
        }
    }
    initCurrentMasters();
    restoreMPX();
}
static FILE* openMPXMasterInfoFile(const char* mode) {
    FILE* fp;
    if(MASTER_INFO_PATH[0] == '~') {
        char* buffer = malloc(strlen(getenv("HOME")) + strlen(MASTER_INFO_PATH));
        buffer[0] = 0;
        strcat(buffer, getenv("HOME"));
        strcat(buffer, MASTER_INFO_PATH + 1);
        fp = fopen(buffer, mode);
        free(buffer);
    }
    else
        fp = fopen(MASTER_INFO_PATH, mode);
    if(!fp) {
        ERROR("Failed to open %s mode %s", MASTER_INFO_PATH, mode);
        perror("Could not open master info path");
    }
    return fp;
}

int saveMPXMasterInfo(void) {
    INFO("Saving MPX state");
    FILE* fp = openMPXMasterInfoFile("w");
    if(!fp)
        return 0;
    FOR_EACH(Master*, master, getAllMasters()) {
        fprintf(fp, "%s\n", master->name);
        fprintf(fp, "%s\n", getMasterInfoForMaster(master)->parentName);
        fprintf(fp, "%06X\n", master->focusColor);
        FOR_EACH(Slave*, slave, getSlaves(master)) {
            fprintf(fp, "%s\n", slave->name);
        }
        fprintf(fp, "\n\n");
    };
    fclose(fp);
    return 1;
}
int loadMPXMasterInfo(void) {
    FILE* fp;
    char* line = NULL;
    size_t bSize = 0;
    ssize_t len;
    INFO("Loading saved MPX state");
    fp = openMPXMasterInfoFile("r");
    if(fp == NULL) {
        WARN("Could not read saved MPX state");
        return 0;
    }
    FOR_EACH(MPXMasterInfo*, info, &masterInfoList) {
        freeBuffer(&info->slaveNames);
        free(info);
    }
    clearArray(&masterInfoList);
    MPXMasterInfo* info = NULL;
    int state = 0;
    bool flag = 0;
    while((len = getline(&line, &bSize, fp)) != -1) {
        if(line[len - 1] == '\n')
            line[--len] = 0;
        if(len == 0) {
            if(flag) {
                state = 0;
            }
            else {
                flag = 1;
                state++;
            }
            continue;
        }
        flag = 0;
        switch(state) {
            case 0:
                INFO("Loading info for %s", line);
                info = newMPXMasterInfo(line, 0);
                break;
            case 1:
                strncpy(info->parentName, line, MAX_NAME_LEN - 1);
                break;
            case 2:
                info->focusColor = (int)strtol(line, NULL, 16);
                break;
            default:
                addString(&info->slaveNames, line);
                break;
        }
        state++;
    }
    free(line);
    fclose(fp);
    return 1;
}
