.c.o:
	gcc -I../../inc -c $*.c

O = ixword1.o 

ixword1: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/ixword1
