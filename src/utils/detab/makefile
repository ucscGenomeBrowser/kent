include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = detab.o

detab: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/detab $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/detab${EXE}
