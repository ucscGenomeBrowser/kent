include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFiberglass.o

hgFiberglass: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgFiberglass $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgFiberglass${EXE}
