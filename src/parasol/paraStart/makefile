.c.o:
	gcc -O -ggdb -I../inc -I../../inc -c $*.c

O = paraStart.o

paraStart: $(O)
	gcc -o paraStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a

