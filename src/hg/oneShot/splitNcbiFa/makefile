include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitNcbiFa.o

splitNcbiFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/splitNcbiFa $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/splitNcbiFa${EXE}
