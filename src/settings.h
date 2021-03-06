/**
 * @file settings.h
 * @brief contains default user settings
 */


#ifndef MPX_SETTINGS_H_
#define MPX_SETTINGS_H_

/// Layout related keybindings
#define LAYOUT_MODE 1

/// User specified start up method to run before connection to X
extern void (*startupMethod)();
/**
 * Loads default settings
 */
void onStartup(void);

/**
 * Load default settings
 */
void loadNormalSettings(void);

/**
 * Prints names of workspaces that are visible or have windows.
 * The layout of each workspace is also printed
 * The title of the active window is printed and the active workspace is marked
 */
void defaultPrintFunction(void);

/**
 * Will be called on startup before the XServer is initialized so the user can set global vars, run
 * arbitrary commands and most commonly add device bindings and event rules
 */
void loadSettings(void);
/**
 * Adds a collection of rules that aren't strictly needed to call this a functioning WM, but may align with expected
 * behavior
 */
void addSuggestedRules();
#endif
