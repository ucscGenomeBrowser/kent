kentSrc = ../..
include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A1 = addAveMedScoreToPsls
A2 = createSageSummary

all::	compile
	rm -f ${DESTDIR}${BINDIR}/${A1}${EXE}
	mv ${A1}${EXE} ${DESTDIR}${BINDIR}/
	rm -f ${DESTDIR}${BINDIR}/${A2}${EXE}
	mv ${A2}${EXE} ${DESTDIR}${BINDIR}/

compile:: ${A1}.o ${A2}.o
	${CC} ${COPT} ${CFLAGS} -o ${A1}${EXE} ${A1}.o ${MYLIBS} ${L}
	${CC} ${COPT} ${CFLAGS} -o ${A2}${EXE} ${A2}.o ${MYLIBS} ${L}

clean::
	rm -f ${A1}.o ${A2}.o ${A1} ${A2}
