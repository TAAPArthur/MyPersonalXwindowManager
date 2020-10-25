/**
 * @file bindings.h
 * @brief Provides methods related to key/mouse bindings and window matching rules.
 */
#ifndef BINDINGS_H_
#define BINDINGS_H_


#include "util/arraylist.h"
#include "mywm-structs.h"
#include <stdbool.h>

/// Wildcard mask
#define ANY_MASK (-1)

/// type of key detail; X will not use a key detail greater than 255
typedef uint16_t Detail;
/// type of keySym or button ;
typedef uint32_t ButtonOrKeySym;
/// type of modifier; X will not use a value greater than 1<<15
typedef uint32_t Modifier;
typedef enum EventWindow {
    NO_WINDOW,
    DEFAULT_WINDOW,
    FOCUSED_WINDOW,
    EVENT_WINDOW,
} EventWindow;
/**
 * Optional arguments to Bindings
 *
 * These are separated into their own class to make it easier to specify only some
 */
typedef struct BindingFlags {
    /**Whether a device should be grabbed or not*/
    bool noGrab;
    ///The mask to grab;
    uint32_t mask;
    /**The target of the grab;; Default: is XIAllMasterDevices*/
    MasterID targetID;
    /// if true the binding won't trigger for key repeats
    bool noKeyRepeat;
    /// Binding will be triggered if the active master is in a compatible mode
    unsigned int mode;
    bool grabDevice;
    bool popChain;
    bool shortCircuit;
    uint16_t ignoreMod;
    EventWindow windowToPass;
} BindingFlags ;
typedef struct {
    struct Binding* bindings;
    int size;
} ChainMembers;
typedef struct BindingFunc {
    void(*func)();
    Arg arg;
    Arg arg2;
} BindingFunc;

typedef struct Binding {
    /**Modifier to match on*/
    Modifier mod;
    /**Either Button or KeySym to match**/
    ButtonOrKeySym buttonOrKey;
    BindingFunc func;
    BindingFlags flags;
    ChainMembers chainMembers;
    /**Converted detail of key bindings. Should not be set manually*/
    Detail detail;
} Binding;
#define CHAIN_MEM(M...) {(Binding[]){M}, .size=LEN(((Binding[]){M}))}

/**
 * Compares Bindings with detail and mods to see if it should be triggered.
 * If bindings->mod ==anyModier, then it will match any modifiers.
 * If bindings->detail or keycode is 0 it will match any detail
 * Intended to be extracted form a device event to contain only the needed information to match against a binding
 */
typedef struct BindingEvent {
    /// @return Returns true if the binding should be triggered based on the combination of detail and mods
    Modifier mod;
    /// either a keycode or a button
    Detail detail;
    /// the mask to match
    uint32_t mask;
    /// keyRepeat if the event was a key repeat
    bool keyRepeat;
    /// the window the binding was triggered on
    WindowInfo* winInfo;
} BindingEvent;


/**
 * Returns a Node list of all registered biddings
 * @see deviceBindings;
*/
void addBindings(Binding* b, int N);
void clearBindings();
int grabAllBindings(Binding* bindings, int numBindings, bool ungrab);
int grabBinding(Binding* binding, bool ungrab);

/**
 * Check bindings to see if they match the userEvent
 * Bindings will be checked in order a binding that matches may cause all subsequent bindings to be skipped
 * @param userEvent
 * @param bindings list of bindings to check against
 * @return 1 if a binding was matched that had passThrough == false
 */
bool checkBindings(const BindingEvent* userEvent);



void enterChain(Binding* binding, ArrayList* masterBindings);

void callBindingWithWindow(const BindingFunc* bindingFunc, EventWindow windowToPass, const BindingEvent* event);
static inline void callBinding(const BindingFunc* bindingFunc) {
    callBindingWithWindow(bindingFunc, NO_WINDOW, NULL);
}



#endif
