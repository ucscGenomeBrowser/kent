include ../../inc/common.mk


O = wordLine.o 

wordLine: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/wordLine $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/wordLine${EXE}

