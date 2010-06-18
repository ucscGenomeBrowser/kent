include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netFilter.o

netFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netFilter $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netFilter${EXE}

clean::
	rm -f ${O}
