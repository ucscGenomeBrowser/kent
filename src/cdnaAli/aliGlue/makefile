include ../../inc/common.mk

O =  aliGlue.o

$(HOME)/bin/$(MACHTYPE)/aliGlue: $(O)  
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/aliGlue $O ../../lib/$(MACHTYPE)/jkweb.a ../../lib/$(MACHTYPE)/jkOwnLib.a -lm

strip:
	strip $(HOME)/bin/$(MACHTYPE)/aliGlue
