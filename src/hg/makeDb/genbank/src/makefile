GBROOT = ..
include ${GBROOT}/common.mk

SUBDIRS = lib download process align copy dbload \
	selectWithPsl gbSanity mgc gbGetSeqs gbConf repair ccds orfeome extFileUpdate refSeqCheck\
	lsSnpPdb

all: ${SUBDIRS:%=%.suball}
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
	rm -rf ${BINDIR}

%.subclean:
	${MAKE} -C $* clean

