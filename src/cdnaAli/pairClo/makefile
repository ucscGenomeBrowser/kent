include ../../inc/common.mk


O =  pairClo.o 

${BINDIR}/pairClo: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pairClo $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
