include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ollySlow.o

ollySlow: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ollySlow $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ollySlow
