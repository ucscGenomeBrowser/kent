include ../../inc/common.mk


O =  exonAli.o

MYLIBS=../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm

exonAli: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/exonAli $O ${MYLIBS} ${L}

lib: 
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
