.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  patCount.o

$(HOME)/bin/$(MACHTYPE)/patCount: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/patCount $O ../../lib/$(MACHTYPE)/jkweb.a


