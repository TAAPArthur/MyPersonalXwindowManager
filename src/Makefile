CC ?= gcc
SHELL := /bin/bash
DEBUGGING_FLAGS := -g -rdynamic -O0
RELEASE_FLAGS ?= -O2
SPEED_TEST_FLAGS := -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable

IGNORED_FLAGS := -Wno-parentheses -Wno-missing-field-initializers -Wno-sign-compare
ERROR_FLAGS := -Wall -Werror
CFLAGS := ${ERROR_FLAGS} ${IGNORED_FLAGS} ${INJECT}

TESTFLAGS := ${CFLAGS} ${DEBUGGING_FLAGS} --coverage -lm -lscutest
LDFLAGS :=  -lX11 -lXi -lxcb -lxcb-xinput -lxcb-xtest -lxcb-ewmh -lxcb-icccm -lxcb-randr -lX11-xcb -lXtst -lxdo

LAYER0_SRCS :=  globals.c util/rect.h util/string-array.c util/logger.c util/debug.c settings.c
LAYER0_SRCS += xutil/xdebug.c xutil/test-functions.c xutil/properties.c xutil/window-properties.c xutil/xsession.c xutil/device-grab.c xutil/xerrors.c
LAYER1_SRCS := util/arraylist.c boundfunction.c
LAYER2_SRCS := slaves.c masters.c workspaces.c windows.c monitors.c
LAYER3_SRCS := system.c xevent.c devices.c bindings.c wmfunctions.c layouts.c
LAYER4_SRCS := wm-rules.c
LAYER5_SRCS := functions.c communications.c mpxmanager.c
LAYER6_SRCS := Extensions/ewmh.c Extensions/compatibility-rules.c Extensions/extra-rules.c Extensions/mpx.c Extensions/session.c Extensions/window-clone.c Extensions/containers.c

TEST_SRCS := ${LAYER1_SRCS} ${LAYER11_SRCS} ${LAYER2_SRCS} ${LAYER3_SRCS} ${LAYER4_SRCS} ${LAYER5_SRCS}
TOP_LAYER_SRCS := settings.c mpxmanager.c
BASE_SRCS := ${TOP_LAYER_SRCS} ${TEST_SRCS} ${LAYER0_SRCS} ${LAYER6_SRCS}
SRCS := ${BASE_SRCS} config.c

MEM_CHECK = valgrind -q  --error-exitcode=123
RUN_TEST = xvfb-run -w 0 -a $(if $(QUICK),  $(MEM_CHECK) $(1) 20,$(1))

NO_XRANDR ?= 0
ifeq ($(NO_XRANDR),1)
	CPPFLAGS += -DNO_XRANDR=1
endif

ifeq ($(QUICK),1)
	MEM_CHECK = valgrind -q  --error-exitcode=123
else ifeq ($(QUICK),2)
	MEM_CHECK = valgrind -q --leak-check=full --errors-for-leak-kinds=definite --show-leak-kinds=definite --error-exitcode=123
else ifeq ($(QUICK),3)
	MEM_CHECK = gdb
endif

ifeq ($(DEBUG),1)
  CFLAGS += ${DEBUGGING_FLAGS}
else ifeq ($(DEBUG),2)
  CFLAGS += ${DEBUGGING_FLAGS} -DNDEBUG
else ifeq ($(DEBUG),3)
  CFLAGS += ${RELEASE_FLAGS} -O2
else ifeq ($(DEBUG),4)
  CFLAGS += ${TESTFLAGS}
else
  CFLAGS += ${RELEASE_FLAGS} -DNDEBUG
endif


pkgname := mpxmanager

all: mpxmanager

install-headers:
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/" *.h
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/Extensions" Extensions/*.h
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/util" util/*.h
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/xutil" xutil/*.h

install: mpxmanager.sh mpxmanager-autocomplete.sh mpxmanager.1 libmpxmanager.a libmpx-patch.so install-headers
	install -m 0744 -Dt "$(DESTDIR)/usr/lib/" libmpxmanager.a
	install -m 0744 -Dt "$(DESTDIR)/usr/lib/" libmpx-patch.so
	install -m 0744 -Dt "$(DESTDIR)/usr/share/$(pkgname)/" sample-*
	install -m 0744 -Dt "$(DESTDIR)/usr/share/man/man1/" mpxmanager.1
	install -D -m 0755 "mpxmanager-autocomplete.sh" "$(DESTDIR)/etc/bash_completion.d/mpxmanager-autocomplete"
	install -D -m 0755 "mpxmanager.sh" "$(DESTDIR)/usr/bin/$(pkgname)"
uninstall:
	rm -f "$(DESTDIR)/usr/lib/libmpxmanager.a"
	rm -f "$(DESTDIR)/usr/lib/libmpx-patch.so"
	rm -frd "$(DESTDIR)/usr/include/$(pkgname)"
	rm -f "$(DESTDIR)/usr/share/$(pkgname)/sample-config.c"
	rm -f "$(DESTDIR)/usr/share/man/man1/mpxmanager.1"
	rm -f "$(DESTDIR)/etc/bash_completion.d/mpxmanager-autocomplete"
	rm -f "$(DESTDIR)/usr/bin/$(pkgname)"

preinstall: mpxmanager.1 mpxmanager-autocomplete.sh
	sed -i "s/pkgver=.*/pkgver='$$(./mpxmanager.sh -v)'/g" ../PKGBUILD

mpxmanager.1: mpxmanager.sh
	help2man -No mpxmanager.1 ./mpxmanager.sh

mpxmanager.sh: mpxmanager.sh.template
	cp $< $@
	sed -i "s/CFLAGS:-__place__holder/CFLAGS:-'${CFLAGS}'/g" mpxmanager.sh
	sed -i "s/LDFLAGS:-__place__holder/LDFLAGS:-'${LDFLAGS}'/g" mpxmanager.sh
	sed -i "s/GIT_HASH=.*/GIT_HASH=$$(git rev-parse --short HEAD)/g" mpxmanager.sh

mpxmanager-autocomplete.sh: mpxmanager-autocomplete.sh.template mpxmanager.c communications.c | mpxmanager
	cp $< $@
	sed -i "s/startOptions=.*/startOptions=\"$$(echo $$(./mpxmanager --list-start-options 2>&1 | sed 's/[,}{]//g' | awk '{print tolower($$0)}'| sed -E 's/((\w|-)+\W)/--\1/g'))\"/g"  mpxmanager-autocomplete.sh
	sed -i "s/sendOptions=.*/sendOptions=\"$$(echo $$(./mpxmanager --list-options 2>&1 | sed 's/[,}{]//g' ))\"/g"  mpxmanager-autocomplete.sh

mpxmanager: $(BASE_SRCS:.c=.o) Tests/test-config.o
	${CC} ${CFLAGS} $^ -o $@ ${LDFLAGS}


libmpxmanager.so: $(BASE_SRCS:.c=.o)
	${CC} ${CFLAGS} -fPIC -shared -o libmpxmanager.so $^ ${CFLAGS} ${LDFLAGS}

libmpxmanager.a: $(BASE_SRCS:.c=.o)
	ar rcs libmpxmanager.a $^

libmpx-patch.so: Hacks/mpx-patch.c
	${CC} ${CFLAGS} -fPIC -shared -o $@ $^ ${LDFLAGS}


test: CFLAGS := ${TESTFLAGS}
test: unitTest.out functional.out unitTestExtensions.out endToEndTest.out # doc-check

functionalTest: CFLAGS := ${TESTFLAGS}
functionalTest: $(BASE_SRCS:.c=.o) Tests/tester.o Tests/functional_test.o Tests/compatibility_test.o |libmpx-patch.so
	${CC} ${CFLAGS} $^ -o $@  ${TESTFLAGS} ${TESTLIBS} ${LDFLAGS}

functional.out: functionalTest
	LD_PRELOAD=$$(pwd)/libmpx-patch.so $(call RUN_TEST, ./$^ )

endToEndTest: CFLAGS := ${TESTFLAGS}
endToEndTest: $(BASE_SRCS:.c=.o) Tests/tester.o Tests/endToEnd_test.o |libmpx-patch.so
	${CC} ${CFLAGS} $^ -o $@  ${TESTFLAGS} ${TESTLIBS} ${LDFLAGS}

endToEndTest.out: CFLAGS := ${TESTFLAGS}
endToEndTest.out: endToEndTest mpxmanager
	$(call RUN_TEST, ./$< )
	$(if $(TEST_FILE)$(TEST_FUNC),exit 1)
	touch $@

unitTest: CFLAGS := ${TESTFLAGS}
unitTest: Tests/tester.o   $(patsubst %.c,Tests/%_unit.o,$(TEST_SRCS)) $(TEST_SRCS:.c=.o) $(LAYER0_SRCS:.c=.o)
	${CC} ${CFLAGS} $^ -o $@  ${TESTFLAGS} ${TESTLIBS} ${LDFLAGS}

unitTest.out: unitTest
	$(call RUN_TEST, ./unitTest )
	$(if $(TEST_FUNC),exit 1)

unitTestExtensions: CFLAGS := ${TESTFLAGS}
unitTestExtensions: Tests/tester.o   $(patsubst %.c,Tests/%_unit.o,$(LAYER6_SRCS)) $(LAYER6_SRCS) $(TEST_SRCS:.c=.o) $(LAYER0_SRCS:.c=.o)
	${CC} ${CFLAGS} $^ -o $@  ${TESTFLAGS} ${TESTLIBS} ${LDFLAGS}

unitTestExtensions.out: unitTestExtensions
	$(call RUN_TEST, ./$^)
	$(if $(TEST_FUNC),exit 1)


code_coverage.out: unitTest
	gcov -mr *
	grep "#####:" *c.gcov > $@
	grep "#####:" *c.gcov |wc -l

package:
	+$(MAKE) -j1 -C .. $@
doc:
	+$(MAKE) -j1 -C .. $@
doc-check:
	+$(MAKE) -j1 -C .. $@


.PHONY: test *.out all clean doc install package

.DELETE_ON_ERROR:

clean-test:
	find . \( -name "*.out" \) -exec rm -f {} \;
clean:
	rm -f unitTest vgcore* *gc?? mpxmanager *.a *.so mpxmanager-autocomplete.sh mpxmanager.sh
	find . \( -name "*.orig" -o -name "*.gc??" -o -name "*.out" -o -name "*.o" \) -exec rm -f {} \;
