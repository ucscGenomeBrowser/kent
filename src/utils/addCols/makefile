.c.o:
	gcc -Wimplicit -I../../inc -c $*.c

O = addCols.o 

addCols: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/addCols $O ../../lib/$(MACHTYPE)/jkweb.a 

