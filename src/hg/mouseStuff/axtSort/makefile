include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSort.o

axtSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtSort $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtSort${EXE}

clean::
	rm -f ${O}
