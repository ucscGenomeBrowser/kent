include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${HOME}/bin/${MACHTYPE}

PROG = regionAgp

O = ${PROG}.o

regionAgp: $O $(MYLIBS)
	gcc -ggdb -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/${PROG}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test
