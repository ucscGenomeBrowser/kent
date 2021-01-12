kentSrc = ..
include ../inc/common.mk


all:
	${MAKE} build
	${MAKE} build IDXSIZE=64

ifeq (${IDXSIZE},64)
A = blat64
CFLAGS += -DGFSERVER64
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib64.a
else
A = blat
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

build: ${DESTDIR}${BINDIR}/${A}${EXE}
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

blat64.c: blat.c
	ln -sf $< $@

tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h

test::
	cd test && ${MAKE}

clean::
	${MAKE} clean_idxsize
	${MAKE} clean_idxsize IDXSIZE=64
	cd test && ${MAKE} clean

clean_idxsize:
	rm -fr ${O} blat64.c
