include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftPromoHits.o

liftPromoHits: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/liftPromoHits $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/liftPromoHits${EXE}
