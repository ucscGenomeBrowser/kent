include ../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSoftPromoter.o

hgSoftPromoter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSoftPromoter $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSoftPromoter${EXE}

clean::
	rm -f ${O}
