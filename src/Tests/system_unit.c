#include <scutest/tester.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../util/arraylist.h"
#include "../util/time.h"
#include "../globals.h"
#include "../util/logger.h"
#include "../masters.h"
#include "../system.h"
#include "test-mpx-helper.h"

SCUTEST(test_failed_exec_spawn) {
    SHELL = "/dev/null";
    assertEquals(SYS_CALL_FAILED, spawnAndWait(""));
}

SCUTEST_ERR(test_failed_exec_restart, SYS_CALL_FAILED) {
    suppressOutput();
    const char* arr[] = {"/dev/null", NULL};
    passedArguments = arr;
    restart();
}

SCUTEST_ITER(test_spawn_child, 2) {
    assertEquals(10, waitForChild(_i ? spawnChild("exit 10") : spawnPipeChild("exit 10", REDIRECT_BOTH)));
}

SCUTEST(test_child_can_outlive_parent) {
    int fds[4];
    int buffer = 0;
    pipe(fds);
    pipe(fds + 2);
    if(!fork()) {
        if(!spawnChild(NULL)) {
            int result = read(fds[0], &buffer, sizeof(buffer));
            assertEquals(result, sizeof(buffer));
            write(fds[3], &buffer, sizeof(buffer));
        }
        exit(10);
    }
    assertEquals(waitForChild(0), 10);
    write(fds[1], &buffer, sizeof(buffer));
    int result = read(fds[2], &buffer, sizeof(buffer));
    assertEquals(result, sizeof(buffer));
}

SCUTEST(test_no_zombies) {
    for(int i = 0; i < 10; i++) {
        spawn("exit 0");
    }
    assertEquals(0, spawnAndWait("exit 0"));
    assertEquals(-1, waitpid(-1, NULL, WNOHANG));
}

SCUTEST(test_spawn_wait) {
    assert(spawnAndWait("exit 0") == 0);
    assert(spawnAndWait("exit 1") == 1);
    assert(spawnAndWait("exit 122") == 122);
}


SCUTEST_SET_ENV(set_handlers, simpleCleanup);

SCUTEST(test_spawn_pipe_input_only) {
    const char values[2][256] = {"1", "2"};
    char buffer[2][256] = {0};
    if(!spawnPipeChild(NULL, REDIRECT_CHILD_INPUT_ONLY)) {
        read(STDIN_FILENO, buffer[0], LEN(buffer[0]));
        assert(strcmp(buffer[0], values[0]) == 0);
        exit(0);
    }
    assert(STATUS_FD);
    dprintf(STATUS_FD, values[0]);
    assertEquals(waitForChild(0), 0);
    assert(dprintf(STATUS_FD, values[0]) < 1);
    assert(STATUS_FD == 0);
}

SCUTEST(test_spawn_pipe_output_only) {
    const char values[2][256] = {"1", "2"};
    char buffer[2][256] = {0};
    if(!spawnPipeChild(NULL, REDIRECT_CHILD_OUTPUT_ONLY)) {
        printf(values[1]);
        exit(0);
    }
    assert(STATUS_FD_READ);
    read(STATUS_FD_READ, buffer[1], LEN(buffer[1]));
    assert(strcmp(buffer[1], values[1]) == 0);
    assertEquals(waitForChild(0), 0);
}

SCUTEST_ITER(spawn_pipe_death, 4) {
    int pid = spawnPipeChild(NULL, (ChildRedirection)_i);
    if(!pid) {
        pause();
        exit(1);
    }
    kill(pid, SIGKILL);
    assertEquals(waitForChild(pid), 9);
}

SCUTEST(spawn_pipe_close) {
    int pid = spawnPipeChild(NULL, REDIRECT_BOTH);
    if(!pid) {
        exit(0);
    }
    assert(close(STATUS_FD_EXTERNAL_READ));
    assert(close(STATUS_FD_EXTERNAL_WRITE));
    assert(!close(STATUS_FD));
    assert(!close(STATUS_FD_READ));
    assertEquals(waitForChild(pid), 0);
}
SCUTEST(spawn_pipe_read_and_close) {
    spawnPipe("head -n 1", REDIRECT_CHILD_INPUT_ONLY);
    createSigAction(SIGPIPE, SIG_IGN);
    while(write(STATUS_FD, "\n\n", 3) != -1);
    assert(!close(STATUS_FD));
    assert(!close(STATUS_FD_READ));
}
SCUTEST(spawn_pipe_child_close) {
    if(!spawnPipeChild(NULL, REDIRECT_BOTH)) {
        exit(0);
    }
    assert(waitForChild(0) == 0);
    dprintf(STATUS_FD, "0\n");
}

SCUTEST_SET_ENV(NULL, simpleCleanup);
SCUTEST_ITER(spawn_env, 2) {
    bool large = _i;
    createSimpleEnv();
    LD_PRELOAD_INJECTION = 1;
    WindowInfo* winInfo = addFakeWindowInfo(large ? 1 << 31 : 1);
    onWindowFocus(winInfo->id);
    addFakeMonitor((Rect) {0, 0, 1, (uint16_t)(large ? -1 : 1)});
    assignUnusedMonitorsToWorkspaces();
    int pid = spawnChild(NULL);
    if(!pid) {
        char buffer[255];
        sprintf(buffer, "%d", getActiveMasterKeyboardID());
        assert(getenv("_WIN_ID"));
        assert(getenv("_WIN_TITLE"));
        assert(getenv("_WIN_CLASS"));
        assert(getenv("_WIN_INSTANCE"));
        assert(getenv("_ROOT_X"));
        assert(getenv(DEFAULT_KEYBOARD_ENV_VAR_NAME));
        assert(getenv(DEFAULT_POINTER_ENV_VAR_NAME));
        assert(getenv("LD_PRELOAD"));
        simpleCleanup();
        quit(0);
    }
    assertEquals(waitForChild(pid), 0);
}

SCUTEST(test_quit) {
    quit(0);
    assert(0);
}
SCUTEST_ERR(test_quit_err, 1) {
    quit(1);
}

SCUTEST_ITER_ERR(test_restart, 2, 10) {
    const char* const args[] = {SHELL, "-c", "exit 10", NULL};
    passedArguments = args;
    numPassedArguments = LEN(args) - 1;
    if(_i)
        restart();
    else
        raise(SIGUSR1);
    assert(0);
}
SCUTEST_SET_ENV(set_handlers, simpleCleanup);
SCUTEST_ERR(seg_fault, SIGSEGV) {
    setLogLevel(LOG_LEVEL_NONE);
    raise(SIGSEGV);
}
SCUTEST_ERR(sig_term, SIGTERM) {
    setLogLevel(LOG_LEVEL_NONE);
    raise(SIGTERM);
}
SCUTEST_ERR(assert_fail, SIGABRT) {
    setLogLevel(LOG_LEVEL_NONE);
    assert(0);
}
