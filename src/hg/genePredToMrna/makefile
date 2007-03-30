include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredToMrna.o

genePredToMrna: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/genePredToMrna $O $(MYLIBS) $L

genePredToMrna.o: genePredToMrna.c 

clean:
	rm -f  *.o
