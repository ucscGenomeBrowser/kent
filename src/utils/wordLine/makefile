include ../../inc/makefile.h


O = wordLine.o 

wordLine: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/wordLine $O ../../lib/$(MACHTYPE)/jkweb.a 
	strip $(HOME)/bin/$(MACHTYPE)/wordLine

