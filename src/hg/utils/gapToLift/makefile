include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gapToLift.o

gapToLift: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gapToLift $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gapToLift${EXE}

compile: $O
	${CC} ${COPT} -o gapToLift $O ${MYLIBS} $L

clean:
	rm -f $O gapToLift
