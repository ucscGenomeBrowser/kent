.c.o:
	gcc -O -I../../inc -c $*.c

O = indexRa.o 

indexRa: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexRa

