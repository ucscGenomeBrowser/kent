include ../../inc/makefile.h


O = indexfa.o 

indexfa: $(O) lib
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexfa

lib:
	cd ../../lib;make; cd ../index/indexfa

