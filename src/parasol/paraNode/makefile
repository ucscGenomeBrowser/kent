include ../../inc/makefile.h


O = paraNode.o

paraNode: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraNode $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraNode

