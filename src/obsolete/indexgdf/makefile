include ../../inc/common.mk


O = indexgdf.o 

indexgdf: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexgdf
