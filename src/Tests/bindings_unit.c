#include <stdlib.h>
#include "../bindings.h"
#include "../devices.h"
#include "../globals.h"
#include "../windows.h"
#include "test-event-helper.h"
#include "test-mpx-helper.h"
#include "tester.h"


SCUTEST_SET_ENV(createXSimpleEnv, cleanupXServer);

SCUTEST_ITER(test_grab_ungrab_bindings, 2) {
    Binding sampleBinding = {0, XK_A, {incrementCount}, .flags = {.noGrab = 1}};
    assertEquals(0, grabBinding(&sampleBinding, 0));
    assert(sampleBinding.detail);
    assertEquals(0, grabBinding(&sampleBinding, 1));
}

#define OTHER_MODE 1
static Binding sampleBindings[] = {
    {0, 1, {incrementCount}},
    {0, 3, {fail} },
    {0, 3, {incrementCount}, .flags.mode = OTHER_MODE},
    {0, 3, {incrementCount}, .flags.mode = ANY_MODE},
    {0, 1, {incrementCount}},
    {
        0, 2, {incrementCount}, .chainMembers = CHAIN_MEM(
        {0, 2, {incrementCount}},
        {0, 3, {incrementCount}, .flags.popChain = 1},
        )
    },
    {
        0, 4, {incrementCount}, .flags.mask = 1, .chainMembers = CHAIN_MEM(
        {0, 4, {incrementCount}},
        {0, 3, {incrementCount}, .flags.popChain = 1},
        )
    },
};

static void setupBindings() {
    createXSimpleEnv();
    addBindings(sampleBindings, LEN(sampleBindings));
    grabAllBindings(NULL, 0, 0);
}
SCUTEST_SET_ENV(setupBindings, cleanupXServer);
SCUTEST_ITER(test_addBindings, 2) {
    for(int i = 0; i < LEN(sampleBindings); i++)
        assert(sampleBindings[i].detail);
}

SCUTEST(test_check_bindings) {
    BindingEvent event = {0, 1};
    checkBindings(&event);
    assertEquals(2, getCount());
}

SCUTEST(test_check_bindings_mode) {
    setActiveMode(OTHER_MODE);
    dumpMaster(getActiveMaster());
    BindingEvent event = {0, 3};
    checkBindings(&event);
    assertEquals(2, getCount());
}

SCUTEST(test_check_bindings_short_cricuit) {
    BindingEvent event = {0, 1};
    sampleBindings[0].flags.shortCircuit = 1;
    checkBindings(&event);
    assertEquals(1, getCount());
}

SCUTEST(test_check_bindings_global_chain) {
    createMasterDevice("test");
    BindingEvent event = {0, 4};
    BindingEvent eventEnd = {0, 3};
    FOR_EACH(Master*, master, getAllMasters()) {
        setActiveMaster(master);
        checkBindings(&event);
    }
    assertEquals(2, getCount());
    checkBindings(&eventEnd);
    assertEquals(3, getCount());
}

SCUTEST(test_check_bindings_chain) {
    BindingEvent event = {0, 2};
    BindingEvent eventEnd = {0, 3};
    checkBindings(&event);
    assertEquals(2, getCount());
    assertEquals(1, getActiveMaster()->bindings.size);
    checkBindings(&event);
    assertEquals(3, getCount());
    assertEquals(1, getActiveMaster()->bindings.size);
    checkBindings(&eventEnd);
    assertEquals(4, getCount());
    assertEquals(0, getActiveMaster()->bindings.size);
}

