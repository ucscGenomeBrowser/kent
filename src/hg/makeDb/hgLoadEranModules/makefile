include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadEranModules.o esMotif.o

hgLoadEranModules: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgLoadEranModules $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgLoadEranModules
