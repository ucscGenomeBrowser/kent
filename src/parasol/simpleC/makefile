.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = simpleC.o

simpleC: $(O)
	gcc -o simpleC $O ../../lib/$(MACHTYPE)/jkweb.a

