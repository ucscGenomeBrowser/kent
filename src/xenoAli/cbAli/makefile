include ../../inc/common.mk


O = cbAli.o 

cbAli: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/cbAli
