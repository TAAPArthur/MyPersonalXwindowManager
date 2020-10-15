/**
 * @file communications.h
 * Contains methods to used to remote communicate with the WM
 */
#ifndef MPXMANAGER_COMMUTNICATE_H_
#define MPXMANAGER_COMMUTNICATE_H_

#include "boundfunction.h"
#include "bindings.h"

/// flags that control how options are handled
enum OptionFlags {
    /// if true and the command was sent, program will fork before executing this command and attempt redirect output to the caller's stdout
    FORK_ON_RECEIVE = 1 << 0,
    /// Indicates an option that is a potential security vulnerability
    UNSAFE = 1 << 1,
    /// Send confirmation before command is run
    CONFIRM_EARLY = 1 << 2,
    /// marks this function as one that only modifies a global variable
    VAR_SETTER = 1 << 4,
    REQUEST_INT = 1 << 5,
    REQUEST_STR = 1 << 6,
    REQUEST_MULTI = 1 << 7,
};

/**
 * Holds a function that takes an string or an int like parameter
 * If a variable pointer is passed in, it is wrapped in a setter
 */

typedef struct {
    /// option name
    const char* name;
    BindingFunc bindingFunc;
    /// bitmask of OptionFlags
    int flags;
} Option ;
bool matchesOption(Option* o, const char* str, char empty);

void callOption(const Option* o, const char* p, const char* p2);

/// Adds a startup mode
void addStartupMode(const char* name, void(*func)());

/**
 * Finds a option by name that can accept value
 *
 * @param name the name of the option
 * @param value the value to be passed
 *
 * @return the first option matching name or NULL
 */
const Option* findOption(const char* name, const char* value1, const char* value2);
/**
 * Sets up the system to receive messages from an external client
 */
void addInterClientCommunicationRule();
/**
 * Should be to a xcb_client_message_event_t event.
 *
 * If the client message was sent to us, in our custom format, we will adjust the given parameters/run methods
 */
void receiveClientMessage(xcb_client_message_event_t* event);
/**
 * Sends a message to the running WM to set name = value or call name(value) depending if name is an variable or a method
 *
 * @param name
 * @param value
 * @param active
 */
void send(const char* name, const char* value);
void sendAs(const char* name, WindowID active, const char* value, const char* value2);


/**
 * @return 1 iff there are send messages whose receipt has not been confirmed
 */
bool hasOutStandingMessages(void);
/**
 * @return the number of calls to send
 */
uint32_t getNumberOfMessageSent();

/**
 * @return the return value of the last mesage sent
 */
int getLastMessageExitCode(void);


void initOptions();
ArrayList* getOptions();
#endif
