kentSrc = ..

# use recursive make to compile 32-bit and 64-bit index versions
# if IDXSIZE=64 is passed in, we get the 64-bit version


ifeq (${IDXSIZE},huge)
A = gfServerHuge
CFLAGS += -DGFSERVER_HUGE
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLibHuge.a
else
A = gfServer
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

build_both:
	${MAKE} do_build
	${MAKE} do_build IDXSIZE=huge

do_build: default

# this must be include here, after the above setup but before the build target
include $(kentSrc)/inc/userApp.mk

gfServerHuge.c: gfServer.c
	ln -sf $< $@

test::
	cd tests && ${MAKE} test

clean::
	${MAKE} clean_idxsize
	${MAKE} clean_idxsize IDXSIZE=HUGE
	cd tests && ${MAKE} clean

clean_idxsize:
	rm -fr ${O} gfServerHuge.c
