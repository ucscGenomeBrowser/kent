.c.o:
	gcc -O -I../inc -I../../inc -c $*.c

O =  sortFilt.o 

$(HOME)/bin/$(MACHTYPE)/sortFilt: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/sortFilt $O ../../lib/$(MACHTYPE)/jkweb.a


