include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslSwap.o

pslSwap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/pslSwap $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/pslSwap${EXE}
