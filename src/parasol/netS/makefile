.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = netS.o

netS: $(O)
	gcc -o netS $O ../../lib/$(MACHTYPE)/jkweb.a

