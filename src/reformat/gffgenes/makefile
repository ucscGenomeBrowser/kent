.c.o:
	gcc -I../../inc -c $*.c

O = gffgenes.o 

$(HOME)/bin/$(MACHTYPE)/gffgenes: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/gffgenes $O ../../lib/$(MACHTYPE)/jkweb.a
