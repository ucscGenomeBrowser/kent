.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = netC.o

netC: $(O)
	gcc -o netC $O ../../lib/$(MACHTYPE)/jkweb.a

