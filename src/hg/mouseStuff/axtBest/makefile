include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBest.o

axtBest: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtBest $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtBest${EXE}

clean::
	rm -f ${O}
