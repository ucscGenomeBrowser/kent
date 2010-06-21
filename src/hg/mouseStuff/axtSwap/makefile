include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSwap.o

axtSwap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtSwap $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtSwap${EXE}

clean::
	rm -f ${O}
