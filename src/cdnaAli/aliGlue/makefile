include ../../inc/makefile.h

O =  aliGlue.o

$(HOME)/bin/$(MACHTYPE)/aliGlue: $(O)  
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/aliGlue $O ../../lib/$(MACHTYPE)/jkweb.a

strip:
	strip $(HOME)/bin/$(MACHTYPE)/aliGlue
