.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  cdnaOff.o 

cdnaOff: $(O)  
	cd ../lib;make
	gcc -o $(HOME)/bin/$(MACHTYPE)/cdnaOff $O ../lib/cdnaAli.o ../../lib/$(MACHTYPE)/jkweb.a


