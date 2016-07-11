kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netContigs.o

netContigs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netContigs $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netContigs${EXE}

clean::
	rm -f ${O}
