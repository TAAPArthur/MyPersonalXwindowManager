/**
 * @file events.c
 * @copybrief events.h
 */
#include <assert.h>

#ifndef NO_XRANDR
    #include <xcb/randr.h>
#endif

#include "bindings.h"
#include "events.h"
#include "devices.h"
#include "globals.h"
#include "logger.h"
#include "mywm-util.h"
#include "state.h"
#include "wmfunctions.h"
#include "xsession.h"

static volatile int idle;
static unsigned int periodCounter;
int getIdleCount(){
    return idle;
}


/// Holds an Arraylist of rules that will be applied in response to various conditions
static ArrayList eventRules[NUMBER_OF_EVENT_RULES];

/// Holds batch events
typedef struct {
    /// how many times the event has been triggerd
    int counter;
    /// the list of events to trigger when counter is non zero
    ArrayList list;
} BatchEventList;

static BatchEventList batchEventRules[NUMBER_OF_EVENT_RULES];

ArrayList* getEventRules(int i){
    assert(i < NUMBER_OF_EVENT_RULES);
    return &eventRules[i];
}
void clearAllRules(void){
    for(unsigned int i = 0; i < NUMBER_OF_EVENT_RULES; i++)
        clearList(getEventRules(i));
}

ArrayList* getBatchEventRules(int i){
    assert(i < NUMBER_OF_EVENT_RULES);
    return &batchEventRules[i].list;
}
void incrementBatchEventRuleCounter(int i){
    batchEventRules[i].counter++;
}
void applyBatchRules(void){
    for(unsigned int i = 0; i < NUMBER_OF_EVENT_RULES; i++)
        if(batchEventRules[i].counter){
            LOG(LOG_LEVEL_TRACE, "Applying Batch rules %d (count:%d) %s number of rules: %d\n", i, batchEventRules[i].counter,
                eventTypeToString(i), getSize(getBatchEventRules(i)));
            batchEventRules[i].counter = 0;
            applyRules(getBatchEventRules(i), NULL);
        }
}
int applyEventRules(int type, WindowInfo* winInfo){
    LOG(LOG_LEVEL_TRACE, "Event detected %d %s number of rules: %d\n",
        type, eventTypeToString(type), getSize(getEventRules(type)));
    incrementBatchEventRuleCounter(type);
    return applyRules(getEventRules(type), winInfo);
}
void clearAllBatchRules(void){
    for(unsigned int i = 0; i < NUMBER_OF_EVENT_RULES; i++)
        clearList(getBatchEventRules(i));
}

static inline xcb_generic_event_t* getNextEvent(){
    if(EVENT_PERIOD && ++periodCounter > EVENT_PERIOD){
        periodCounter = 0;
        applyEventRules(Periodic, NULL);
    }
    static xcb_generic_event_t* event;
    event = xcb_poll_for_event(dis);
    if(!event && !xcb_connection_has_error(dis)){
        for(int i = 0; i < POLL_COUNT; i++){
            msleep(POLL_INTERVAL);
            event = xcb_poll_for_event(dis);
            if(event)return event;
        }
        periodCounter = 0;
        lock();
        applyBatchRules();
        applyEventRules(Periodic, NULL);
        applyEventRules(Idle, NULL);
        idle++;
        flush();
        unlock();
        LOG(LOG_LEVEL_VERBOSE, "Idle %d\n", idle);
        event = xcb_wait_for_event(dis);
    }
    return event;
}

int isSyntheticEvent(){
    xcb_generic_event_t* event = getLastEvent();
    return event->response_type > 127;
}


void* runEventLoop(void* arg __attribute__((unused))){
    xcb_generic_event_t* event;
    int xrandrFirstEvent = -1;
#ifndef NO_XRANDR
    xrandrFirstEvent = xcb_get_extension_data(dis, &xcb_randr_id)->first_event;
#endif
    LOG(LOG_LEVEL_TRACE, "starting event loop; xrandr event %d\n", xrandrFirstEvent);
    while(!isShuttingDown() && dis){
        event = getNextEvent();
        if(isShuttingDown() || xcb_connection_has_error(dis) || !event){
            if(event)free(event);
            if(isShuttingDown())
                LOG(LOG_LEVEL_INFO, "shutting down\n");
            break;
        }
        int type = event->response_type;
        if(!IGNORE_SEND_EVENT)
            type &= 127;
        if(type < 127){
            lock();
            setLastEvent(event);
            type = type < LASTEvent ? type : type == xrandrFirstEvent ? onScreenChange : ExtraEvent;
            applyEventRules(type, NULL);
            unlock();
        }
        free(event);
#ifdef DEBUG
        XSync(dpy, 0);
#endif
        LOG(LOG_LEVEL_VERBOSE, "event proccesed\n");
    }
    LOG(LOG_LEVEL_DEBUG, "Exited event loop\n");
    return NULL;
}
int loadGenericEvent(xcb_ge_generic_event_t* event){
    LOG(LOG_LEVEL_ALL, "processing generic event; ext: %d type: %d event type %d  seq %d\n",
        event->extension, event->response_type, event->event_type, event->sequence);
    if(event->extension == xcb_get_extension_data(dis, &xcb_input_id)->major_opcode){
        int type = event->event_type + GENERIC_EVENT_OFFSET;
        return type;
    }
    return 0;
}

void registerForMonitorChange(){
#ifndef NO_XRANDR
    catchError(xcb_randr_select_input_checked(dis, root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE));
#endif
}
void registerForDeviceEvents(){
    ArrayList* list = getDeviceBindings();
    LOG(LOG_LEVEL_DEBUG, "Grabbing %d buttons/keys\n", getSize(list));
    FOR_EACH(Binding*, binding, list){
        initBinding(binding);
        grabBinding(binding);
    }
    LOG(LOG_LEVEL_DEBUG, "listening for device event;  masks: %d\n", ROOT_DEVICE_EVENT_MASKS);
    if(ROOT_DEVICE_EVENT_MASKS)
        passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
}
int registerForWindowEvents(WindowID window, int mask){
    xcb_void_cookie_t cookie;
    cookie = xcb_change_window_attributes_checked(dis, window, XCB_CW_EVENT_MASK, &mask);
    return catchErrorSilent(cookie);
}
void registerForEvents(){
    if(ROOT_EVENT_MASKS)
        registerForWindowEvents(root, ROOT_EVENT_MASKS);
    registerForDeviceEvents();
    registerForMonitorChange();
}
