include ../../inc/makefile.h


O = makepgo.o 

makepgo: $(O) 
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/makepgo

lib:
	cd ../../lib;make; cd ../index/makepgo

