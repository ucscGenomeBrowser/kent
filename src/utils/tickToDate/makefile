include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tickToDate.o

tickToDate: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/tickToDate $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/tickToDate
