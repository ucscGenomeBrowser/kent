include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = geneScore.o genePredSingleCover.o

genePredSingleCover: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/genePredSingleCover $O $(MYLIBS) $L

geneScore.o: geneScore.c geneScore.h
genePredSingleCover.o: genePredSingleCover.c geneScore.h

test:
	(cd tests && ${MAKE} test)

savebak:
	savebak genePredSingleCover  *.[ch] makefile tests/makefile tests/input

clean:
	rm -f  *.o
