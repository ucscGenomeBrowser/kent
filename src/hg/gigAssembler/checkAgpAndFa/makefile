.c.o:
	gcc -ggdb -g -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkAgpAndFa.o

checkAgpAndFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa $O $(MYLIBS) $L
	cp $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa .

clean:
	rm *.o $(HOME)/bin/$(MACHTYPE)/checkAgpAndFa ./checkAgpAndFa
