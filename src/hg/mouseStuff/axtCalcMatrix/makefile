include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtCalcMatrix.o

axtCalcMatrix: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtCalcMatrix $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtCalcMatrix${EXE}

clean::
	rm -f ${O}
