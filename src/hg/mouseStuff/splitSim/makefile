include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitSim.o

splitSim: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/splitSim $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/splitSim${EXE}

clean::
	rm -f ${O}
