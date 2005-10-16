include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O =  geneBins.o mkMafFrames.o finishMafFrames.o genePredToMafFrames.o

genePredToMafFrames: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/genePredToMafFrames $O $(MYLIBS) $L

geneBins.o: geneBins.h geneBins.c
mkMafFrames.o: mkMafFrames.c mkMafFrames.h geneBins.h frameIncr.h
finishMafFrames.o: finishMafFrames.c finishMafFrames.h geneBins.h frameIncr.h
genePredToMafFrames.o: genePredToMafFrames.c geneBins.h mkMafFrames.h finishMafFrames.h

test:
	(cd tests && ${MAKE} test)

savebak:
	savebak genePredToMafFrames  *.[ch] makefile tests/makefile tests/input

clean:
	rm -f  *.o
