include ../../inc/common.mk


O = countChars.o 

countChars: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/countChars $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/countChars${EXE}

