include ../../inc/makefile.h


O = indexgdf.o 

indexgdf: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexgdf
