include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkQa.o 

checkQa: $O $(MYLIBS)
	gcc -ggdb -o checkQa $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
