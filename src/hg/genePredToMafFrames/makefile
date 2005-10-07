include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O =  geneBins.o genePredToMafFrames.o

genePredToMafFrames: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/genePredToMafFrames $O $(MYLIBS) $L

geneBins.o: geneBins.h geneBins.c
genePredToMafFrames.o: genePredToMafFrames.c geneBins.h

test:
	(cd tests && ${MAKE} test)

savebak:
	savebak genePredToMafFrames  *.[ch] makefile tests/makefile tests/input

clean:
	rm -f  *.o
