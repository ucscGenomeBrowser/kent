.c.o:
	gcc -I../../inc -c $*.c

O = sources.o 

$(HOME)/bin/$(MACHTYPE)/sources: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/sources $O ../../lib/$(MACHTYPE)/jkweb.a
