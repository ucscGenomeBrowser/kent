include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

#O = hgExpDistance.o
O = hgExpDistance_fm.o
#O = hgExpDistance_fq1.o
#O = hgExpDistance_fq2.o

hgExpDistance: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance${EXE}

hgExpDistance_fm: $O ${MYLIBS}
	${CC} ${COPT} -pthread -o ${BINDIR}/hgExpDistance_fm $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance_fm${EXE}

hgExpDistance_fq1: $O ${MYLIBS}
	${CC} ${COPT} -pthread -o ${BINDIR}/hgExpDistance_fq1 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance_fq1${EXE}

hgExpDistance_fq2: $O ${MYLIBS}
	${CC} ${COPT} -pthread -o ${BINDIR}/hgExpDistance_fq2 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance_fq2${EXE}

clean::
	rm -f ${O} hgExpDistance
