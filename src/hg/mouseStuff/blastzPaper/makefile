include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastzPaper.o

blastzPaper: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/blastzPaper $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/blastzPaper${EXE}
