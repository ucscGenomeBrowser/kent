include ../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFiberglass.o

hgFiberglass: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgFiberglass $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgFiberglass${EXE}

clean::
	rm -f ${O}
