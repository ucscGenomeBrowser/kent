kentSrc = ..

# use recursive make to compile 32-bit and 64-bit index versions
# if IDXSIZE=64 is passed in, we get the 64-bit version

all:
	${MAKE} build
	${MAKE} build IDXSIZE=huge

ifeq (${IDXSIZE},huge)
A = gfServerHuge
CFLAGS += -DGFSERVER_HUGE
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLibHuge.a
else
A = gfServer
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
endif

build: ${DESTDIR}${BINDIR}/${A}${EXE}
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

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
