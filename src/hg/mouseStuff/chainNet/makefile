include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainNet.o

chainNet: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chainNet $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/chainNet${EXE}
