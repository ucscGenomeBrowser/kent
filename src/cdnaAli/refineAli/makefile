.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  refineAli.o

$(HOME)/bin/$(MACHTYPE)/refiAli: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/refiAli $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm


