include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

PROG = regionAgp

O = ${PROG}.o

regionAgp: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/${PROG} $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/${PROG}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test
