.c.o:
	gcc -I../../inc -c $*.c

O = makegdf.o 

makegdf: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/makegdf
