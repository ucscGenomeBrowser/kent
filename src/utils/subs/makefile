.c.o:
	gcc -I../../inc -c $*.c

O = subs.o 

subs: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/subs $O ../../lib/$(MACHTYPE)/jkweb.a 

