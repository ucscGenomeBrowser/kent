include ../../inc/common.mk


O = fixCr.o 

fixCr: $O
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/fixCr $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} $(HOME)/bin/$(MACHTYPE)/fixCr${EXE}

