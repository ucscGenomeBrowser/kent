include ../../inc/makefile.h


O = makegdf.o 

makegdf: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/makegdf
