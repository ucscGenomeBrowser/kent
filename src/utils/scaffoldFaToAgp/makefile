include ../../inc/common.mk

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

PROG = scaffoldFaToAgp

O = ${PROG}.o 

${PROG}: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/${PROG} $O ../../lib/$(MACHTYPE)/jkhgap.a ../../lib/$(MACHTYPE)/jkweb.a

