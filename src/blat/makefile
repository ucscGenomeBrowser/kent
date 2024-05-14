kentSrc = ..
include ${kentSrc}/inc/common.mk


compile::
	${MAKE} build
	${MAKE} build IDXSIZE=huge

ifeq (${IDXSIZE},huge)
A = blatHuge
CFLAGS += -DGFSERVER_HUGE
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLibHuge.a
else
A = blat
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

# this must be include here, after the above setup but before the build target
include $(kentSrc)/inc/userApp.mk

build: ${DESTDIR}${BINDIR}/${A}${EXE}

L += -lm $(SOCKETLIB)

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
