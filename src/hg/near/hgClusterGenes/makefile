include ../../../inc/common.mk

L += -lm  $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgClusterGenes.o

hgClusterGenes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgClusterGenes $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/hgClusterGenes${EXE}

clean::
	rm -f ${O}
