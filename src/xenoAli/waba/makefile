.c.o:
	gcc -ggdb -O -I../../inc -c $*.c

O = waba.o wabaCrude.o

waba: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out $(HOME)/bin/$(MACHTYPE)/waba
