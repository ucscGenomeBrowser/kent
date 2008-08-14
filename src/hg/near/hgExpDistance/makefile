include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

hgExpDistance: hgExpDistance.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance $@.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance${EXE}

hgExpDistance_fm: hgExpDistance_fm.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fm $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fm${EXE}

hgExpDistance_fq: hgExpDistance_fq.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fq $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fq${EXE}

test:
	hgExpDistance hg17 gnfAtlas2 gnfAtlas2Exp hgExpDistance_TEST  -verbose=2

clean::
	rm -f hgExpDistance*.o 
