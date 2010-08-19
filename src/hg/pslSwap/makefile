include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pslSwap
O = pslSwap.o

all: ${A}

${A}: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test: compile
	cd tests && ${MAKE} test
	rm -f ${A}${EXE}

clean:
	rm -f pslSwap ${OBJS}
	cd tests && ${MAKE} clean

savebak:
	savebak pslSwap makefile *.c tests/makefile tests/input
