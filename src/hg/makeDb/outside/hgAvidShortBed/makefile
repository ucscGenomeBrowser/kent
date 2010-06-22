include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgAvidShortBed.o

hgAvidShortBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgAvidShortBed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgAvidShortBed${EXE}

clean::
	rm -f ${O}
