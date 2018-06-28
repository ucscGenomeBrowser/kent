kentSrc = ../..
include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredToMrna.o

genePredToMrna: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/genePredToMrna $O $(MYLIBS) $L

genePredToMrna.o: genePredToMrna.c 

clean:
	rm -f  *.o
