/**
 * @file
 */

#include <stdlib.h>

#include <X11/keysym.h>

#include "functions.h"
#include "globals.h"
#include "devices.h"
#include "layouts.h"
#include "util/logger.h"
#include "util/threads.h"
#include "masters.h"
#include "monitors.h"
#include "settings.h"
#include "system.h"
#include "xutil/window-properties.h"
#include "xutil/device-grab.h"
#include "windows.h"
#include "xevent.h"
#include "wm-rules.h"
#include "wmfunctions.h"
#include "workspaces.h"

//static Binding CYCLE_WINDOWCHAIN_BINDINGS[] = {
static Binding CHAIN_BINDINGS[] = {
    {
        Mod1Mask, XK_Tab, setFocusStackFrozen, {1}, .flags = {.grabDevice = 1, .ignoreMod = ShiftMask}, .chainMembers = CHAIN_MEM(
        {Mod1Mask, XK_Tab, cycleWindows, {DOWN}, .flags = {.noGrab = 1}},
        {Mod1Mask | ShiftMask, XK_Tab, cycleWindows, {UP}, .flags = {.noGrab = 1}},
        {WILDCARD_MODIFIER, XK_Alt_L, setFocusStackFrozen, {0}, .flags = {.noGrab = 1, .popChain = 1, .mask = XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE}},
        )
    },
    {
        Mod4Mask, XK_Tab, setFocusStackFrozen, {1}, .flags = {.grabDevice = 1, .ignoreMod = ShiftMask}, .chainMembers = CHAIN_MEM(
        {Mod4Mask, XK_Tab, shiftFocusToNextClass, {DOWN}, .flags = {.noGrab = 1}},
        {Mod4Mask | ShiftMask, XK_Tab, shiftFocusToNextClass, {UP}, .flags = {.noGrab = 1}},
        {WILDCARD_MODIFIER, XK_Super_L, setFocusStackFrozen, {0}, .flags = {.noGrab = 1, .popChain = 1, .mask = XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE}},
        )
    },
    {
        DEFAULT_MOD_MASK, Button1, startWindowMove, .flags = {.grabDevice = 1, .windowToPass = EVENT_WINDOW}, .chainMembers = CHAIN_MEM(
        {DEFAULT_MOD_MASK, 0, updateWindowMoveResize, .flags = {.shortCircuit = 1, .noGrab = 1, .mask = XCB_INPUT_XI_EVENT_MASK_MOTION | XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS}},
        {WILDCARD_MODIFIER, XK_Escape, cancelWindowMoveResize, .flags = {.noGrab = 1, .popChain = 1}},
        {WILDCARD_MODIFIER, 0, commitWindowMoveResize, .flags = {.noGrab = 1, .popChain = 1}},
        )
    },
    {
        DEFAULT_MOD_MASK, Button3, startWindowResize, .flags = {.grabDevice = 1, .windowToPass = EVENT_WINDOW}, .chainMembers = CHAIN_MEM(
        {DEFAULT_MOD_MASK, 0, updateWindowMoveResize, .flags = {.shortCircuit = 1, .noGrab = 1, .mask = XCB_INPUT_XI_EVENT_MASK_MOTION | XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS}},
        {WILDCARD_MODIFIER, XK_Escape, cancelWindowMoveResize, .flags = {.noGrab = 1, .popChain = 1}},
        {WILDCARD_MODIFIER, 0, commitWindowMoveResize, .flags = {.noGrab = 1, .popChain = 1}},
        )
    },
};

void recompileAndRestart() {
    if(spawnAndWait("mpxmanager --recompile") == 0)
        restart();
}
/**
 * Creates a set of bindings related to switching workspaces
 * @param K the key to bind; various modifiers will be used for the different functions
 * @param N the workspace to switch to/act on
 */
#define WORKSPACE_OPERATION(K,N) \
    {DEFAULT_MOD_MASK, K, switchToWorkspace, {N}}, \
    {DEFAULT_MOD_MASK|ShiftMask, K, moveToWorkspace, {N}, .flags.windowToPass=FOCUSED_WINDOW}, \
    {DEFAULT_MOD_MASK|ControlMask, K, swapActiveMonitor, {N}}

/**
 * Creates a set of bindings related to the windowStack
 * @param KEY_UP
 * @param KEY_DOWN
 * @param KEY_LEFT
 * @param KEY_RIGHT
 */
#define STACK_OPERATION(KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT) \
    {DEFAULT_MOD_MASK, KEY_UP, swapPosition, {UP}}, \
    {DEFAULT_MOD_MASK, KEY_DOWN, swapPosition, {DOWN}}, \
    {DEFAULT_MOD_MASK, KEY_LEFT, shiftFocus, {UP}}, \
    {DEFAULT_MOD_MASK | ShiftMask, KEY_LEFT, shiftFocusToNextClass,{UP}}, \
    {DEFAULT_MOD_MASK, KEY_RIGHT, shiftFocus, {DOWN}}, \
    {DEFAULT_MOD_MASK | ShiftMask, KEY_RIGHT, shiftFocusToNextClass, {DOWN}} \


/// add default bindings
Binding DEFAULT_BINDINGS[] = {
    WORKSPACE_OPERATION(XK_1, 0),
    WORKSPACE_OPERATION(XK_2, 1),
    WORKSPACE_OPERATION(XK_3, 2),
    WORKSPACE_OPERATION(XK_4, 3),
    WORKSPACE_OPERATION(XK_5, 4),
    WORKSPACE_OPERATION(XK_6, 5),
    WORKSPACE_OPERATION(XK_7, 6),
    WORKSPACE_OPERATION(XK_8, 7),
    WORKSPACE_OPERATION(XK_9, 8),
    WORKSPACE_OPERATION(XK_0, 9),
    STACK_OPERATION(XK_Up, XK_Down, XK_Left, XK_Right),
    STACK_OPERATION(XK_H, XK_J, XK_K, XK_L),

    {WILDCARD_MODIFIER, Button1, activateWorkspaceUnderMouse, .flags = {.shortCircuit = 0, .noGrab = 1}},
    {WILDCARD_MODIFIER, Button1, .func.funcBool = activateWindow, .flags = {.shortCircuit = 0, .noGrab = 1,  .windowToPass = EVENT_WINDOW, }},
    {0, Button1, replayPointerEvent},

    {DEFAULT_MOD_MASK, XK_c, killClientOfWindowInfo, .flags = {.windowToPass = FOCUSED_WINDOW, .noKeyRepeat = 1}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_c, killClientOfWindowInfo, .flags = { .windowToPass = EVENT_WINDOW, .noKeyRepeat = 1}},
    //{DEFAULT_MOD_MASK | ControlMask, XK_c, destroyWindow, {.noKeyRepeat = 1}},
    //{DEFAULT_MOD_MASK | ControlMask | ShiftMask, XK_c, destroyWindow, { .windowTarget = TARGET_WINDOW, .noKeyRepeat = 1}},
    {DEFAULT_MOD_MASK, Button1, floatWindow, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK, Button3, floatWindow, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK | ShiftMask, Button1, sinkWindow, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK, XK_t, sinkWindow, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK | Mod1Mask, XK_t, toggleMask, {STICKY_MASK}, .flags = {.windowToPass = FOCUSED_WINDOW}},
    // TODO readd
    //{DEFAULT_MOD_MASK, XK_u, activateNextUrgentWindow},
    //{DEFAULT_MOD_MASK | ShiftMask, XK_y, popHiddenWindow},
    {DEFAULT_MOD_MASK, XK_y, toggleMask, HIDDEN_MASK, .flags = {.windowToPass = FOCUSED_WINDOW}},

    {DEFAULT_MOD_MASK, XK_F11, .func.funcBool = toggleActiveLayout, {.p = &FULL}},
    {DEFAULT_MOD_MASK, XK_F, toggleMask, {FULLSCREEN_MASK}, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_F, toggleMask, {ROOT_FULLSCREEN_MASK}, .flags = {.windowToPass = FOCUSED_WINDOW}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_F11, toggleMask, {FULLSCREEN_MASK}, .flags = {.windowToPass = FOCUSED_WINDOW}},

    {DEFAULT_MOD_MASK, XK_space, cycleLayouts, {DOWN}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_space, cycleLayouts, {UP}},

    {DEFAULT_MOD_MASK, XK_Return, shiftTopAndFocus},

    {DEFAULT_MOD_MASK | ControlMask | ShiftMask, XK_q, requestShutdown},
    {DEFAULT_MOD_MASK | ShiftMask, XK_q, restart},
    {DEFAULT_MOD_MASK, XK_q, recompileAndRestart},

    {DEFAULT_MOD_MASK, XK_F1, setActiveMode, {0}, .flags = {.mode = ANY_MODE}},
    {DEFAULT_MOD_MASK, XK_F2, setActiveMode, {1}, .flags = {.mode = ANY_MODE}},
    {DEFAULT_MOD_MASK, XK_F3, setActiveMode, {2}, .flags = {.mode = ANY_MODE}},
    {DEFAULT_MOD_MASK, XK_F4, setActiveMode, {3}, .flags = {.mode = ANY_MODE}},

    {DEFAULT_MOD_MASK, XK_r, retile},
    {DEFAULT_MOD_MASK | ShiftMask, XK_r, restoreActiveLayout, .flags = {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK, XK_p, increaseLayoutArg, {LAYOUT_PADDING}, {10}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_p, increaseLayoutArg, {LAYOUT_PADDING}, {-10}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK, XK_b, increaseLayoutArg, {LAYOUT_NO_BORDER}, {1}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK, XK_d, increaseLayoutArg, {LAYOUT_DIM}, {1}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK, XK_t, increaseLayoutArg, {LAYOUT_TRANSFORM}, {1}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_t, increaseLayoutArg, {LAYOUT_TRANSFORM}, {-1}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK, XK_a, increaseLayoutArg, {LAYOUT_ARG}, {1}, {.mode = LAYOUT_MODE}},
    {DEFAULT_MOD_MASK | ShiftMask, XK_a, increaseLayoutArg, {LAYOUT_ARG}, {-1}, {.mode = LAYOUT_MODE}},
};


void defaultPrintFunction(void) {
    if(STATUS_FD == 0)
        return;
    if(isLogging(LOG_LEVEL_DEBUG)) {
        dprintf(STATUS_FD, "[%d]: ", getIdleCount());
    }
    if(getActiveMode())
        dprintf(STATUS_FD, "%d: ", getActiveMode());
    if(getAllMasters()->size > 1)
        dprintf(STATUS_FD, "%dM %dA ", getAllMasters()->size, getActiveMasterKeyboardID());
    FOR_EACH(Workspace*, w, getAllWorkspaces()) {
        const char* color;
        if(hasWindowWithMask(w, URGENT_MASK))
            color = "cyan";
        else if(isWorkspaceVisible(w))
            color = "green";
        else if(hasWindowWithMask(w, MAPPABLE_MASK))
            color = "yellow";
        else continue;
        dprintf(STATUS_FD, "^fg(%s)%s%s:%s^fg() ", color, w->name, w == getActiveWorkspace() ? "*" : "",
            getLayout(w) ? getLayout(w)->name : "");
    }
    if(getActiveMaster()->bindings.size)
        dprintf(STATUS_FD, "[(%d)] ", getActiveMaster()->bindings.size);
    if(getFocusedWindow() && isNotInInvisibleWorkspace(getFocusedWindow())) {
        if(isLogging(LOG_LEVEL_DEBUG))
            dprintf(STATUS_FD, "%0x ", getFocusedWindow()->id);
        dprintf(STATUS_FD, "^fg(%s)%s^fg()", "green", getFocusedWindow()->title);
    }
    else {
        dprintf(STATUS_FD, "Focused on %0xd (root: %0xd)", getActiveFocus(getActiveMasterKeyboardID()), root);
    }
    dprintf(STATUS_FD, "\n");
}
void loadNormalSettings() {
    INFO("Loading normal settings");
    //TODO
    //printStatusMethod = defaultPrintFunction;
    addBindings(DEFAULT_BINDINGS, LEN(DEFAULT_BINDINGS));
    addBindings(CHAIN_BINDINGS, LEN(CHAIN_BINDINGS));
}
void addSuggestedRules() {
    /* TODO
    addAutoFocusRule();
    addAutoTileRules();
    addDesktopRule();
    addEWMHRules();
    addFloatRule();
    addIgnoreInputOnlyWindowsRule();
    addIgnoreNonTopLevelWindowsRule();
    addInterClientCommunicationRule();
    addKeepTransientsOnTopRule();
    addMoveNonTileableWindowsToWorkspaceBounds();
    addPrintStatusRule();
    addStickyPrimaryMonitorRule();
    */
}
void __attribute__((weak)) loadSettings(void) {
    addSuggestedRules();
    loadNormalSettings();
}
void (*startupMethod)();
void onStartup(void) {
    INFO("Starting up");
    addWorkspaces(DEFAULT_NUMBER_OF_WORKSPACES);
    addBasicRules();
    if(!RUN_AS_WM)
        ROOT_EVENT_MASKS &= ~WM_MASKS;
    if(startupMethod)
        startupMethod();
    lock();
    openXDisplay();
    unlock();
}
