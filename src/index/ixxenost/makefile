.c.o:
	gcc -I../../inc -c $*.c

O = ixxenost.o 

ixxenost: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/ixxenost
