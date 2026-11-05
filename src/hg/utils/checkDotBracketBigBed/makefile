kentSrc = ../../..
include ${kentSrc}/inc/common.mk

# a python program, so nothing to compile; all it does is install
all: install

install:
	chmod +x checkDotBracketBigBed
	rm -f ${DESTBINDIR}/checkDotBracketBigBed
	cp -p checkDotBracketBigBed ${DESTBINDIR}/checkDotBracketBigBed

compile:
	@true

test::
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean
