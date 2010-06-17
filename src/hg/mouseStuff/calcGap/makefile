include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = calcGap.o

calcGap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/calcGap $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/calcGap${EXE}

clean::
	rm -f ${O}
