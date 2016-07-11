kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = callGraph.o

callGraph: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/callGraph $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/callGraph${EXE}

clean::
	rm -f ${O}
