#include <assert.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/Xlib-xcb.h>

#include "boundfunction.h"
#include "globals.h"
#include "monitors.h"
#include "user-events.h"
#include "util/logger.h"
#include "xevent.h"
#include "xutil/xsession.h"


static volatile bool shuttingDown;
void requestShutdown() {
    shuttingDown = 1;
}
bool isShuttingDown(void) {
    return shuttingDown;
}
static volatile int idle;
int getIdleCount() {
    return idle;
}
static int lastQueuedEventSequenceNumber;
static int lastEventSequenceNumber;
uint32_t getLastDetectedEventSequenceNumber() {return MAX(lastQueuedEventSequenceNumber, lastEventSequenceNumber);}
uint16_t getCurrentSequenceNumber(void) {
    return lastEventSequenceNumber;
}

typedef struct {
    xcb_generic_event_t* arr[MPX_EVENT_QUEUE_SIZE];
    /// next index to read from
    uint16_t bufferIndexRead;
    /// next index to write to
    uint16_t bufferIndexWrite;
} EventQueue;
static EventQueue eventQueue;
int getEventQueueSize() {
    return (eventQueue.bufferIndexWrite - eventQueue.bufferIndexRead + MPX_EVENT_QUEUE_SIZE) % MPX_EVENT_QUEUE_SIZE;
}
bool isEventQueueEmpty() {
    return eventQueue.bufferIndexWrite == eventQueue.bufferIndexRead;
}

static bool pushEvent(xcb_generic_event_t* event) {
    if(!event) {
        TRACE("No more events to read");
        return NULL;
    }
    eventQueue.arr[eventQueue.bufferIndexWrite++ % MPX_EVENT_QUEUE_SIZE] = event;
    lastQueuedEventSequenceNumber = event->sequence;
    return (eventQueue.bufferIndexWrite - eventQueue.bufferIndexRead) % MPX_EVENT_QUEUE_SIZE;
}

static xcb_generic_event_t* popEvent() {
    return eventQueue.arr[eventQueue.bufferIndexRead++ % MPX_EVENT_QUEUE_SIZE];
}


struct {
    struct pollfd pollFDs[255];
    void(*extraEventCallBacks[255])();
    int numberOfFDsToPoll;
} eventFDInfo = {.numberOfFDsToPoll = 1};
static int xFD;
void addExtraEvent(int fd, int mask,  void(*callBack)()) {
    int index = eventFDInfo.numberOfFDsToPoll++;
    eventFDInfo.pollFDs[index] = (struct pollfd) {fd, mask};
    eventFDInfo.extraEventCallBacks[index] = callBack;
}
static void removeExtraEvent(int index) {
    for(int i = index + 1; i < eventFDInfo.numberOfFDsToPoll; i++) {
        eventFDInfo.pollFDs[i - 1] = eventFDInfo.pollFDs[i];
        eventFDInfo.extraEventCallBacks[i - 1] = eventFDInfo.extraEventCallBacks[i];
    }
    eventFDInfo.numberOfFDsToPoll--;
}

static inline int processEvents(int timeout) {
    int numEvents;
    assert(eventFDInfo.numberOfFDsToPoll);
    TRACE("polling for %d events timeout %d", eventFDInfo.numberOfFDsToPoll, timeout);
    if((numEvents = poll(eventFDInfo.pollFDs, eventFDInfo.numberOfFDsToPoll, timeout))) {
        TRACE("FD poll returned %d events out of %d", numEvents, eventFDInfo.numberOfFDsToPoll);
        for(int i = eventFDInfo.numberOfFDsToPoll - 1; i >= 0; i--) {
            if(eventFDInfo.pollFDs[i].revents) {
                if(eventFDInfo.pollFDs[i].revents & eventFDInfo.pollFDs[i].events) {
                    eventFDInfo.extraEventCallBacks[i](eventFDInfo.pollFDs[i].fd, eventFDInfo.pollFDs[i].revents);
                }
                if(eventFDInfo.pollFDs[i].revents & (POLLERR | POLLNVAL | POLLHUP)) {
                    WARN("Removing extra event index %d", i);
                    removeExtraEvent(i);
                }
            }
        }
    }
    return numEvents;
}

static inline xcb_generic_event_t* pollForXEvent(void) {
    TRACE("Polling for event");
    xcb_generic_event_t* event = xcb_poll_for_event(dis);
    if(event) {
        TRACE("Reading events on the X queue");
        while(pushEvent(xcb_poll_for_queued_event(dis)));
        TRACE("Finished reading events off of the X queue");
    }
    return event;
}

xcb_generic_event_t* getXEvent(void) {
    xcb_generic_event_t* event = NULL;
    if(!isEventQueueEmpty())
        event = popEvent();
    if(!event)
        event = pollForXEvent();
    return event;
}

void processXEvent(xcb_generic_event_t* event) {
    int type = event->response_type & 127;
    // TODO pre event processing rule
    type = type < LASTEvent ? type : EXTRA_EVENT;
    lastEventSequenceNumber = event->sequence;
    applyEventRules(type, event);
    free(event);
#ifdef DEBUG
    XSync(dpy, 0);
    fflush(NULL);
#endif
}
void processXEvents(void) {
    xcb_generic_event_t* event = NULL;
    TRACE("Process X events");
    while(!isShuttingDown()) {
        event = getXEvent();
        if(!event)
            break;
        processXEvent(event);
    }
    TRACE("Finished Process X events");
}

void setIdleProperty() {
    setWindowPropertyInt(getPrivateWindow(), MPX_IDLE_PROPERTY, XCB_ATOM_CARDINAL, getIdleCount());
}
void runEventLoop() {
    xFD = xcb_get_file_descriptor(dis);
    eventFDInfo.pollFDs[0] = (struct pollfd) {xFD, POLLIN};
    eventFDInfo.extraEventCallBacks[0] = processXEvents;
    flush();
    shuttingDown = 0;
    INFO("Starting event loop");
    while(!isShuttingDown()) {
        assert(isEventQueueEmpty());
        if(processEvents(IDLE_TIMEOUT)) {
            continue;
        }
        applyEventRules(IDLE, NULL);
        flush();
        if(pushEvent(xcb_poll_for_queued_event(dis))) {
            processXEvents();
            continue;
        }
        if(processEvents(IDLE_TIMEOUT)) {
            continue;
        }
        idle++;
        applyEventRules(TRUE_IDLE, NULL);
        DEBUG("Idle %d", idle);
        flush();
        if(!isShuttingDown()) {
            if(pushEvent(xcb_poll_for_event(dis))) {
                processXEvents();
                continue;
            }
            processEvents(-1);
        }
    }
    INFO("Exiting event loop");
}

void onGenericEvent(xcb_ge_generic_event_t* event) {
    int type = loadGenericEvent(event);
    if(type)
        applyEventRules(type, event);
}
void addXIEventSupport() {
    addEvent(XCB_GE_GENERIC, DEFAULT_EVENT(onGenericEvent));
}

int loadGenericEvent(xcb_ge_generic_event_t* event) {
    TRACE("processing generic event; ext: %d type: %d event type %d seq %d",
        event->extension, event->response_type, event->event_type, event->sequence);
    if(event->extension == xcb_get_extension_data(dis, &xcb_input_id)->major_opcode) {
        int type = event->event_type + GENERIC_EVENT_OFFSET;
        return type;
    }
    WARN("Could not process generic event");
    return 0;
}
int isSyntheticEvent(void* event) {
    return ((xcb_generic_event_t*)event)->response_type > 127;
}

void addShutdownOnIdleRule() {
    addEvent(TRUE_IDLE, DEFAULT_EVENT(requestShutdown, LOWEST_PRIORITY));
}
