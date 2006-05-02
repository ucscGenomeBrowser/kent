include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = arrayCheck.o snpMapToExon.o snpMaskChrom.o snpMaskFlank.o \
    snpMaskFlankSubset.o snpMaskGenes.o

all: arrayCheck snpMapToExon snpMaskChrom snpMaskFlank \
    snpMaskFlankSubset snpMaskGenes

arrayCheck: arrayCheck.o ${MYLIBS}
	${CC} ${COPT} -O -o arrayCheck arrayCheck.o ${MYLIBS} $L

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

