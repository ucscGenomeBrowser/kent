.c.o:
	gcc -ggdb -I../../inc -c $*.c

O = blastStats.o

blastStats: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/blastStats $O ../../lib/$(MACHTYPE)/jkweb.a
