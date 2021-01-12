kentSrc = ..

# use recursive make to compile 32bit and 64bit index versions
# if IDXSIZE=64 is passed in, we get the 64bit version

all:
	${MAKE} build
	${MAKE} build IDXSIZE=64

ifeq (${IDXSIZE},64)
A = gfServer64
CFLAGS += -DGFSERVER64
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib64.a
else
A = gfServer
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

build: ${DESTDIR}${BINDIR}/${A}${EXE}
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

gfServer64.c: gfServer.c
	ln -sf $< $@

test::
	cd tests && ${MAKE} test

clean::
	${MAKE} clean_idxsize
	${MAKE} clean_idxsize IDXSIZE=64
	cd tests && ${MAKE} clean

clean_idxsize:
	rm -fr ${O} gfServer64.c
