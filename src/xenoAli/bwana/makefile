.c.o:
	gcc -O -I../../inc -c $*.c

O = bwana.o 

bwana: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/bwana
