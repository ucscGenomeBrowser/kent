include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = barger.o 

barger: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/barger $O $(MYLIBS) $L


lib:
	cd ../../lib; make
	cd ../../../lib; make
