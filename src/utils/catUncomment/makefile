include ../../inc/common.mk


O = catUncomment.o 

catUncomment: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/catUncomment $O ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} $(HOME)/bin/$(MACHTYPE)/catUncomment${EXE}


