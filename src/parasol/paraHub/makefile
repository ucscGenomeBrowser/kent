.c.o:
	gcc -O -ggdb -I../inc -I../../inc -c $*.c

O = paraHub.o

paraHub: $(O)
	gcc -o paraHub $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a

