include ../../inc/common.mk


O = findCdna.o 

findCdna: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/findCdna
