GBROOT = ..
include ${GBROOT}/common.mk

SUBDIRS = lib download process align copy dbload \
	selectWithPsl gbSanity mgc gbGetSeqs gbFaCheck

all: ${SUBDIRS:%=%.suball} build-tools
%.suball:
	${MAKE} -C $* all

clean: ${SUBDIRS:%=%.subclean}
	rm -rf ${BINDIR}

%.subclean:
	${MAKE} -C $* clean


# build copies of the tools needed and install in our bindir
# build other programs that we use:
PROGS = ${BINARCH}/blat \
	${BINARCH}/pslReps \
	${BINARCH}/liftUp \
	${BINARCH}/polyInfo \
	${BINARCH}/pslIntronsOnly
build-tools: ${PROGS}

ABSROOT = $(shell cd ${GBROOT} && pwd)
ABSKENTLIB = $(shell cd ${MYLIBDIR} && pwd)

${BINARCH}/blat:
	(cd ${KENT}/blat && ${MAKE} MYLIBDIR=${ABSKENTLIB} HOME=${ABSROOT})
${BINARCH}/pslReps:
	(cd ${KENT}/hg/pslReps && ${MAKE} HOME=${ABSROOT} MYLIBDIR=${ABSKENTLIB})
${BINARCH}/liftUp:
	(cd ${KENT}/hg/liftUp && ${MAKE} HOME=${ABSROOT} MYLIBDIR=${ABSKENTLIB})
${BINARCH}/polyInfo:
	(cd ${KENT}/hg/geneBounds/lib && ${MAKE} MYLIBDIR=${ABSKENTLIB})
	(cd ${KENT}/hg/geneBounds/polyInfo && ${MAKE} HOME=${ABSROOT} MYLIBDIR=${ABSKENTLIB})
${BINARCH}/pslIntronsOnly:
	(cd ${KENT}/hg/pslIntronsOnly && ${MAKE} HOME=${ABSROOT} MYLIBDIR=${ABSKENTLIB})

