include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

O = blat2p.o

blat2p: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/blat2p $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/blat2p${EXE}
