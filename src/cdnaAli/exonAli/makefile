include ../../inc/common.mk


O =  exonAli.o

exonAli: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/exonAli $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm

lib: 
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
