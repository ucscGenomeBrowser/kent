include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = i16Find.o i16.o

both: i16Find i16Dump

i16Find: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/i16Find $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/i16Find${EXE}

i16Dump: i16Dump.o i16.o
	${CC} ${COPT} -o ${BINDIR}/i16Dump i16Dump.o i16.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/i16Dump${EXE}

clean:
	rm -f $O
