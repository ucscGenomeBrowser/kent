kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

progs = bin/gencodeGxfToAttrs \
	bin/ensToUcscMkLift \
	bin/gencodeGenerateTrackDbs

all::

test::
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean

lint:

	python3 -m flake8 ${progs} lib
