include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = tickToDate.o

tickToDate: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/tickToDate $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/tickToDate${EXE}
