include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

PROG = ${BINDIR}/pslSwap${EXE}
OBJS = pslSwap.o

all: ${PROG}

${PROG}: pslSwap
	@${MKDIR} ${BINDIR}
	cp -f pslSwap ${PROG}
	${STRIP} ${PROG}${EXE}

pslSwap: ${OBJS} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslSwap ${OBJS} ${MYLIBS} ${MYSQLLIBS} $L

test:
	cd tests && ${MAKE} test

clean:
	rm -f pslSwap ${OBJS}
	cd tests && ${MAKE} clean

savebak:
	savebak pslSwap makefile *.c tests/makefile tests/input
