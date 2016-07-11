kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cdnaOnOoJobs.o

cdnaOnOoJobs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cdnaOnOoJobs $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/cdnaOnOoJobs${EXE}

fix: makeFix.o
	${CC} ${COPT} ${CFLAGS} -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	${CC} ${COPT} ${CFLAGS} -o test test.o $(MYLIBS) $L

clean::
	rm -f ${O} test.o
