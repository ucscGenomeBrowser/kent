.c.o:
	gcc -Wimplicit -I../../inc -c $*.c

O = aveCols.o 

aveCols: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/aveCols $O ../../lib/$(MACHTYPE)/jkweb.a 

