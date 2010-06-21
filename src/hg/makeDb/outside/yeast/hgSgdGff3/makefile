include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdGff3.o

hgSgdGff3: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSgdGff3 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSgdGff3${EXE}

clean::
	rm -f ${O}
