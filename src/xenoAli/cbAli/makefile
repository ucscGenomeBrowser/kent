.c.o:
	gcc -I../../inc -c $*.c

O = cbAli.o 

cbAli: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/cbAli
