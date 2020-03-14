#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <string.h>

#include <xcb/xinput.h>

#include "bindings.h"
#include "device-grab.h"
#include "ext.h"
#include "globals.h"
#include "logger.h"
#include "masters.h"
#include "windows.h"
#include "xsession.h"

static Index<WindowID> key;
static inline WindowID* getTarget(Master* master, bool createNew) {
    return get(key, master, createNew);
}
WindowID getTarget(Master* master) {
    WindowID* id = getTarget(master, 0);
    return id ? *id : 0;
}
void setTarget(Master* master, WindowID target) {
    if(target)
        *getTarget(master, 1) = target;
    else
        remove(key, master);
}

static UserEvent lastUserEvent;
UserEvent& getLastUserEvent() {return lastUserEvent;}
void setLastUserEvent(const UserEvent& event) {lastUserEvent = event;}

WindowInfo* Binding::getWindowToActOn(const UserEvent& event)const {
    WindowID* target = getTarget(event.master, 0);
    switch(target ? TARGET_WINDOW : getWindowTarget()) {
        default:
        case DEFAULT_WINDOW:
            return getKeyboardMask(getMask()) ? event.master ? event.master->getFocusedWindow() : NULL : event.winInfo;
        case FOCUSED_WINDOW:
            return event.master ? event.master->getFocusedWindow() : NULL;
        case TARGET_WINDOW:
            return target ? getWindowInfo(*target) : event.winInfo;
    }
}
bool Binding::trigger(const UserEvent& event)const {
    INFO("Triggering " << *this);
    return shouldPassThrough(getPassThrough(), boundFunction({getWindowToActOn(event), event.master}));
}
std::ostream& operator<<(std::ostream& stream, const Binding& binding) {
    return stream << "{ Name:'" << binding.getName() << "' " << binding.getKeyBindings() << " Func:'" <<
        binding.boundFunction << "' " << binding.getMask()
        << "}" ;
}
bool BindingFlags::matches(const UserEvent& event) const {
    return (getMask() == 0 || (getMask() & event.getMask()) == event.getMask()) &&
        (getMode() == ANY_MODE || event.getMode() == ANY_MODE || event.getMode() == getMode());
}
bool Binding::matches(const UserEvent& event) const {
    if(flags.matches(event))
        for(KeyBinding* key : getKeyBindings()) {
            if(key->matchesMod(event.mod) && (key->isWildcard() || key->getDetail() == event.detail))
                return 1;
        }
    return 0;
}
std::ostream& operator<<(std::ostream& stream, const UserEvent& userEvent) {
    stream << "{ Mod:" << userEvent.mod << " Detail:" << userEvent.detail << " Mask:" << userEvent.mask << " KR:" <<
        userEvent.keyRepeat;
    stream << " Master:" << (userEvent.master ? userEvent.master->getID() : 0);
    stream << " Window:" << (userEvent.winInfo ? userEvent.winInfo->getID() : 0);
    return stream << "}";
}

bool checkBindings(const UserEvent& userEvent, const ArrayList<Binding*>& bindings) {
    TRACE("checking " << bindings.size() << " bindings");
    DEBUG("Event: " << userEvent);
    for(Binding* binding : bindings)
        if(binding->matches(userEvent) && !binding->trigger(userEvent)) {
            INFO("checkBindings terminated early");
            return 0;
        }
    return 1;
}

ArrayList<Binding*>& getDeviceBindings() {
    static UniqueArrayList<Binding*> deviceBindings;
    return deviceBindings;
}


int Binding::grab(bool ungrab) {
    bool result = 0;
    if(!isNotGrabbable()) {
        for(KeyBinding* key : getKeyBindings())
            if(key->getDetail())
                if(!ungrab)
                    result |= grabDetail(getTargetID(), key->getDetail(), key->getMod(), getMask());
                else
                    result |= ungrabDetail(getTargetID(), key->getDetail(), key->getMod(), getKeyboardMask(getMask()));
        return result;
    }
    return -1;
}

Detail KeyBinding::getDetail() {
    if(detail == 0 && buttonOrKey != 0) {
        detail = getButtonDetailOrKeyCode(buttonOrKey);
        assert(detail);
    }
    return detail;
}
std::ostream& operator<<(std::ostream& stream, const KeyBinding& binding) {
    return stream << "{ " << binding.mod << " " << binding.buttonOrKey << "}" ;
}
