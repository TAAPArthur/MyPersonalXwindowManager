#include <assert.h>
#include <string.h>

#include <X11/extensions/XInput2.h>
#include <xcb/xinput.h>
#ifndef NO_XRANDR
#include <xcb/randr.h>
#endif

#include "../util/logger.h"
#include "device-grab.h"
#include "xsession.h"

#pragma GCC diagnostic ignored "-Wnarrowing"

void passiveGrab(WindowID window, uint32_t maskValue) {
    TRACE("Passively Selecting %d  on window %d", maskValue, window);
    XIEventMask eventMask = {XIAllDevices, 4, (unsigned char*)& maskValue};
    XISelectEvents(dpy, window, &eventMask, 1);
}
void passiveUngrab(WindowID window) {
    passiveGrab(window, 0);
}

int grabDevice(MasterID deviceID, uint32_t maskValue) {
    assert(!isSpecialID(deviceID));
    XIEventMask eventMask = {deviceID, 4, (unsigned char*)& maskValue};
    INFO("Grabbing device %d with mask %d", deviceID, maskValue);
    return XIGrabDevice(dpy, deviceID, root, CurrentTime, None, GrabModeAsync,
            GrabModeAsync, 1, &eventMask);
}
int ungrabDevice(MasterID id) {
    INFO("Ungrabbing device %d", id);
    return XIUngrabDevice(dpy, id, 0);
}

int grabDetail(MasterID deviceID, uint32_t detail, uint32_t mod, uint32_t maskValue, uint32_t ignoreMod) {
    XIEventMask eventMask = {deviceID, 2, (unsigned char*)& maskValue};
    XIGrabModifiers modifiers[4] = {{mod}, {mod | IGNORE_MASK}, {mod | ignoreMod}, {mod | IGNORE_MASK | ignoreMod} };
    TRACE("Grabbing detail on %d detail:%d mod:%d mask: %d", deviceID, detail, mod, maskValue);
    int size = ignoreMod ? LEN(modifiers) : LEN(modifiers) / 2;
    if(!getKeyboardMask(maskValue))
        return XIGrabButton(dpy, deviceID, detail, root, 0,
                maskValue & XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE ? XIGrabModeAsync : XIGrabModeSync, XIGrabModeAsync, 1, &eventMask,
                size, modifiers);
    else
        return XIGrabKeycode(dpy, deviceID, detail, root, XIGrabModeAsync, XIGrabModeAsync,
                1, &eventMask, size, modifiers);
}
int ungrabDetail(MasterID deviceID, uint32_t detail, uint32_t mod, uint32_t ignoreMod, bool isKeyboard) {
    DEBUG("UNGrabbing device:%d detail:%d mod:%d %d",
        deviceID, detail, mod, isKeyboard);
    XIGrabModifiers modifiers[4] = {{mod}, {mod | IGNORE_MASK}, {mod | ignoreMod}, {mod | IGNORE_MASK | ignoreMod} };
    int size = ignoreMod ? LEN(modifiers) : LEN(modifiers) / 2;
    if(!isKeyboard)
        return XIUngrabButton(dpy, deviceID, detail, root, size, modifiers);
    else
        return XIUngrabKeycode(dpy, deviceID, detail, root, size, modifiers);
}
void replayPointerEvent() {
    TRACE("Replaying pointer events");
    xcb_allow_events(dis, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
}
void replayKeyboardEvent() {
    TRACE("Replaying keyboard event");
    xcb_allow_events(dis, XCB_ALLOW_REPLAY_KEYBOARD, XCB_CURRENT_TIME);
}

int registerForWindowEvents(WindowID window, int mask) {
    xcb_void_cookie_t cookie;
    cookie = xcb_change_window_attributes_checked(dis, window, XCB_CW_EVENT_MASK, &mask);
    return catchErrorSilent(cookie);
}
int unregisterForWindowEvents(WindowID window) {
    return registerForWindowEvents(window, 0);
}
