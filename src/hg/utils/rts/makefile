kentSrc = ../../..
include ${kentSrc}/inc/common.mk

# Scripts to install into ${BINDIR} (on the user's PATH).
SCRIPT_FILES = \
	rtsUpdate

all: $(SCRIPT_FILES:%=%.install)

%.install:
	chmod +x $*
	rm -f ${DESTDIR}${BINDIR}/$*
	cp -p $* ${DESTDIR}${BINDIR}/$*

clean:
	@true
