.c.o:
	gcc -I../../inc -c $*.c

O = ixword3.o 

ixword3: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/ixword3
