include ../../inc/common.mk


O = makegdf.o 

makegdf: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/makegdf
