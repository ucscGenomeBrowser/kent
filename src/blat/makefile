kentSrc = ..
include ../inc/common.mk


all:
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

build: ${DESTDIR}${BINDIR}/${A}${EXE}
include $(kentSrc)/inc/userApp.mk
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
