include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = calc.o

calc: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/calc $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/calc${EXE}
