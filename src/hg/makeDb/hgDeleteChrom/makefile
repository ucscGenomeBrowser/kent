include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgDeleteChrom.o 

hgDeleteChrom: $O ../../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgDeleteChrom $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
