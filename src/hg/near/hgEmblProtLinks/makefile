include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgEmblProtLinks.o

hgEmblProtLinks: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgEmblProtLinks $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgEmblProtLinks${EXE}

clean::
	rm -f ${O}
