.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  introns.o ../lib/cdnaAli.o

$(HOME)/bin/$(MACHTYPE)/introns: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/introns $O ../../lib/$(MACHTYPE)/jkweb.a


