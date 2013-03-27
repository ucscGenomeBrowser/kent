include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = bestThreshold.o
A = bestThreshold

${A}: ${O}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

install:: compile
	rm -f ${DESTDIR}${BINDIR}/${A}${EXE}
	cp -p ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

test: compile
	./${A}${EXE} in.good in.bad > out
	diff expected out

clean::
	rm -f ${O} ${A}${EXE} out
