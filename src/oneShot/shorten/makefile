.c.o:
	gcc -I../../inc -c $*.c

O = shorten.o 

shorten: $(O)
	gcc $O -o $(HOME)/bin/$(MACHTYPE)/shorten ../../lib/$(MACHTYPE)/jkweb.a
