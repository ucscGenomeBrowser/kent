.c.o:
	gcc -O -ggdb -I../inc -I../../inc -c $*.c

O = paraStat.o

paraStat: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraStat $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a

