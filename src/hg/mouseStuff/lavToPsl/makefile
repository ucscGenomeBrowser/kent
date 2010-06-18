include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = lavToPsl.o

lavToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/lavToPsl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/lavToPsl${EXE}

clean::
	rm -f ${O}
