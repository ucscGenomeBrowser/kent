include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslCrudeCmp.o

pslCrudeCmp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslCrudeCmp $O $(MYLIBS) $L
