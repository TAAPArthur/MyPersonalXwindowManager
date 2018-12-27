/**
 * @file events.c
 */
#include <assert.h>
#include <pthread.h>

#include <xcb/randr.h>

#include "logger.h"
#include "bindings.h"
#include "mywm-util.h"
#include "devices.h"
#include "globals.h"
#include "xsession.h"
#include "state.h"
#include "wmfunctions.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock(&mutex);
#define UNLOCK pthread_mutex_unlock(&mutex);

void lock(){LOCK}
void unlock(){UNLOCK}

static volatile int shuttingDown=0;
void requestShutdown(){
    shuttingDown=1;
}
int isShuttingDown(){
    return shuttingDown;
}
pthread_t runInNewThread(void *(*method) (void *),void*arg,int detached){
    pthread_t thread;
    int result __attribute__((unused)) =pthread_create(&thread, NULL, method, arg)==0;
    assert(result);
    if(detached)
        pthread_detach(thread);
    return thread;
}
static volatile int idle;
int getIdleCount(){
    return idle;
}


static inline xcb_generic_event_t *getNextEvent(){
    
    xcb_generic_event_t *event;
    event = xcb_poll_for_event(dis);
    if(!event && !xcb_connection_has_error(dis)){
        for(int i=0;i<POLL_COUNT;i++){
            msleep(POLL_INTERVAL);
            event = xcb_poll_for_event(dis);
            if(event)return event;
        }
        idle++;
        applyRules(eventRules[Idle],NULL);
        flush();
        LOG(LOG_LEVEL_TRACE,"Idle %d\n",idle);
        event = xcb_wait_for_event (dis);
    }
    return event;
}
void *runEventLoop(void*c __attribute__((unused))){
    LOG(LOG_LEVEL_TRACE,"starting event loop\n");
    xcb_generic_event_t *event;
    while(!isShuttingDown() && dis) {
        event=getNextEvent();
        if(isShuttingDown()||xcb_connection_has_error(dis)||!event){
            if(event)free(event);
            if(isShuttingDown())
                LOG(LOG_LEVEL_INFO,"shutting down\n");
            break;
        }

        int type=event->response_type;
        if(!IGNORE_SEND_EVENT)
            type &= 127;

        assert(type<NUMBER_OF_EVENT_RULES);
        LOG(LOG_LEVEL_TRACE,"Event detected %d %s number of rules: %d\n",
                        event->response_type,eventTypeToString(type),getSize(eventRules[type]));
        lock();
        setLastEvent(event);
        applyRules(eventRules[type<35?type:35],NULL);
        unlock();
        free(event);
        LOG(LOG_LEVEL_TRACE,"event proccesed %d\n",getActiveWorkspaceIndex());
    }
    LOG(LOG_LEVEL_DEBUG,"Exited event loop\n");
    return NULL;
}

int loadGenericEvent(xcb_ge_generic_event_t*event){
    LOG(LOG_LEVEL_TRACE,"processing generic event; ext: %d type: %d event type %d \n",
            event->extension,event->response_type,event->event_type);
    LOG(LOG_LEVEL_TRACE,"%d %d %d",
            xcb_get_extension_data(dis, &xcb_randr_id)->major_opcode,
            xcb_get_extension_data(dis, &xcb_randr_id)->first_event,
            xcb_get_extension_data(dis, &xcb_randr_id)->first_error);
    LOG(LOG_LEVEL_TRACE,"%d %d %d",
            xcb_get_extension_data(dis, &xcb_input_id)->major_opcode,
            xcb_get_extension_data(dis, &xcb_input_id)->first_event,
            xcb_get_extension_data(dis, &xcb_input_id)->first_error);


    if(event->extension == xcb_get_extension_data(dis, &xcb_input_id)->major_opcode){
        LOG(LOG_LEVEL_TRACE,"generic event detected %d %d %s\n",event->event_type,event->response_type,genericEventTypeToString(event->event_type));
        return event->event_type+GENERIC_EVENT_OFFSET;

    }
    else //if(event->extension == xcb_get_extension_data(dis, &xcb_randr_id)->major_opcode)
    {
        return 1+MONITOR_EVENT_OFFSET;
    }
}

void registerForMonitorChange(){
    xcb_randr_select_input(dis, root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
}
void registerForDeviceEvents(){
    Node*list=getDeviceBindings();

    LOG(LOG_LEVEL_DEBUG,"Grabbing %d buttons/keys\n",getSize(list));
    FOR_EACH_CIRCULAR(list,
        initBinding(getValue(list));
        grabBinding(getValue(list));
    )
    LOG(LOG_LEVEL_DEBUG,"listening for device event;  masks: %d\n",ROOT_DEVICE_EVENT_MASKS);
    passiveGrab(root, ROOT_DEVICE_EVENT_MASKS);
}
void registerForWindowEvents(int window,int mask){
    xcb_void_cookie_t cookie;
    cookie=xcb_change_window_attributes_checked(dis, window,XCB_CW_EVENT_MASK, &mask);
    catchError(cookie);
}
void registerForEvents(){
    registerForWindowEvents(root,ROOT_EVENT_MASKS);
    registerForDeviceEvents();
}
