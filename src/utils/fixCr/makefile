include ../../inc/common.mk


O = fixCr.o 

fixCr: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/fixCr $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/fixCr${EXE}

