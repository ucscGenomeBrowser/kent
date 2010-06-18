include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftPromoHits.o

liftPromoHits: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/liftPromoHits $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/liftPromoHits${EXE}

clean::
	rm -f ${O}
