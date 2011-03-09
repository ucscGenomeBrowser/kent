include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = refSeqGet
O = refSeqVerInfo.o refSeqGet.o

refSeqGet: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

test::
	(cd tests && ${MAKE} test)

clean::
	rm -f ${A} ${O}
	(cd tests && ${MAKE} clean)

refSeqVerInfo.o: refSeqVerInfo.h refSeqVerInfo.c
refSeqGet.o:  refSeqVerInfo.h refSeqGet.c

