include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = aNotB.o

aNotB: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/aNotB $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/aNotB${EXE}

clean::
	rm -f ${O}
