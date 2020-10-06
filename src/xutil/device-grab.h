/**
 * @file
 * Proves methods to grab devices or buttons/keys
 */
#ifndef MPX_DEVICE_GRAB_H_
#define MPX_DEVICE_GRAB_H_

#include "../masters.h"

/**
 * Establishes a passive grab on the given window for events indicated by mask.
 * Multiple clients can have passive grabs on the same window
 * If the client already has an passive grab, it it replaced
 * @param window
 * @param maskValue
 */
void passiveGrab(WindowID window, uint32_t maskValue);
/**
 * Ends a passive grab on the given window for events
 * @param window
 */
void passiveUngrab(WindowID window);

/**
 *
 * Grabs the keyboard or mouse
 * @param deviceID a (non-special) device to grab
 * @param maskValue mask of the events to grab
 * @return 0 on success
 * @see XIGrabDevice
 */
int grabDevice(MasterID deviceID, uint32_t maskValue);
/**
 * Ungrabs the keyboard or mouse
 * Note that id has to be a real (non-special) device id
 * @param id id of the device to ungrab
 * @return 0 on success
 */
int ungrabDevice(MasterID id);
/**
 * Grabs the specified detail/mod combination
 *
 * @param deviceID the device id to grab (supports special ids)
 * @param detail the key or button value to grab
 * @param mod
 * @param maskValue specifies what type of event we are interested in
 * @return 0 iff the grab succeeded
 */
int grabDetail(MasterID deviceID, uint32_t detail, uint32_t mod, uint32_t maskValue, uint32_t ignoreMod);
/**
 * Ungrabs the specified detail/mod combination
 * @param deviceID the device id to grab (supports special ids)
 * @param detail the key or button value to grab
 * @param mod
 * @param isKeyboard used to tell if a key or button should be grabbed
 * @return 1 iff the grab succeeded
 */
int ungrabDetail(MasterID deviceID, uint32_t detail, uint32_t mod, uint32_t ignoreMod, bool isKeyboard);

/**
 * Replays current pointer event to children of the grab window
 * Wraps xcb_allow_events
 */
void replayPointerEvent();

/**
 *
 * @param id
 * @return 0 iff the grab succeeded
 * @see grabDevice()
 */
static inline int grabKeyboard(MasterID id) {
    return grabDevice(id, KEYBOARD_MASKS);
}

static inline void grabActiveKeyboard(void) {grabKeyboard(getActiveMasterKeyboardID());}
static inline void ungrabActiveKeyboard(void) {ungrabDevice(getActiveMasterKeyboardID());}

/**
 *
 * @param id
 * @return 0 iff the grab succeeded
 * @see grabDevice()
 */
static inline int grabPointer(MasterID id) {
    return grabDevice(id, POINTER_MASKS);
}
static inline int grabActivePointer(void) {return grabPointer(getActiveMasterPointerID());}

/**
 * Returns the mask that relates to a keyboard devices
 * @param mask
 * @return mask & KEYBOARD_MASKS
 */
static inline uint8_t getKeyboardMask(int mask) {
    return mask & KEYBOARD_MASKS;
}
/**
 * Returns the mask that relates to a pointer devices
 * @param mask
 * @return mask & POINTER_MASKS
 */
static inline uint8_t getPointerMask(int mask) {
    return mask & POINTER_MASKS;
}
/**
 * @param id
 *
 * @return 1 iff id is 0 or 1
 */
static inline bool isSpecialID(MasterID id) {
    return id == XIAllMasterDevices || id == XIAllDevices;
}
/**
 * Declare interest in select window events
 * @param window
 * @param mask
 * @return the error code if an error occurred
 */
int registerForWindowEvents(WindowID window, int mask);

/**
 * Undoes registerForMonitorChange
 *
 * @param window
 *
 * @return the error code if an error occurred
 */
int unregisterForWindowEvents(WindowID window) ;
#endif
