.c.o:
	gcc -ggdb -O -I../inc -I../../inc -c $*.c

O =  aliGlue.o

$(HOME)/bin/$(MACHTYPE)/aliGlue: $(O)  
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/aliGlue $O ../../lib/$(MACHTYPE)/jkweb.a

strip:
	strip $(HOME)/bin/$(MACHTYPE)/aliGlue
