.c.o:
	gcc -O -I../../inc -c $*.c

O = bwana.o 

bwana: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/bwana
