.c.o:
	gcc -I../../inc -c $*.c

O = ixali.o 

ixali: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/ixali
