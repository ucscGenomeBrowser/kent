kentSrc = ../..
include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = preProcGgfAndy.o

preProcGgfAndy: $O 
	gcc -o ${BINDIR}/preProcGgfAndy $O $(MYLIBS)

clean::
	rm -f ${O}
