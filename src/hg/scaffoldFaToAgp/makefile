include ../../inc/common.mk

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

INSTALLDIR = $(HOME)/bin/$(MACHTYPE)

PROG = scaffoldFaToAgp

O = ${PROG}.o 

${PROG}: $(O)
	gcc -o $(INSTALLDIR)/${PROG} $O ../../lib/$(MACHTYPE)/jkhgap.a ../../lib/$(MACHTYPE)/jkweb.a

test: $(INSTALLDIR)/$(PROG)
	cd test && csh runtest

clean: 
	rm -f $(O)
