kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rmskOoJobs.o

rmskOoJobs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rmskOoJobs $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rmskOoJobs${EXE}

fix: makeFix.o
	${CC} ${COPT} ${CFLAGS} -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	${CC} ${COPT} ${CFLAGS} -o test test.o $(MYLIBS) $L

clean::
	rm -f ${O} test.o makeFix.o
