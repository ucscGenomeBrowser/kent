include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFilter.o

faFilter: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faFilter $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faFilter${EXE}
