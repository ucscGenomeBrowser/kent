include ../../inc/common.mk


O = subChar.o 

subChar: $O
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/subChar $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/subChar${EXE}

