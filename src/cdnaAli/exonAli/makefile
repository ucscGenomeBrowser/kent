.c.o:
	gcc -o -Wimplicit -I../../inc -c $*.c

O =  exonAli.o

exonAli: $(O)  
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/exonAli

lib: 
	cd ../../lib;make; cd ../cdnaAli/exonAli

