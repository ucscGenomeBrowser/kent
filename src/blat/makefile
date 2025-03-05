kentSrc = ..
include ${kentSrc}/inc/common.mk


ifeq (${IDXSIZE},huge)
A = blatHuge
CFLAGS += -DGFSERVER_HUGE
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLibHuge.a
else
A = blat
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

build_both:
	${MAKE} do_build
	${MAKE} do_build IDXSIZE=huge


do_build: default

# this must be include here, after the above setup but before the build target
include $(kentSrc)/inc/userApp.mk

blatHuge.c: blat.c
	ln -sf $< $@

tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h

test::
	cd test && ${MAKE}

clean::
	${MAKE} clean_idxsize
	${MAKE} clean_idxsize IDXSIZE=huge
	cd test && ${MAKE} clean

clean_idxsize:
	rm -fr ${O} blatHuge.c
