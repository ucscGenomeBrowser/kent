.c.o:
	gcc -I../../inc -c $*.c

O = fatont4.o 

fatont4: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/fatont4
