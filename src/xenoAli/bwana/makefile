include ../../inc/common.mk


O = bwana.o 

bwana: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/bwana
