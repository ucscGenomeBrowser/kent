.c.o:
	gcc -O -Wimplicit -I../../inc -c $*.c

O =  exonAli.o

exonAli: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/exonAli $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm

lib: 
	cd ../../lib;make; cd ../cdnaAli/exonAli

