GBROOT = ..
include ${GBROOT}/common.mk

SUBDIRS = lib download process align copy dbload \
	selectWithPsl gbSanity mgc gbGetSeqs repair ccds

all: ${SUBDIRS:%=%.suball}
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
	rm -rf ${BINDIR}

%.subclean:
	${MAKE} -C $* clean

