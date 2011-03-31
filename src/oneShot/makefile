include ../inc/common.mk

L += -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = oneShot.o

oneShot: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/oneShot $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/oneShot${EXE}

clean::
	rm -f ${O}
