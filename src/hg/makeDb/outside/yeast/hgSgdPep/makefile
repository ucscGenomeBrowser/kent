include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdPep.o

hgSgdPep: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSgdPep $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSgdPep${EXE}

clean::
	rm -f ${O}
