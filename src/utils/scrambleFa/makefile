.c.o:
	gcc -I../../inc -c $*.c

O = scrambleFa.o 

scrambleFa: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/scrambleFa $O ../../lib/$(MACHTYPE)/jkweb.a 

