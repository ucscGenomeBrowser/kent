.c.o:
	gcc -I../../inc -c $*.c

O = wordLine.o 

wordLine: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/wordLine $O ../../lib/$(MACHTYPE)/jkweb.a 

