.c.o:
	gcc -I../../inc -c $*.c

O = invert.o 

invert: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/invert $O ../../lib/$(MACHTYPE)/jkweb.a
