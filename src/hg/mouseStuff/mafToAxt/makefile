include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = mafToAxt.o

mafToAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafToAxt $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/mafToAxt${EXE}

clean::
	rm -f ${O}
