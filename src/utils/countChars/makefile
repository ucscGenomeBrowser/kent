include ../../inc/common.mk


O = countChars.o 

countChars: $O
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/countChars $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/countChars${EXE}

