include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtIndex.o

axtIndex: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtIndex $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtIndex${EXE}

clean::
	rm -f ${O}
