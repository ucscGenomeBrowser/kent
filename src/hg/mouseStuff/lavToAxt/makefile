include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = lavToAxt.o

lavToAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/lavToAxt $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/lavToAxt${EXE}
