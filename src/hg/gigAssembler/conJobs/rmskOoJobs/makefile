include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rmskOoJobs.o

rmskOoJobs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/rmskOoJobs $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/rmskOoJobs${EXE}

fix: makeFix.o
	${CC} ${COPT} ${CFLAGS} -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	${CC} ${COPT} ${CFLAGS} -o test test.o $(MYLIBS) $L

