#include <X11/extensions/XInput2.h>
#include <xcb/xinput.h>

#include "globals.h"
#include "window-masks.h"

bool ALLOW_SETTING_UNSYNCED_MASKS = 0;
bool ALLOW_UNSAFE_OPTIONS = 1;
bool LD_PRELOAD_INJECTION = 0;
bool RUN_AS_WM = 1;
bool STEAL_WM_SELECTION = 0;
int16_t DEFAULT_BORDER_WIDTH = 1;
std::string LD_PRELOAD_PATH = "/usr/lib/libmpx-patch.so";
std::string MASTER_INFO_PATH = "$HOME/.config/mpxmanager/master-info.txt";
std::string NOTIFY_CMD = "notify-send -h string:x-canonical-private-synchronous:MPX_NOTIFICATIONS -a "
    WINDOW_MANAGER_NAME;
std::string SHELL = "/bin/sh";
uint32_t AUTO_FOCUS_NEW_WINDOW_TIMEOUT = 1000;
uint32_t CRASH_ON_ERRORS = 0;
uint32_t DEFAULT_BORDER_COLOR = 0x0000FF;
uint32_t DEFAULT_MOD_MASK = Mod4Mask;
uint32_t DEFAULT_NUMBER_OF_WORKSPACES = 10;
uint32_t DEFAULT_UNFOCUS_BORDER_COLOR = 0xDDDDDD;
uint32_t EVENT_PERIOD = 100;
uint32_t IGNORE_MASK = Mod2Mask;
uint32_t KILL_TIMEOUT = 100;
uint32_t NON_ROOT_DEVICE_EVENT_MASKS = XCB_INPUT_XI_EVENT_MASK_FOCUS_OUT | XCB_INPUT_XI_EVENT_MASK_FOCUS_IN;
uint32_t NON_ROOT_EVENT_MASKS = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_VISIBILITY_CHANGE;
uint32_t POLL_COUNT = 3;
uint32_t POLL_INTERVAL = 10;
uint32_t ROOT_DEVICE_EVENT_MASKS = XCB_INPUT_XI_EVENT_MASK_HIERARCHY;
uint32_t ROOT_EVENT_MASKS = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY;
uint32_t SRC_INDICATION = 7;

WindowMask MASKS_TO_SYNC = MODAL_MASK | ABOVE_MASK | BELOW_MASK | HIDDEN_MASK | NO_TILE_MASK | STICKY_MASK |
    URGENT_MASK;

static volatile bool shuttingDown = 0;
void requestShutdown(void) {
    shuttingDown = 1;
}
bool isShuttingDown(void) {
    return shuttingDown;
}
