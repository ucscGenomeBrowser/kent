.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooNtInput.o

ooNtInput: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooNtInput $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ooNtInput


lib:
	cd ../lib; make
	cd ../../lib; make
