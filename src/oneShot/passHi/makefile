.c.o:
	gcc -O -I../inc -I../../inc -c $*.c

O =  passHi.o

$(HOME)/bin/$(MACHTYPE)/passHi: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/passHi $O ../../lib/$(MACHTYPE)/jkweb.a


