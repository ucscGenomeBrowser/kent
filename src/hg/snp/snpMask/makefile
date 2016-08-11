kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

all: snpMaskSingle snpMaskAddInsertions snpMaskCutDeletions

snpMaskSingle: snpMaskSingle.o ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/snpMaskSingle snpMaskSingle.o ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/snpMaskSingle${EXE}

snpMaskAddInsertions: snpMaskAddInsertions.o ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/snpMaskAddInsertions snpMaskAddInsertions.o ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/snpMaskAddInsertions${EXE}

snpMaskCutDeletions: snpMaskCutDeletions.o ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/snpMaskCutDeletions snpMaskCutDeletions.o ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/snpMaskCutDeletions${EXE}

O = snpMapToExon.o snpMaskChrom.o snpMaskFlank.o \
    snpMaskFlankSubset.o snpMaskGenes.o \
    seqWithInsertions.o seqWithoutDeletions.o

old:snpMapToExon snpMaskChrom snpMaskFlank \
    snpMaskFlankSubset snpMaskGenes \
    seqWithInsertions seqWithoutDeletions

snpMapToExon: snpMapToExon.o ${MYLIBS}
	${CC} ${COPT} -O -o snpMapToExon snpMapToExon.o ${MYLIBS} $L

snpMaskChrom: snpMaskChrom.o ${MYLIBS}
	${CC} ${COPT} -O -o snpMaskChrom snpMaskChrom.o ${MYLIBS} $L

snpMaskFlank: snpMaskFlank.o ${MYLIBS}
	${CC} ${COPT} -O -o snpMaskFlank snpMaskFlank.o ${MYLIBS} $L

snpMaskFlankSubset: snpMaskFlankSubset.o ${MYLIBS}
	${CC} ${COPT} -O -o snpMaskFlankSubset snpMaskFlankSubset.o ${MYLIBS} $L

snpMaskGenes: snpMaskGenes.o ${MYLIBS}
	${CC} ${COPT} -O -o snpMaskGenes snpMaskGenes.o ${MYLIBS} $L

seqWithInsertions: seqWithInsertions.o ${MYLIBS}
	${CC} ${COPT} -O -o seqWithInsertions seqWithInsertions.o ${MYLIBS} $L

seqWithoutDeletions: seqWithoutDeletions.o ${MYLIBS}
	${CC} ${COPT} -O -o seqWithoutDeletions seqWithoutDeletions.o ${MYLIBS} $L

clean::
	rm -f ${O}
