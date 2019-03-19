/**
 * @file settings.h
 * @brief contains default user settings
 */


#ifndef SETTINGS_H_
#define SETTINGS_H_

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
 * Will be called on startup before the XServer is initilized so the user can set global vars, run
 * arbitary commands and most commonly add device bindings and event rules
 */
void loadSettings(void);
#endif
