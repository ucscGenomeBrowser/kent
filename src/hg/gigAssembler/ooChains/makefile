include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooChains.o

ooChains: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooChains $O $(MYLIBS) $L


lib:
	cd ../../lib; make
	cd ../../../lib; make
