.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

O =  pairClo.o 

$(HOME)/bin/$(MACHTYPE)/pairClo: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/pairClo $O ../../lib/$(MACHTYPE)/jkweb.a


