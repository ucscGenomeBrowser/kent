include ../../../inc/common.mk

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkDupe.o

checkDupe: $O $(MYLIBS)
	gcc -ggdb -o checkDupe $O $(MYLIBS) $L

lib:
	cd ../lib; make
	cd ../../lib; make
