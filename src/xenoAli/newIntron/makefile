.c.o:
	gcc -ggdb -O -I../../inc -c $*.c

O = newIntron.o 

newIntron: $(O)
	gcc -o newIntron $O ../../lib/$(MACHTYPE)/jkweb.a
