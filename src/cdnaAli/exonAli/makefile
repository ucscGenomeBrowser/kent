.c.o:
	gcc -O -Wimplicit -I../../inc -c $*.c

O =  exonAli.o

exonAli: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/exonAli $O ../../lib/$(MACHTYPE)/jkweb.a

lib: 
	cd ../../lib;make; cd ../cdnaAli/exonAli

