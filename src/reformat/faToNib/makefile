.c.o:
	gcc -I../../inc -c $*.c

O = faToNib.o 

faToNib: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/faToNib
