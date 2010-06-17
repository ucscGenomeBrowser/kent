include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netToBed.o

netToBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netToBed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netToBed${EXE}

clean::
	rm -f ${O}
